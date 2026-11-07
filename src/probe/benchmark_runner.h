#pragma once

#include "egl_renderer.h"
#include "cuda_compute.h"

#include <vector>
#include <string>
#include <chrono>

struct BenchmarkConfig {
    int iterations = 500;
    int warmup = 50;
    int target_device = 0;
    int render_width = 128;
    int render_height = 128;
    size_t cuda_elements = 262144;
    int cuda_intensity = 100;
};

struct TimingStats {
    std::string name;
    int iterations;
    double total_wall_ms;
    double mean_us;
    double stddev_us;
    double min_us;
    double max_us;
    double p50_us;
    double p95_us;
    double p99_us;
    double throughput_ops_sec;
};

class BenchmarkRunner {
public:
    BenchmarkRunner(const BenchmarkConfig& config);
    ~BenchmarkRunner();

    bool setup();
    void runAll();
    void printReport() const;

private:
    TimingStats computeStats(const std::string& name, const std::vector<double>& latencies_us, double wall_time_ms) const;

    TimingStats runSoloRender();
    TimingStats runSoloCompute();
    TimingStats runSerialInterleaved();
    TimingStats runConcurrentMultiThreaded();

    BenchmarkConfig config_;
    EGLRenderer renderer_;
    CudaCompute compute_;

    TimingStats stats_solo_render_;
    TimingStats stats_solo_compute_;
    TimingStats stats_serial_;
    TimingStats stats_concurrent_;
    bool interop_supported_;
};
