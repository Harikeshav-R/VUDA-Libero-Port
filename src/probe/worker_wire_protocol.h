#pragma once

#include <cstdint>
#include <cstddef>
#include "measurement_window.h"
#include "operation_trace.h"
#include <optional>

#pragma pack(push, 1)

// Fixed-size POD struct for reliable, allocation-free, overflow-proof IPC
struct WorkerWireReport {
    char role[16]; // "render" or "compute"
    int32_t pid;
    int32_t tid;
    char physical_uuid[64];
    char pci_bus_id[64];

    uint32_t num_affinity_cpus;
    int32_t cpu_affinity[32]; // Max 32 affinity CPUs recorded

    uint64_t ready_ns;
    uint64_t warmup_start_ns;
    uint64_t warmup_end_ns;
    uint64_t warmup_completed;

    uint64_t first_submit_ns;
    uint64_t last_submit_ns;
    uint64_t last_completion_ns;

    uint64_t completed;
    uint64_t late;
    uint64_t latency_samples;

    double p50_us;
    double p95_us;
    double p99_us;
    double throughput_ops_sec;

    uint32_t num_late_samples;
    double late_latencies_us[16]; // Bounded late operations

    uint64_t start_skew_ns;
    uint64_t drain_ns;
    uint64_t validation_ns;
    uint64_t final_tag;
    uint8_t validation_passed; // 1 or 0
    char error_text[256];
};

struct IpcCommand {
    enum Type : uint32_t {
        CMD_NONE = 0,
        CMD_READY = 1,
        CMD_START_WINDOW = 2,
        CMD_START_VALIDATION = 3,
        CMD_STEP_RENDER = 4,
        CMD_RENDER_DONE = 5,
        CMD_STEP_COMPUTE = 6,
        CMD_COMPUTE_DONE = 7,
        CMD_TERMINATE = 8,
        CMD_ERROR = 9
    } type = CMD_NONE;

    uint64_t timestamp_ns = 0;
    uint64_t start_ns = 0;
    uint64_t deadline_ns = 0;
    uint64_t iteration = 0;
    char error_text[256]{};
};

#pragma pack(pop)

inline std::optional<OperationRecord> completedSerialPair(
        const MeasurementWindow& window, const IpcCommand& render,
        const IpcCommand& compute, uint64_t expected_iteration) {
    if (render.type != IpcCommand::CMD_RENDER_DONE ||
        compute.type != IpcCommand::CMD_COMPUTE_DONE) {
        throw std::runtime_error("Serial pair received an unexpected worker response");
    }
    for (const auto* step : {&render, &compute}) {
        if (step->start_ns == 0 && step->iteration + 1 == expected_iteration &&
            step->timestamp_ns >= window.deadline_ns) {
            return std::nullopt;
        }
        if (step->iteration != expected_iteration || !window.canSubmit(step->start_ns) ||
            step->timestamp_ns < step->start_ns) {
            throw std::runtime_error("Serial pair worker identity or timestamp mismatch");
        }
    }
    if (compute.start_ns < render.timestamp_ns) {
        throw std::runtime_error("Serial pair compute started before render completion");
    }
    return OperationRecord{render.start_ns, compute.timestamp_ns};
}
