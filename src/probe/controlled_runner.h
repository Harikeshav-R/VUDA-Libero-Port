#pragma once

#include "egl_renderer.h"
#include "cuda_compute.h"
#include "measurement_window.h"

#include <string>
#include <vector>
#include <cstdint>

struct TraceMetadata {
    std::string path;
    uint64_t count = 0;
    std::string format = "exp9ts01_le";
};

struct ControlledConfig {
    std::string mode;            // "solo-render", "solo-compute", "serial", "concurrent", "historical"
    std::string topology;        // "threads", "processes"
    double duration_seconds = 15.0;
    double warmup_seconds = 5.0;
    bool validation = false;     // strict per-iteration validation path
    int iterations = 0;          // validation finite count or historical iterations
    int warmup_iterations = 0;   // for historical mode
    std::string report_json_path;

    size_t cuda_elements = 262144;
    int cuda_intensity = 100;
    int render_width = 128;
    int render_height = 128;
    int target_device = 0;

    int render_cpu = -1;
    int compute_cpu = -1;

    // Internal worker role when launched as independent process worker
    std::string internal_worker_role; // "render" or "compute" or empty
    std::string trace_file_path;      // explicit trace file path passed to child
    int ipc_sync_fd_in = -1;
    int ipc_sync_fd_out = -1;
};

struct WorkerReport {
    std::string role; // "render" or "compute"
    int pid = 0;
    int tid = 0;
    std::vector<int> cpu_affinity;
    uint64_t ready_ns = 0;
    uint64_t warmup_start_ns = 0;
    uint64_t warmup_end_ns = 0;
    uint64_t warmup_completed = 0;
    uint64_t first_submit_ns = 0;
    uint64_t last_submit_ns = 0;
    uint64_t last_completion_ns = 0;
    uint64_t completed = 0;
    uint64_t late = 0;
    uint64_t latency_samples = 0;
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
    double throughput_ops_sec = 0.0;
    std::vector<double> late_latencies_us;
    uint64_t start_skew_ns = 0;
    uint64_t drain_ns = 0;
    uint64_t validation_ns = 0;
    uint64_t final_tag = 0;
    bool validation_passed = false;
    std::string failure_error;
    std::string physical_uuid;
    std::string pci_bus_id;

    TraceMetadata trace;
};

struct SerialPairsReport {
    uint64_t completed = 0;
    uint64_t late = 0;
    uint64_t latency_samples = 0;
    double p50_us = 0.0;
    double p95_us = 0.0;
    double p99_us = 0.0;
    std::vector<double> late_latencies_us;
    TraceMetadata trace;
};

struct RunReport {
    int schema_version = 1;
    bool valid = false;
    std::string mode;
    std::string topology;
    std::string validation_strength; // "per_iteration" or "final_tag_only"
    ControlledConfig config;
    std::string clock = "CLOCK_MONOTONIC";
    uint64_t start_ns = 0;
    uint64_t deadline_ns = 0;
    uint64_t observed_end_ns = 0;
    std::vector<WorkerReport> workers;
    bool has_pairs = false;
    SerialPairsReport pairs;
};

class ControlledRunner {
public:
    explicit ControlledRunner(const ControlledConfig& config);
    ~ControlledRunner();

    // Runs according to mode and topology. Throws on failure or returns false.
    bool run();

    // Internal subprocess worker entrypoint
    static int runInternalWorker(const ControlledConfig& config);

private:
    bool runThreadTimed();
    bool runThreadValidation();

    bool runProcessTimed();
    bool runProcessValidation();

    void writeReportJson(const RunReport& report) const;

    ControlledConfig config_;
    std::string binary_path_;
};
