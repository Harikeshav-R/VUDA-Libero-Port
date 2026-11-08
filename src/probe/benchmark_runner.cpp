#include "benchmark_runner.h"
#include "nvtx_wrapper.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <future>
#include <atomic>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
void checkCuda(cudaError_t err, const char* msg) {
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string(msg) + ": " + cudaGetErrorString(err));
    }
}

struct NvtxRangeGuard {
    NvtxScope scope;
    explicit NvtxRangeGuard(const char* name) : scope(name) {}
    void pop() { scope.pop(); }
};
} // anonymous namespace

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
    double wall_ms = 0.0;
    {
        NvtxRangeGuard nvtx("Solo_EGL_Render");
        auto start_wall = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < config_.iterations; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            renderer_.renderFrame();
            auto t1 = std::chrono::high_resolution_clock::now();
            latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        }

        auto end_wall = std::chrono::high_resolution_clock::now();
        wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();
    }

    // Verify rendered output at the end of baseline run
    if (!renderer_.readPixelsAndVerify()) {
        throw std::runtime_error("Solo EGL Render verification failed");
    }
    std::cout << "[Verification] Solo EGL Render: PASSED" << std::endl;

    return computeStats("Solo EGL Render", latencies_us, wall_ms);
}

TimingStats BenchmarkRunner::runSoloCompute() {
    std::cout << "\n>>> [2/4] Running Solo CUDA Compute Baseline ("
              << config_.iterations << " iterations, " << config_.warmup << " warmup)..." << std::endl;

    cudaStream_t stream = nullptr;
    checkCuda(cudaStreamCreate(&stream), "Solo compute cudaStreamCreate failed");

    struct StreamCleanup {
        cudaStream_t s;
        ~StreamCleanup() {
            if (s) {
                cudaStreamDestroy(s);
            }
        }
    } cleanup{stream};

    // Warmup
     for (int i = 0; i < config_.warmup; ++i) {
         compute_.launchCompute(stream);
         compute_.synchronize(stream);
     }

     std::vector<double> latencies_us(config_.iterations);
    double wall_ms = 0.0;
    {
        NvtxRangeGuard nvtx("Solo_CUDA_Compute");
        auto start_wall = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < config_.iterations; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            compute_.launchCompute(stream);
            compute_.synchronize(stream);
            auto t1 = std::chrono::high_resolution_clock::now();
            latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
        }

        auto end_wall = std::chrono::high_resolution_clock::now();
        wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();
    }

    cleanup.s = nullptr;
    checkCuda(cudaStreamDestroy(stream), "Solo compute cudaStreamDestroy failed");

    // Verify compute output
    if (!compute_.verifyOutput()) {
        throw std::runtime_error("Solo CUDA Compute verification failed");
    }
    std::cout << "[Verification] Solo CUDA Compute: PASSED" << std::endl;

    return computeStats("Solo CUDA Compute", latencies_us, wall_ms);
}

TimingStats BenchmarkRunner::runSerialInterleaved() {
    std::cout << "\n>>> [3/4] Running Serial Interleaved (Render -> Compute) Baseline ("
              << config_.iterations << " iterations)..." << std::endl;

    cudaStream_t stream = nullptr;
    checkCuda(cudaStreamCreate(&stream), "Serial interleaved cudaStreamCreate failed");

    struct StreamCleanup {
        cudaStream_t s;
        ~StreamCleanup() {
            if (s) {
                cudaStreamDestroy(s);
            }
        }
    } cleanup{stream};

    // Warmup
    for (int i = 0; i < config_.warmup; ++i) {
        renderer_.renderFrame();
        compute_.launchCompute(stream);
        compute_.synchronize(stream);
    }

    std::vector<double> latencies_us(config_.iterations);
    double wall_ms = 0.0;
    {
        NvtxRangeGuard nvtx("Serial_Interleaved");
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
        wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();
    }

    cleanup.s = nullptr;
    checkCuda(cudaStreamDestroy(stream), "Serial interleaved cudaStreamDestroy failed");

    // Verify rendered and compute outputs post-benchmark
    if (!renderer_.readPixelsAndVerify()) {
        throw std::runtime_error("Serial Interleaved EGL verification failed");
    }
    std::cout << "[Verification] Serial Interleaved EGL: PASSED" << std::endl;

    if (!compute_.verifyOutput()) {
        throw std::runtime_error("Serial Interleaved CUDA verification failed");
    }
    std::cout << "[Verification] Serial Interleaved CUDA: PASSED" << std::endl;

    return computeStats("Serial Interleaved", latencies_us, wall_ms);
}

std::pair<TimingStats, TimingStats> BenchmarkRunner::runConcurrentMultiThreaded() {
    std::cout << "\n>>> [4/4] Running Concurrent Multi-Threaded Probe (Thread 1: Render, Thread 2: Compute) ("
              << config_.iterations << " iterations)..." << std::endl;

    // Release context from main thread so Worker Thread can make it current
    if (!eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        throw std::runtime_error("Failed to unbind EGL context from main thread");
    }

    std::atomic<bool> start_flag(false);
    std::atomic<bool> abort_flag(false);
    std::atomic<bool> ready_render(false);
    std::atomic<bool> ready_compute(false);

    std::exception_ptr render_exception = nullptr;
    std::exception_ptr compute_exception = nullptr;

    std::vector<double> render_latencies_us(config_.iterations);
    std::vector<double> compute_latencies_us(config_.iterations);

    // Worker 1: Render thread
    auto render_worker = [&]() {
        bool nvtx_pushed = false;
        bool context_bound = false;
        try {
            nvtx_push("Worker_EGL_Render");
            nvtx_pushed = true;

            // Set client API to OpenGL for this thread
            if (!eglBindAPI(EGL_OPENGL_API)) {
                throw std::runtime_error("Render worker failed to bind EGL_OPENGL_API");
            }

            // Bind EGL context to this thread
            if (!eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, renderer_.getContext())) {
                throw std::runtime_error("Render worker failed to bind EGL context");
            }
            context_bound = true;

            ready_render.store(true, std::memory_order_release);

            while (!start_flag.load(std::memory_order_acquire) && !abort_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            if (!abort_flag.load(std::memory_order_acquire)) {
                for (int i = 0; i < config_.iterations; ++i) {
                    if (abort_flag.load(std::memory_order_acquire)) {
                        break;
                    }
                    auto t0 = std::chrono::high_resolution_clock::now();
                    renderer_.renderFrame();
                    auto t1 = std::chrono::high_resolution_clock::now();
                    render_latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
                }
            }
        } catch (...) {
            render_exception = std::current_exception();
            abort_flag.store(true, std::memory_order_release);
            ready_render.store(true, std::memory_order_release);
        }

        std::exception_ptr cleanup_err = nullptr;
        if (context_bound) {
            if (!eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
                cleanup_err = std::make_exception_ptr(std::runtime_error("Render worker failed to unbind EGL context"));
            }
        }
        if (nvtx_pushed) {
            nvtx_pop();
        }
        if (cleanup_err && !render_exception) {
            render_exception = cleanup_err;
        }
    };

    // Worker 2: Compute thread
    auto compute_worker = [&]() {
        bool nvtx_pushed = false;
        cudaStream_t stream = nullptr;
        try {
            nvtx_push("Worker_CUDA_Compute");
            nvtx_pushed = true;

            cudaError_t err = cudaSetDevice(config_.target_device);
            if (err != cudaSuccess) {
                throw std::runtime_error(std::string("CUDA worker cudaSetDevice failed: ") + cudaGetErrorString(err));
            }

            err = cudaStreamCreate(&stream);
            if (err != cudaSuccess) {
                throw std::runtime_error(std::string("CUDA worker cudaStreamCreate failed: ") + cudaGetErrorString(err));
            }

            ready_compute.store(true, std::memory_order_release);

            while (!start_flag.load(std::memory_order_acquire) && !abort_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            if (!abort_flag.load(std::memory_order_acquire)) {
                for (int i = 0; i < config_.iterations; ++i) {
                    if (abort_flag.load(std::memory_order_acquire)) {
                        break;
                    }
                    auto t0 = std::chrono::high_resolution_clock::now();
                    compute_.launchCompute(stream);
                    compute_.synchronize(stream);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    compute_latencies_us[i] = std::chrono::duration<double, std::micro>(t1 - t0).count();
                }
            }
        } catch (...) {
            compute_exception = std::current_exception();
            abort_flag.store(true, std::memory_order_release);
            ready_compute.store(true, std::memory_order_release);
        }

        std::exception_ptr cleanup_err = nullptr;
        if (stream) {
            cudaError_t err = cudaStreamDestroy(stream);
            if (err != cudaSuccess) {
                cleanup_err = std::make_exception_ptr(std::runtime_error(
                    std::string("CUDA worker cudaStreamDestroy failed: ") + cudaGetErrorString(err)));
            }
        }
        if (nvtx_pushed) {
            nvtx_pop();
        }
        if (cleanup_err && !compute_exception) {
            compute_exception = cleanup_err;
        }
    };

    std::thread t1;
    std::thread t2;
    try {
        t1 = std::thread(render_worker);
        t2 = std::thread(compute_worker);
    } catch (...) {
        abort_flag.store(true, std::memory_order_release);
        if (t1.joinable()) {
            t1.join();
        }
        if (t2.joinable()) {
            t2.join();
        }
        eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, renderer_.getContext());
        throw;
    }

    // Wait until both workers are ready (or published failure)
    while (!ready_render.load(std::memory_order_acquire) || !ready_compute.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    double wall_ms = 0.0;
    if (render_exception || compute_exception) {
        abort_flag.store(true, std::memory_order_release);
        t1.join();
        t2.join();

        // Restore main context
        if (!eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, renderer_.getContext())) {
            std::cerr << "[BenchmarkRunner] Failed to rebind EGL context to main thread" << std::endl;
        }

        if (render_exception) {
            std::rethrow_exception(render_exception);
        }
        if (compute_exception) {
            std::rethrow_exception(compute_exception);
        }
    } else {
        nvtx_push("Concurrent_Multi_Threaded");
        auto start_wall = std::chrono::high_resolution_clock::now();
        start_flag.store(true, std::memory_order_release);

        t1.join();
        t2.join();
        auto end_wall = std::chrono::high_resolution_clock::now();
        nvtx_pop();
        wall_ms = std::chrono::duration<double, std::milli>(end_wall - start_wall).count();

        // Rebind context to main thread
        if (!eglMakeCurrent(renderer_.getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, renderer_.getContext())) {
            std::string err_msg = "Failed to rebind EGL context to main thread";
            if (render_exception) {
                std::cerr << "[BenchmarkRunner] " << err_msg << std::endl;
                std::rethrow_exception(render_exception);
            } else if (compute_exception) {
                std::cerr << "[BenchmarkRunner] " << err_msg << std::endl;
                std::rethrow_exception(compute_exception);
            } else {
                throw std::runtime_error(err_msg);
            }
        }

        if (render_exception) {
            std::rethrow_exception(render_exception);
        }
        if (compute_exception) {
            std::rethrow_exception(compute_exception);
        }
    }

    // Verify rendered output post-benchmark
    if (!renderer_.readPixelsAndVerify()) {
        throw std::runtime_error("Concurrent EGL verification failed");
    }
    std::cout << "[Verification] Concurrent EGL: PASSED" << std::endl;

    // Verify compute output post-benchmark
    if (!compute_.verifyOutput()) {
        throw std::runtime_error("Concurrent CUDA verification failed");
    }
    std::cout << "[Verification] Concurrent CUDA: PASSED" << std::endl;

    TimingStats render_stats = computeStats("Concurrent EGL Render", render_latencies_us, wall_ms);
    TimingStats compute_stats = computeStats("Concurrent CUDA Compute", compute_latencies_us, wall_ms);

    return {render_stats, compute_stats};
}

void BenchmarkRunner::runAll() {
    stats_solo_render_ = runSoloRender();
    stats_solo_compute_ = runSoloCompute();
    stats_serial_ = runSerialInterleaved();
    auto concurrent_stats = runConcurrentMultiThreaded();
    stats_concurrent_render_ = concurrent_stats.first;
    stats_concurrent_compute_ = concurrent_stats.second;
    printReport();
}

void BenchmarkRunner::printReport() const {
    double solo_render_ms = stats_solo_render_.total_wall_ms;
    double solo_compute_ms = stats_solo_compute_.total_wall_ms;
    double serial_ms = stats_serial_.total_wall_ms;
    double concurrent_ms = stats_concurrent_render_.total_wall_ms;

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
              << std::setw(14) << "| p95 (us)"
              << std::setw(14) << "| p99 (us)"
              << std::setw(25) << "| Throughput"
              << " |" << std::endl;
    std::cout << "|--------------------------|-------------|-------------|-------------|-------------|-------------|-------------------------|" << std::endl;

    auto print_row = [](const TimingStats& s, const std::string& unit) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << s.throughput_ops_sec << " " << unit;
        std::cout << "| " << std::left << std::setw(24) << s.name
                  << " | " << std::right << std::setw(11) << std::fixed << std::setprecision(2) << s.total_wall_ms
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.mean_us
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.p50_us
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.p95_us
                  << " | " << std::setw(11) << std::fixed << std::setprecision(1) << s.p99_us
                  << " | " << std::right << std::setw(23) << ss.str()
                  << " |" << std::endl;
    };

    print_row(stats_solo_render_, "frame/s");
    print_row(stats_solo_compute_, "compute iterations/s");
    print_row(stats_serial_, "pairs/s");
    print_row(stats_concurrent_render_, "frame/s");
    print_row(stats_concurrent_compute_, "compute iterations/s");

    std::cout << "|----------------------------------------------------------------------------------------------------------------------|" << std::endl;

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
    std::cout << "  -> INCONCLUSIVE: Timing ratios and throughput metrics alone cannot confirm" << std::endl;
    std::cout << "     true spatial sharing or driver time-slicing (TSG) serialization." << std::endl;
    std::cout << "     Hardware execution overlap requires GPU hardware trace validation (e.g. Nsight Systems)." << std::endl;
    std::cout << "==========================================================================================\n" << std::endl;
}
