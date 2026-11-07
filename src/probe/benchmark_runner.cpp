#include "benchmark_runner.h"
#include <nvtx3/nvToolsExt.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <future>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <cmath>

BenchmarkRunner::BenchmarkRunner(const BenchmarkConfig& config)
    : config_(config),
      renderer_(config.render_width, config.render_height),
      compute_(config.cuda_elements, config.cuda_intensity),
      interop_supported_(false) {}

BenchmarkRunner::~BenchmarkRunner() {}

bool BenchmarkRunner::setup() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "  EXP-002: Native EGL + CUDA Spatial Sharing Probe Setup" << std::endl;
    std::cout << "========================================================" << std::endl;

    if (!compute_.initialize(config_.target_device)) {
        std::cerr << "[BenchmarkRunner] Failed to initialize CUDA" << std::endl;
        return false;
    }

    if (!renderer_.initialize(config_.target_device)) {
        std::cerr << "[BenchmarkRunner] Failed to initialize EGL Renderer" << std::endl;
        return false;
    }

    // Verify rendering correctness before benchmarking
    renderer_.renderFrame();
    if (!renderer_.readPixelsAndVerify()) {
        std::cerr << "[BenchmarkRunner] Initial render verification failed!" << std::endl;
        return false;
    }
    std::cout << "[BenchmarkRunner] Initial EGL render verification: PASSED" << std::endl;

    // Verify CUDA compute correctness before benchmarking
    compute_.launchCompute();
    compute_.synchronize();
    if (!compute_.verifyOutput()) {
        std::cerr << "[BenchmarkRunner] Initial compute verification failed!" << std::endl;
        return false;
    }
    std::cout << "[BenchmarkRunner] Initial CUDA compute verification: PASSED" << std::endl;

    // Probe OpenGL-CUDA interop support
    interop_supported_ = compute_.testOpenGLInterop(renderer_.getColorRenderbuffer(),
                                                   renderer_.getWidth(),
                                                   renderer_.getHeight());
    std::cout << "[BenchmarkRunner] OpenGL-CUDA Interop Registered & Mapped: "
              << (interop_supported_ ? "YES (Device-resident handoff supported)"
                                     : "NO (Host-roundtrip fallback required)")
              << std::endl;

    return true;
}

TimingStats BenchmarkRunner::computeStats(const std::string& name,
                                          const std::vector<double>& latencies_us,
                                          double wall_time_ms) const {
    TimingStats stats;
    stats.name = name;
    stats.iterations = static_cast<int>(latencies_us.size());
    stats.total_wall_ms = wall_time_ms;

    if (latencies_us.empty()) {
        return stats;
    }

    std::vector<double> sorted = latencies_us;
    std::sort(sorted.begin(), sorted.end());

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    stats.mean_us = sum / sorted.size();

    double sq_sum = 0.0;
    for (double val : sorted) {
        sq_sum += (val - stats.mean_us) * (val - stats.mean_us);
    }
    stats.stddev_us = std::sqrt(sq_sum / sorted.size());

    stats.min_us = sorted.front();
    stats.max_us = sorted.back();

    auto percentile = [&sorted](double p) -> double {
        size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    };

    stats.p50_us = percentile(0.50);
    stats.p95_us = percentile(0.95);
    stats.p99_us = percentile(0.99);

    stats.throughput_ops_sec = (wall_time_ms > 0) ? (stats.iterations / (wall_time_ms / 1000.0)) : 0.0;

    return stats;
}

TimingStats BenchmarkRunner::runSoloRender() {
    std::cout << "\n>>> [1/4] Running Solo EGL Render Baseline ("
              << config_.iterations << " iterations, " << config_.warmup << " warmup)..." << std::endl;

    // Warmup
    for (int i = 0; i < config_.warmup; ++i) {
        renderer_.renderFrame();
    }

    std::vector<double> latencies_us(config_.iterations);
    nvtxRangePushA("Solo_EGL_Render");
    auto start_wall = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < config_.iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        renderer_.renderFrame();
        auto t1 = std::chrono::high_resolution_clock::now();
        latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    auto end_wall = std::chrono::high_resolution_clock::now();
    nvtxRangePop();
    double wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();

    // Verify rendered output at the end of baseline run
    if (!renderer_.readPixelsAndVerify()) {
        std::cerr << "  WARNING: Solo render output verification failed post-benchmark!" << std::endl;
    }

    return computeStats("Solo EGL Render", latencies_us, wall_ms);
}

TimingStats BenchmarkRunner::runSoloCompute() {
    std::cout << "\n>>> [2/4] Running Solo CUDA Compute Baseline ("
              << config_.iterations << " iterations, " << config_.warmup << " warmup)..." << std::endl;

    cudaStream_t stream = nullptr;
    cudaStreamCreate(&stream);

    // Warmup
    for (int i = 0; i < config_.warmup; ++i) {
        compute_.launchCompute(stream);
        compute_.synchronize(stream);
    }

    std::vector<double> latencies_us(config_.iterations);
    nvtxRangePushA("Solo_CUDA_Compute");
    auto start_wall = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < config_.iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        compute_.launchCompute(stream);
        compute_.synchronize(stream);
        auto t1 = std::chrono::high_resolution_clock::now();
        latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    auto end_wall = std::chrono::high_resolution_clock::now();
    nvtxRangePop();
    double wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();

    cudaStreamDestroy(stream);

    // Verify compute output
    if (!compute_.verifyOutput()) {
        std::cerr << "  WARNING: Solo compute numerical verification failed post-benchmark!" << std::endl;
    }

    return computeStats("Solo CUDA Compute", latencies_us, wall_ms);
}

TimingStats BenchmarkRunner::runSerialInterleaved() {
    std::cout << "\n>>> [3/4] Running Serial Interleaved (Render -> Compute) Baseline ("
              << config_.iterations << " iterations)..." << std::endl;

    cudaStream_t stream = nullptr;
    cudaStreamCreate(&stream);

    // Warmup
    for (int i = 0; i < config_.warmup; ++i) {
        renderer_.renderFrame();
        compute_.launchCompute(stream);
        compute_.synchronize(stream);
    }

    std::vector<double> latencies_us(config_.iterations);
    nvtxRangePushA("Serial_Interleaved");
    auto start_wall = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < config_.iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        renderer_.renderFrame();
        compute_.launchCompute(stream);
        compute_.synchronize(stream);
        auto t1 = std::chrono::high_resolution_clock::now();
        latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
    }

    auto end_wall = std::chrono::high_resolution_clock::now();
    nvtxRangePop();
    double wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();

    cudaStreamDestroy(stream);

    return computeStats("Serial Interleaved", latencies_us, wall_ms);
}

TimingStats BenchmarkRunner::runConcurrentMultiThreaded() {
    std::cout << "\n>>> [4/4] Running Concurrent Multi-Threaded Probe (Thread 1: Render, Thread 2: Compute) ("
              << config_.iterations << " iterations)..." << std::endl;

    // Release context from main thread so Worker Thread can make it current
    eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    std::atomic<bool> start_flag(false);
    std::atomic<bool> ready_render(false);
    std::atomic<bool> ready_compute(false);

    std::vector<double> render_latencies_us(config_.iterations);
    std::vector<double> compute_latencies_us(config_.iterations);

    // Worker 1: Render thread
    auto render_worker = [&]() {
        nvtxRangePushA("Worker_EGL_Render");
        // Bind EGL context to this thread
        if (!eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, renderer_.getContext())) {
            std::cerr << "  [RenderWorker] eglMakeCurrent failed in thread!" << std::endl;
            return;
        }

        ready_render.store(true);
        while (!start_flag.load()) {
            std::this_thread::yield();
        }

        for (int i = 0; i < config_.iterations; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            renderer_.renderFrame();
            auto t1 = std::chrono::high_resolution_clock::now();
            render_latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        }

        // Release context
        eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        nvtxRangePop();
    };

    // Worker 2: Compute thread
    auto compute_worker = [&]() {
        nvtxRangePushA("Worker_CUDA_Compute");
        cudaSetDevice(config_.target_device);
        cudaStream_t stream = nullptr;
        cudaStreamCreate(&stream);

        ready_compute.store(true);
        while (!start_flag.load()) {
            std::this_thread::yield();
        }

        for (int i = 0; i < config_.iterations; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            compute_.launchCompute(stream);
            compute_.synchronize(stream);
            auto t1 = std::chrono::high_resolution_clock::now();
            compute_latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        }

        cudaStreamDestroy(stream);
        nvtxRangePop();
    };

    std::thread t1(render_worker);
    std::thread t2(compute_worker);

    // Wait until both workers are ready
    while (!ready_render.load() || !ready_compute.load()) {
        std::this_thread::yield();
    }

    nvtxRangePushA("Concurrent_Multi_Threaded");
    auto start_wall = std::chrono::high_resolution_clock::now();
    start_flag.store(true);

    t1.join();
    t2.join();
    auto end_wall = std::chrono::high_resolution_clock::now();
    nvtxRangePop();
    double wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();

    // Rebind context to main thread
    eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, renderer_.getContext());

    return computeStats("Concurrent Multi-Threaded", render_latencies_us, wall_ms);
}

void BenchmarkRunner::runAll() {
    stats_solo_render_ = runSoloRender();
    stats_solo_compute_ = runSoloCompute();
    stats_serial_ = runSerialInterleaved();
    stats_concurrent_ = runConcurrentMultiThreaded();
    printReport();
}

void BenchmarkRunner::printReport() const {
    double solo_render_ms = stats_solo_render_.total_wall_ms;
    double solo_compute_ms = stats_solo_compute_.total_wall_ms;
    double serial_ms = stats_serial_.total_wall_ms;
    double concurrent_ms = stats_concurrent_.total_wall_ms;

    double theoretical_serial_sum_ms = solo_render_ms + solo_compute_ms;
    double speedup_vs_serial = (concurrent_ms > 0) ? (serial_ms / concurrent_ms) : 0.0;
    double speedup_vs_sum = (concurrent_ms > 0) ? (theoretical_serial_sum_ms / concurrent_ms) : 0.0;

    std::cout << "\n==========================================================================================" << std::endl;
    std::cout << "                      EXP-002 EMPIRICAL BENCHMARK RESULTS REPORT                          " << std::endl;
    std::cout << "==========================================================================================" << std::endl;

    std::cout << "\nHardware Specs:" << std::endl;
    std::cout << "  GPU:                  " << compute_.getSpecs().name << " (Compute Capability "
              << compute_.getSpecs().major << "." << compute_.getSpecs().minor << ")" << std::endl;
    std::cout << "  SMs / Total Memory:   " << compute_.getSpecs().sm_count << " SMs / "
              << (compute_.getSpecs().total_memory_bytes / (1024 * 1024)) << " MiB" << std::endl;
    std::cout << "  OpenGL/CUDA Interop:  " << (interop_supported_ ? "Supported" : "Unsupported") << std::endl;

    std::cout << "\nBenchmark Parameters:" << std::endl;
    std::cout << "  Iterations:           " << config_.iterations << " (+ " << config_.warmup << " warmup)" << std::endl;
    std::cout << "  Render Resolution:    " << config_.render_width << "x" << config_.render_height << " RGBA8" << std::endl;
    std::cout << "  CUDA Compute Grid:    " << config_.cuda_elements << " elements, intensity "
              << config_.cuda_intensity << std::endl;

    std::cout << "\n" << std::left
              << std::setw(26) << "| Mode"
              << std::setw(14) << "| Total (ms)"
              << std::setw(14) << "| Mean (us)"
              << std::setw(14) << "| p50 (us)"
              << std::setw(14) << "| p99 (us)"
              << std::setw(16) << "| Throughput"
              << " |" << std::endl;
    std::cout << "|--------------------------|-------------|-------------|-------------|-------------|----------------|" << std::endl;

    auto print_row = [](const TimingStats& s, const std::string& unit) {
        std::cout << "| " << std::left << std::setw(24) << s.name
                  << " | " << std::right << std::setw(11) << std::fixed << std::setprecision(2) << s.total_wall_ms
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.mean_us
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.p50_us
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.p99_us
                  << " | " << std::setw(10) << std::fixed << std::setprecision(1) << s.throughput_ops_sec << " " << unit
                  << " |" << std::endl;
    };

    print_row(stats_solo_render_, "fps");
    print_row(stats_solo_compute_, "ips");
    print_row(stats_serial_, "pairs/s");
    print_row(stats_concurrent_, "pairs/s");

    std::cout << "------------------------------------------------------------------------------------------" << std::endl;

    std::cout << "\nAnalysis & Concurrency Metrics:" << std::endl;
    std::cout << "  Theoretical Serial Sum (Solo Render + Solo Compute): "
              << std::fixed << std::setprecision(2) << theoretical_serial_sum_ms << " ms" << std::endl;
    std::cout << "  Observed Serial Interleaved:                         "
              << std::fixed << std::setprecision(2) << serial_ms << " ms" << std::endl;
    std::cout << "  Observed Concurrent Multi-Threaded:                  "
              << std::fixed << std::setprecision(2) << concurrent_ms << " ms" << std::endl;
    std::cout << "  Speedup vs. Theoretical Serial Sum:                  "
              << std::fixed << std::setprecision(3) << speedup_vs_sum << "x" << std::endl;
    std::cout << "  Speedup vs. Serial Interleaved:                      "
              << std::fixed << std::setprecision(3) << speedup_vs_serial << "x" << std::endl;

    std::cout << "\nInterpretation:" << std::endl;
    if (speedup_vs_sum >= 1.20) {
        std::cout << "  -> SIGNIFICANT CONCURRENCY: Observed speedup (" << speedup_vs_sum
                  << "x) indicates hardware overlap / spatial sharing of GPU resources." << std::endl;
    } else if (speedup_vs_sum >= 0.95 && speedup_vs_sum < 1.20) {
        std::cout << "  -> TIME-SLICED / SERIALIZED: Observed speedup (" << speedup_vs_sum
                  << "x) is close to 1.0x, indicating GPU driver serializes graphics and compute via Timeslice Groups (TSGs)." << std::endl;
    } else {
        std::cout << "  -> RESOURCE CONTENTION / SLOWDOWN: Observed speedup (" << speedup_vs_sum
                  << "x) indicates contention or overhead under concurrent submission." << std::endl;
    }
    std::cout << "==========================================================================================\n" << std::endl;
}
