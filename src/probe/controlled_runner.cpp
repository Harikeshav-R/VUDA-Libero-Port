#include "controlled_runner.h"
#include "device_identity_guard.h"
#include "nvtx_wrapper.h"
#include "worker_wire_protocol.h"
#include "operation_trace.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

#if defined(__linux__)
#include <sched.h>
#include <pthread.h>
#include <sys/syscall.h>
#endif

namespace {

void checkCudaError(cudaError_t err, const char* action) {
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string(action) + " failed: " +
                                 cudaGetErrorString(err) + " (code " +
                                 std::to_string(static_cast<int>(err)) + ")");
    }
}

int getCurrentTid() {
#if defined(__linux__) && defined(SYS_gettid)
    return static_cast<int>(syscall(SYS_gettid));
#else
    return static_cast<int>(getpid());
#endif
}

std::vector<int> getAffinityCpus() {
    std::vector<int> cpus;
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        for (int i = 0; i < CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &cpuset)) {
                cpus.push_back(i);
            }
        }
    }
#endif
    return cpus;
}

bool pinCpu(int cpu) {
    if (cpu < 0) return true;
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        std::cerr << "[ControlledRunner] Warning: failed to pin to CPU " << cpu
                  << " (" << strerror(errno) << ")" << std::endl;
        return false;
    }
    return true;
#else
    (void)cpu;
    return true;
#endif
}

struct ScopedNvtx {
    NvtxScope scope;
    explicit ScopedNvtx(const char* name) : scope(name) {}
    void pop() { scope.pop(); }
};

template <typename T>
bool writeExact(int fd, const T& data) {
    const char* ptr = reinterpret_cast<const char*>(&data);
    size_t total = sizeof(T);
    size_t written = 0;
    while (written < total) {
        ssize_t n = write(fd, ptr + written, total - written);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

template <typename T>
bool readExact(int fd, T& data, int timeout_ms = 60000) {
    char* ptr = reinterpret_cast<char*>(&data);
    size_t total = sizeof(T);
    size_t read_bytes = 0;

    while (read_bytes < total) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) return false;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return false;

        if (pfd.revents & POLLIN) {
            ssize_t n = read(fd, ptr + read_bytes, total - read_bytes);
            if (n <= 0) {
                if (n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                return false;
            }
            read_bytes += static_cast<size_t>(n);
        }
    }
    return true;
}

void wireToReport(const WorkerWireReport& wire, WorkerReport& rep) {
    rep.role = wire.role;
    rep.pid = wire.pid;
    rep.tid = wire.tid;
    rep.physical_uuid = wire.physical_uuid;
    rep.pci_bus_id = wire.pci_bus_id;

    rep.cpu_affinity.clear();
    for (uint32_t i = 0; i < wire.num_affinity_cpus && i < 32; ++i) {
        rep.cpu_affinity.push_back(wire.cpu_affinity[i]);
    }

    rep.ready_ns = wire.ready_ns;
    rep.warmup_start_ns = wire.warmup_start_ns;
    rep.warmup_end_ns = wire.warmup_end_ns;
    rep.warmup_completed = wire.warmup_completed;

    rep.first_submit_ns = wire.first_submit_ns;
    rep.last_submit_ns = wire.last_submit_ns;
    rep.last_completion_ns = wire.last_completion_ns;

    rep.completed = wire.completed;
    rep.late = wire.late;
    rep.latency_samples = wire.latency_samples;

    rep.p50_us = wire.p50_us;
    rep.p95_us = wire.p95_us;
    rep.p99_us = wire.p99_us;
    rep.throughput_ops_sec = wire.throughput_ops_sec;

    rep.late_latencies_us.clear();
    for (uint32_t i = 0; i < wire.num_late_samples && i < 16; ++i) {
        rep.late_latencies_us.push_back(wire.late_latencies_us[i]);
    }

    rep.start_skew_ns = wire.start_skew_ns;
    rep.drain_ns = wire.drain_ns;
    rep.validation_ns = wire.validation_ns;
    rep.final_tag = wire.final_tag;
    rep.validation_passed = (wire.validation_passed != 0);
    rep.failure_error = wire.error_text;
}

void reportToWire(const WorkerReport& rep, WorkerWireReport& wire) {
    std::memset(&wire, 0, sizeof(wire));
    std::strncpy(wire.role, rep.role.c_str(), sizeof(wire.role) - 1);
    wire.pid = rep.pid;
    wire.tid = rep.tid;
    std::strncpy(wire.physical_uuid, rep.physical_uuid.c_str(), sizeof(wire.physical_uuid) - 1);
    std::strncpy(wire.pci_bus_id, rep.pci_bus_id.c_str(), sizeof(wire.pci_bus_id) - 1);

    wire.num_affinity_cpus = std::min<uint32_t>(static_cast<uint32_t>(rep.cpu_affinity.size()), 32);
    for (uint32_t i = 0; i < wire.num_affinity_cpus; ++i) {
        wire.cpu_affinity[i] = rep.cpu_affinity[i];
    }

    wire.ready_ns = rep.ready_ns;
    wire.warmup_start_ns = rep.warmup_start_ns;
    wire.warmup_end_ns = rep.warmup_end_ns;
    wire.warmup_completed = rep.warmup_completed;

    wire.first_submit_ns = rep.first_submit_ns;
    wire.last_submit_ns = rep.last_submit_ns;
    wire.last_completion_ns = rep.last_completion_ns;

    wire.completed = rep.completed;
    wire.late = rep.late;
    wire.latency_samples = rep.latency_samples;

    wire.p50_us = rep.p50_us;
    wire.p95_us = rep.p95_us;
    wire.p99_us = rep.p99_us;
    wire.throughput_ops_sec = rep.throughput_ops_sec;

    wire.num_late_samples = std::min<uint32_t>(static_cast<uint32_t>(rep.late_latencies_us.size()), 16);
    for (uint32_t i = 0; i < wire.num_late_samples; ++i) {
        wire.late_latencies_us[i] = rep.late_latencies_us[i];
    }

    wire.start_skew_ns = rep.start_skew_ns;
    wire.drain_ns = rep.drain_ns;
    wire.validation_ns = rep.validation_ns;
    wire.final_tag = rep.final_tag;
    wire.validation_passed = rep.validation_passed ? 1 : 0;
    std::strncpy(wire.error_text, rep.failure_error.c_str(), sizeof(wire.error_text) - 1);
}

} // anonymous namespace

ControlledRunner::ControlledRunner(const ControlledConfig& config)
    : config_(config) {
    char exe_buf[1024];
    ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (len > 0) {
        exe_buf[len] = '\0';
        binary_path_ = exe_buf;
    } else {
        binary_path_ = "./vuda_native_probe";
    }
}

ControlledRunner::~ControlledRunner() {}

bool ControlledRunner::run() {
    if (config_.mode != "solo-render" &&
        config_.mode != "solo-compute" &&
        config_.mode != "serial" &&
        config_.mode != "concurrent") {
        throw std::invalid_argument("Unknown mode: " + config_.mode);
    }

    if (config_.topology != "threads" && config_.topology != "processes") {
        throw std::invalid_argument("Unknown topology: " + config_.topology);
    }

    if (config_.validation) {
        if (config_.topology == "threads") {
            return runThreadValidation();
        } else {
            return runProcessValidation();
        }
    } else {
        if (config_.topology == "threads") {
            return runThreadTimed();
        } else {
            return runProcessTimed();
        }
    }
}

void ControlledRunner::writeReportJson(const RunReport& report) const {
    if (report.config.report_json_path.empty()) {
        return;
    }

    std::string tmp_path = report.config.report_json_path + ".tmp";
    std::ofstream out(tmp_path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open report output path: " + tmp_path);
    }

    out << "{\n";
    out << "  \"schema_version\": " << report.schema_version << ",\n";
    out << "  \"valid\": " << (report.valid ? "true" : "false") << ",\n";
    out << "  \"mode\": \"" << report.mode << "\",\n";
    out << "  \"topology\": \"" << report.topology << "\",\n";
    out << "  \"validation_strength\": \"" << report.validation_strength << "\",\n";
    out << "  \"percentile_definition\": \"nearest_rank_ceil_pN_minus1\",\n";
    out << "  \"latency_population\": \"" << (report.validation_strength == "per_iteration" ? "validation_per_iteration" : "in_window_boundary_censored") << "\",\n";
    out << "  \"clock\": \"" << report.clock << "\",\n";
    out << "  \"start_ns\": " << report.start_ns << ",\n";
    out << "  \"deadline_ns\": " << report.deadline_ns << ",\n";
    out << "  \"observed_end_ns\": " << report.observed_end_ns << ",\n";

    // config object
    out << "  \"config\": {\n";
    out << "    \"mode\": \"" << report.config.mode << "\",\n";
    out << "    \"topology\": \"" << report.config.topology << "\",\n";
    out << "    \"duration_seconds\": " << std::fixed << std::setprecision(6) << report.config.duration_seconds << ",\n";
    out << "    \"warmup_seconds\": " << std::fixed << std::setprecision(6) << report.config.warmup_seconds << ",\n";
    out << "    \"validation\": " << (report.config.validation ? "true" : "false") << ",\n";
    out << "    \"iterations\": " << report.config.iterations << ",\n";
    out << "    \"elements\": " << report.config.cuda_elements << ",\n";
    out << "    \"intensity\": " << report.config.cuda_intensity << ",\n";
    out << "    \"width\": " << report.config.render_width << ",\n";
    out << "    \"height\": " << report.config.render_height << ",\n";
    out << "    \"device\": " << report.config.target_device << "\n";
    out << "  },\n";

    // workers list
    out << "  \"workers\": [\n";
    for (size_t i = 0; i < report.workers.size(); ++i) {
        const auto& w = report.workers[i];
        out << "    {\n";
        out << "      \"role\": \"" << w.role << "\",\n";
        out << "      \"pid\": " << w.pid << ",\n";
        out << "      \"tid\": " << w.tid << ",\n";
        out << "      \"physical_uuid\": \"" << w.physical_uuid << "\",\n";
        out << "      \"pci_bus_id\": \"" << w.pci_bus_id << "\",\n";
        out << "      \"cpu_affinity\": [";
        for (size_t c = 0; c < w.cpu_affinity.size(); ++c) {
            out << w.cpu_affinity[c] << (c + 1 < w.cpu_affinity.size() ? ", " : "");
        }
        out << "],\n";
        out << "      \"ready_ns\": " << w.ready_ns << ",\n";
        out << "      \"warmup_start_ns\": " << w.warmup_start_ns << ",\n";
        out << "      \"warmup_end_ns\": " << w.warmup_end_ns << ",\n";
        out << "      \"warmup_completed\": " << w.warmup_completed << ",\n";
        out << "      \"first_submit_ns\": " << w.first_submit_ns << ",\n";
        out << "      \"last_submit_ns\": " << w.last_submit_ns << ",\n";
        out << "      \"last_completion_ns\": " << w.last_completion_ns << ",\n";
        out << "      \"completed\": " << w.completed << ",\n";
        out << "      \"late\": " << w.late << ",\n";
        out << "      \"latency_samples\": " << w.latency_samples << ",\n";
        out << "      \"p50_us\": " << std::fixed << std::setprecision(3) << w.p50_us << ",\n";
        out << "      \"p95_us\": " << std::fixed << std::setprecision(3) << w.p95_us << ",\n";
        out << "      \"p99_us\": " << std::fixed << std::setprecision(3) << w.p99_us << ",\n";
        out << "      \"throughput_ops_sec\": " << std::fixed << std::setprecision(3) << w.throughput_ops_sec << ",\n";
        out << "      \"late_latencies_us\": [";
        for (size_t l = 0; l < w.late_latencies_us.size(); ++l) {
            out << std::fixed << std::setprecision(3) << w.late_latencies_us[l]
                << (l + 1 < w.late_latencies_us.size() ? ", " : "");
        }
        out << "],\n";
        out << "      \"start_skew_ns\": " << w.start_skew_ns << ",\n";
        out << "      \"drain_ns\": " << w.drain_ns << ",\n";
        out << "      \"validation_ns\": " << w.validation_ns << ",\n";
        out << "      \"final_tag\": " << w.final_tag << ",\n";
        out << "      \"validation_passed\": " << (w.validation_passed ? "true" : "false") << ",\n";
        out << "      \"trace\": {\n";
        out << "        \"path\": \"" << w.trace.path << "\",\n";
        out << "        \"count\": " << w.trace.count << ",\n";
        out << "        \"format\": \"" << w.trace.format << "\"\n";
        out << "      }\n";
        out << "    }" << (i + 1 < report.workers.size() ? ",\n" : "\n");
    }
    out << "  ],\n";

    // pairs object
    if (report.has_pairs) {
        out << "  \"pairs\": {\n";
        out << "    \"completed\": " << report.pairs.completed << ",\n";
        out << "    \"late\": " << report.pairs.late << ",\n";
        out << "    \"latency_samples\": " << report.pairs.latency_samples << ",\n";
        out << "    \"p50_us\": " << std::fixed << std::setprecision(3) << report.pairs.p50_us << ",\n";
        out << "    \"p95_us\": " << std::fixed << std::setprecision(3) << report.pairs.p95_us << ",\n";
        out << "    \"p99_us\": " << std::fixed << std::setprecision(3) << report.pairs.p99_us << ",\n";
        out << "    \"late_latencies_us\": [";
        for (size_t l = 0; l < report.pairs.late_latencies_us.size(); ++l) {
            out << std::fixed << std::setprecision(3) << report.pairs.late_latencies_us[l]
                << (l + 1 < report.pairs.late_latencies_us.size() ? ", " : "");
        }
        out << "],\n";
        out << "    \"trace\": {\n";
        out << "      \"path\": \"" << report.pairs.trace.path << "\",\n";
        out << "      \"count\": " << report.pairs.trace.count << ",\n";
        out << "      \"format\": \"" << report.pairs.trace.format << "\"\n";
        out << "    }\n";
        out << "  }\n";
    } else {
        out << "  \"pairs\": null\n";
    }
    out << "}\n";

    out.close();

    // Atomic rename
    if (std::rename(tmp_path.c_str(), report.config.report_json_path.c_str()) != 0) {
        throw std::runtime_error("Failed to atomically rename report: " + report.config.report_json_path);
    }
}

// -------------------------------------------------------------------------------------------------
// Thread Topology: Timed Mode
// -------------------------------------------------------------------------------------------------

bool ControlledRunner::runThreadTimed() {
    const size_t MAX_SAMPLES = 5000000;

    auto guard = verifyPhysicalDeviceIdentity(config_.target_device);
    if (!guard.matched) {
        std::cerr << "[ControlledRunner] Physical device guard check failed: "
                  << guard.failure_reason << std::endl;
        return false;
    }

    bool need_render = (config_.mode != "solo-compute");
    bool need_compute = (config_.mode != "solo-render");

    WorkerReport render_report;
    render_report.role = "render";
    render_report.physical_uuid = guard.cuda_uuid_hex;
    render_report.pci_bus_id = guard.pci_bus_id;
    render_report.late_latencies_us.reserve(1);

    WorkerReport compute_report;
    compute_report.role = "compute";
    compute_report.physical_uuid = guard.cuda_uuid_hex;
    compute_report.pci_bus_id = guard.pci_bus_id;
    compute_report.late_latencies_us.reserve(1);

    std::atomic<bool> render_ready{false};
    std::atomic<bool> compute_ready{false};
    std::atomic<bool> fatal_error{false};

    std::atomic<uint64_t> shared_start_ns{0};
    std::atomic<uint64_t> shared_deadline_ns{0};
    std::atomic<bool> window_released{false};

    std::atomic<uint64_t> serial_render_turn{1};
    std::atomic<uint64_t> serial_compute_turn{0};
    std::atomic<bool> serial_stop{false};

    struct PairRecord {
        uint64_t submit_ns;
        uint64_t completion_ns;
    };
    std::vector<PairRecord> pair_records;
    if (config_.mode == "serial") pair_records.reserve(MAX_SAMPLES);

    std::vector<OperationRecord> render_records;
    std::vector<OperationRecord> compute_records;
    if (need_render) render_records.reserve(MAX_SAMPLES);
    if (need_compute) compute_records.reserve(MAX_SAMPLES);

    std::vector<double> render_latencies_us;
    std::vector<double> compute_latencies_us;
    if (need_render) render_latencies_us.reserve(MAX_SAMPLES);
    if (need_compute) compute_latencies_us.reserve(MAX_SAMPLES);
    std::thread render_thread;
    std::thread compute_thread;

    // Render Worker
    if (need_render) {
        render_thread = std::thread([&]() {
            try {
                if (config_.render_cpu >= 0) pinCpu(config_.render_cpu);
                render_report.pid = static_cast<int>(getpid());
                render_report.tid = getCurrentTid();
                render_report.cpu_affinity = getAffinityCpus();

                EGLRenderer renderer(config_.render_width, config_.render_height);
                if (!renderer.initialize(config_.target_device)) {
                    throw std::runtime_error("Render initialize failed");
                }
                if (!renderer.setupFBO()) {
                    throw std::runtime_error("Render setupFBO failed");
                }
                if (!renderer.setupShaders()) {
                    throw std::runtime_error("Render setupShaders failed");
                }

                // Warmup
                {
                    ScopedNvtx nvtx("Render_Warmup");
                    render_report.warmup_start_ns = getMonotonicTimeNs();
                    uint64_t warmup_deadline = render_report.warmup_start_ns +
                        static_cast<uint64_t>(config_.warmup_seconds * 1e9);
                    uint64_t w_iter = 0;
                    while (getMonotonicTimeNs() < warmup_deadline && !fatal_error.load()) {
                        renderer.renderTaggedFrame(++w_iter);
                        render_report.warmup_completed++;
                    }
                    render_report.warmup_end_ns = getMonotonicTimeNs();
                }

                render_report.ready_ns = getMonotonicTimeNs();
                render_ready.store(true, std::memory_order_release);

                while (!window_released.load(std::memory_order_acquire)) {
                    if (fatal_error.load(std::memory_order_relaxed)) return;
                    std::this_thread::yield();
                }

                uint64_t start_ns = shared_start_ns.load(std::memory_order_acquire);
                uint64_t deadline_ns = shared_deadline_ns.load(std::memory_order_acquire);
                MeasurementWindow window(start_ns, deadline_ns);

                while (getMonotonicTimeNs() < start_ns) {}

                {
                    ScopedNvtx nvtx("Render_Measurement");
                    uint64_t iter = 0;

                    if (config_.mode == "serial") {
                        while (!serial_stop.load(std::memory_order_relaxed) && !fatal_error.load()) {
                            while (serial_render_turn.load(std::memory_order_acquire) != (iter + 1)) {
                                if (serial_stop.load(std::memory_order_relaxed) || fatal_error.load()) break;
                                std::this_thread::yield();
                            }
                            if (serial_stop.load(std::memory_order_relaxed)) break;

                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) {
                                serial_stop.store(true, std::memory_order_release);
                                serial_compute_turn.store(iter + 1, std::memory_order_release);
                                break;
                            }

                            if (render_report.first_submit_ns == 0) {
                                render_report.first_submit_ns = submit_ns;
                                render_report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            render_report.last_submit_ns = submit_ns;

                            iter++;
                            renderer.renderTaggedFrame(iter);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            render_report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (render_latencies_us.size() >= MAX_SAMPLES) {
                                    throw std::runtime_error("Render sample buffer capacity exhausted");
                                }
                                render_latencies_us.push_back(lat_us);
                                appendMeasurementSample(render_records, {submit_ns, comp_ns});
                                render_report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                render_report.late++;
                                appendMeasurementSample(render_records, {submit_ns, comp_ns});
                                appendMeasurementSample(render_report.late_latencies_us, lat_us);
                            }

                            serial_compute_turn.store(iter, std::memory_order_release);
                        }
                    } else { // solo-render or concurrent
                        while (!fatal_error.load()) {
                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) break;

                            if (render_report.first_submit_ns == 0) {
                                render_report.first_submit_ns = submit_ns;
                                render_report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            render_report.last_submit_ns = submit_ns;

                            iter++;
                            renderer.renderTaggedFrame(iter);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            render_report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (render_latencies_us.size() >= MAX_SAMPLES) {
                                    throw std::runtime_error("Render sample buffer capacity exhausted");
                                }
                                render_latencies_us.push_back(lat_us);
                                appendMeasurementSample(render_records, {submit_ns, comp_ns});
                                render_report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                render_report.late++;
                                appendMeasurementSample(render_records, {submit_ns, comp_ns});
                                appendMeasurementSample(render_report.late_latencies_us, lat_us);
                            }
                        }
                    }
                    render_report.final_tag = iter;
                }

                uint64_t drain_start = getMonotonicTimeNs();
                glFinish();
                uint64_t drain_end = getMonotonicTimeNs();
                uint64_t post_deadline_tail = (render_report.last_completion_ns > deadline_ns) ?
                    (render_report.last_completion_ns - deadline_ns) : 0;
                render_report.drain_ns = post_deadline_tail + (drain_end - drain_start);

                {
                    ScopedNvtx nvtx("Render_Validation");
                    uint64_t val_start = getMonotonicTimeNs();
                    if (render_report.final_tag > 0) {
                        if (!renderer.verifyTaggedFrame(render_report.final_tag)) {
                            throw std::runtime_error("Final render tag verification failed for tag " +
                                                     std::to_string(render_report.final_tag));
                        }
                    }
                    render_report.validation_passed = true;
                    render_report.validation_ns = getMonotonicTimeNs() - val_start;
                }

            } catch (const std::exception& e) {
                fatal_error.store(true);
                render_report.failure_error = e.what();
                std::cerr << "[RenderWorker Error] " << e.what() << std::endl;
            }
        });
    }

    // Compute Worker
    if (need_compute) {
        compute_thread = std::thread([&]() {
            cudaStream_t stream = nullptr;
            try {
                if (config_.compute_cpu >= 0) pinCpu(config_.compute_cpu);
                compute_report.pid = static_cast<int>(getpid());
                compute_report.tid = getCurrentTid();
                compute_report.cpu_affinity = getAffinityCpus();

                CudaCompute compute(config_.cuda_elements, config_.cuda_intensity);
                if (!compute.initialize(config_.target_device)) {
                    throw std::runtime_error("Compute initialize failed");
                }
                checkCudaError(cudaStreamCreate(&stream), "cudaStreamCreate");

                // Warmup
                {
                    ScopedNvtx nvtx("Compute_Warmup");
                    compute_report.warmup_start_ns = getMonotonicTimeNs();
                    uint64_t warmup_deadline = compute_report.warmup_start_ns +
                        static_cast<uint64_t>(config_.warmup_seconds * 1e9);
                    uint64_t w_iter = 0;
                    while (getMonotonicTimeNs() < warmup_deadline && !fatal_error.load()) {
                        compute.launchTaggedCompute(++w_iter, stream);
                        compute.synchronize(stream);
                        compute_report.warmup_completed++;
                    }
                    compute_report.warmup_end_ns = getMonotonicTimeNs();
                }

                compute_report.ready_ns = getMonotonicTimeNs();
                compute_ready.store(true, std::memory_order_release);

                while (!window_released.load(std::memory_order_acquire)) {
                    if (fatal_error.load(std::memory_order_relaxed)) return;
                    std::this_thread::yield();
                }

                uint64_t start_ns = shared_start_ns.load(std::memory_order_acquire);
                uint64_t deadline_ns = shared_deadline_ns.load(std::memory_order_acquire);
                MeasurementWindow window(start_ns, deadline_ns);

                while (getMonotonicTimeNs() < start_ns) {}

                {
                    ScopedNvtx nvtx("Compute_Measurement");
                    uint64_t iter = 0;

                    if (config_.mode == "serial") {
                        while (!serial_stop.load(std::memory_order_relaxed) && !fatal_error.load()) {
                            while (serial_compute_turn.load(std::memory_order_acquire) != (iter + 1)) {
                                if (serial_stop.load(std::memory_order_relaxed) || fatal_error.load()) break;
                                std::this_thread::yield();
                            }
                            if (serial_stop.load(std::memory_order_relaxed)) break;

                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) {
                                serial_stop.store(true, std::memory_order_release);
                                break;
                            }

                            if (compute_report.first_submit_ns == 0) {
                                compute_report.first_submit_ns = submit_ns;
                                compute_report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            compute_report.last_submit_ns = submit_ns;

                            iter++;
                            compute.launchTaggedCompute(iter, stream);
                            compute.synchronize(stream);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            compute_report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (compute_latencies_us.size() >= MAX_SAMPLES) {
                                    throw std::runtime_error("Compute sample buffer capacity exhausted");
                                }
                                compute_latencies_us.push_back(lat_us);
                                appendMeasurementSample(compute_records, {submit_ns, comp_ns});
                                compute_report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                compute_report.late++;
                                appendMeasurementSample(compute_records, {submit_ns, comp_ns});
                                appendMeasurementSample(compute_report.late_latencies_us, lat_us);
                            }

                            if (iter <= render_latencies_us.size()) {
                                uint64_t pair_submit = render_report.last_submit_ns;
                                appendMeasurementSample(pair_records, {pair_submit, comp_ns});
                            }

                            serial_render_turn.store(iter + 1, std::memory_order_release);
                        }
                    } else { // solo-compute or concurrent
                        while (!fatal_error.load()) {
                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) break;

                            if (compute_report.first_submit_ns == 0) {
                                compute_report.first_submit_ns = submit_ns;
                                compute_report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            compute_report.last_submit_ns = submit_ns;

                            iter++;
                            compute.launchTaggedCompute(iter, stream);
                            compute.synchronize(stream);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            compute_report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (compute_latencies_us.size() >= MAX_SAMPLES) {
                                    throw std::runtime_error("Compute sample buffer capacity exhausted");
                                }
                                compute_latencies_us.push_back(lat_us);
                                appendMeasurementSample(compute_records, {submit_ns, comp_ns});
                                compute_report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                compute_report.late++;
                                appendMeasurementSample(compute_records, {submit_ns, comp_ns});
                                appendMeasurementSample(compute_report.late_latencies_us, lat_us);
                            }
                        }
                    }
                    compute_report.final_tag = iter;
                }

                uint64_t drain_start = getMonotonicTimeNs();
                compute.synchronize(stream);
                uint64_t drain_end = getMonotonicTimeNs();
                uint64_t post_deadline_tail = (compute_report.last_completion_ns > deadline_ns) ?
                    (compute_report.last_completion_ns - deadline_ns) : 0;
                compute_report.drain_ns = post_deadline_tail + (drain_end - drain_start);

                {
                    ScopedNvtx nvtx("Compute_Validation");
                    uint64_t val_start = getMonotonicTimeNs();
                    if (compute_report.final_tag > 0) {
                        if (!compute.verifyTaggedOutput(compute_report.final_tag)) {
                            throw std::runtime_error("Final compute tag verification failed for tag " +
                                                     std::to_string(compute_report.final_tag));
                        }
                    }
                    compute_report.validation_passed = true;
                    compute_report.validation_ns = getMonotonicTimeNs() - val_start;
                }

                if (stream) {
                    checkCudaError(cudaStreamDestroy(stream), "cudaStreamDestroy");
                    stream = nullptr;
                }
            } catch (const std::exception& e) {
                fatal_error.store(true);
                compute_report.failure_error = e.what();
                std::cerr << "[ComputeWorker Error] " << e.what() << std::endl;
                if (stream) cudaStreamDestroy(stream);
            }
        });
    }

    auto wait_ready_start = std::chrono::steady_clock::now();
    while (true) {
        if (fatal_error.load()) {
            if (render_thread.joinable()) render_thread.join();
            if (compute_thread.joinable()) compute_thread.join();
            return false;
        }
        bool r_ok = (!need_render || render_ready.load(std::memory_order_acquire));
        bool c_ok = (!need_compute || compute_ready.load(std::memory_order_acquire));
        if (r_ok && c_ok) break;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - wait_ready_start).count();
        if (elapsed > 120) {
            fatal_error.store(true);
            if (render_thread.joinable()) render_thread.join();
            if (compute_thread.joinable()) compute_thread.join();
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    uint64_t now_ns = getMonotonicTimeNs();
    uint64_t planned_start_ns = now_ns + 50000000ULL;
    auto window = MeasurementWindow::fromDurationSeconds(planned_start_ns, config_.duration_seconds);

    shared_start_ns.store(window.start_ns, std::memory_order_release);
    shared_deadline_ns.store(window.deadline_ns, std::memory_order_release);
    window_released.store(true, std::memory_order_release);

    if (render_thread.joinable()) render_thread.join();
    if (compute_thread.joinable()) compute_thread.join();

    uint64_t observed_end_ns = getMonotonicTimeNs();

    if (fatal_error.load()) return false;

    uint64_t req_warmup_ns = static_cast<uint64_t>(config_.warmup_seconds * 1e9);
    if (need_render) {
        if (render_report.warmup_completed == 0) {
            std::cerr << "[ControlledRunner] Rejecting run: render warmup completed 0 operations" << std::endl;
            return false;
        }
        if (render_report.warmup_end_ns < render_report.warmup_start_ns + req_warmup_ns) {
            std::cerr << "[ControlledRunner] Rejecting run: render warmup elapsed time less than requested" << std::endl;
            return false;
        }
        if (render_report.completed == 0) {
            std::cerr << "[ControlledRunner] Rejecting run: render worker completed 0 in-window operations" << std::endl;
            return false;
        }
        if (!render_report.validation_passed) return false;
    }

    if (need_compute) {
        if (compute_report.warmup_completed == 0) {
            std::cerr << "[ControlledRunner] Rejecting run: compute warmup completed 0 operations" << std::endl;
            return false;
        }
        if (compute_report.warmup_end_ns < compute_report.warmup_start_ns + req_warmup_ns) {
            std::cerr << "[ControlledRunner] Rejecting run: compute warmup elapsed time less than requested" << std::endl;
            return false;
        }
        if (compute_report.completed == 0) {
            std::cerr << "[ControlledRunner] Rejecting run: compute worker completed 0 in-window operations" << std::endl;
            return false;
        }
        if (!compute_report.validation_passed) return false;
    }
    double window_duration_sec = window.duration_ns() / 1e9;

    if (need_render && !render_latencies_us.empty()) {
        std::sort(render_latencies_us.begin(), render_latencies_us.end());
        auto stats = LatencyStats::calculate(render_latencies_us);
        render_report.latency_samples = stats.count;
        render_report.p50_us = stats.p50_us;
        render_report.p95_us = stats.p95_us;
        render_report.p99_us = stats.p99_us;
        render_report.throughput_ops_sec = render_report.completed / window_duration_sec;
    }

    if (need_compute && !compute_latencies_us.empty()) {
        std::sort(compute_latencies_us.begin(), compute_latencies_us.end());
        auto stats = LatencyStats::calculate(compute_latencies_us);
        compute_report.latency_samples = stats.count;
        compute_report.p50_us = stats.p50_us;
        compute_report.p95_us = stats.p95_us;
        compute_report.p99_us = stats.p99_us;
        compute_report.throughput_ops_sec = compute_report.completed / window_duration_sec;
    }

    RunReport run_report;
    run_report.schema_version = 1;
    run_report.valid = true;
    run_report.mode = config_.mode;
    run_report.topology = config_.topology;
    run_report.validation_strength = "final_tag_only";
    run_report.config = config_;
    run_report.start_ns = window.start_ns;
    run_report.deadline_ns = window.deadline_ns;
    run_report.observed_end_ns = observed_end_ns;

    if (need_render) {
        if (!config_.report_json_path.empty()) {
            std::string trace_path = config_.report_json_path + ".render.samples.bin";
            dumpOperationTrace(trace_path, window.start_ns, window.deadline_ns, render_records);
            render_report.trace.path = trace_path;
            render_report.trace.count = render_records.size();
            render_report.trace.format = "exp9ts01_le";
        }
        run_report.workers.push_back(render_report);
    }
    if (need_compute) {
        if (!config_.report_json_path.empty()) {
            std::string trace_path = config_.report_json_path + ".compute.samples.bin";
            dumpOperationTrace(trace_path, window.start_ns, window.deadline_ns, compute_records);
            compute_report.trace.path = trace_path;
            compute_report.trace.count = compute_records.size();
            compute_report.trace.format = "exp9ts01_le";
        }
        run_report.workers.push_back(compute_report);
    }
    if (config_.mode == "serial" && need_render && need_compute) {
        run_report.has_pairs = true;
        std::vector<double> in_window_pair_latencies_us;
        in_window_pair_latencies_us.reserve(pair_records.size());

        for (const auto& rec : pair_records) {
            if (rec.submit_ns >= window.start_ns && rec.completion_ns < window.deadline_ns) {
                in_window_pair_latencies_us.push_back((rec.completion_ns - rec.submit_ns) / 1000.0);
            } else if (rec.submit_ns >= window.start_ns && rec.submit_ns < window.deadline_ns &&
                       rec.completion_ns >= window.deadline_ns) {
                run_report.pairs.late++;
                run_report.pairs.late_latencies_us.push_back((rec.completion_ns - rec.submit_ns) / 1000.0);
            }
        }

        run_report.pairs.completed = in_window_pair_latencies_us.size();
        if (!in_window_pair_latencies_us.empty()) {
            std::sort(in_window_pair_latencies_us.begin(), in_window_pair_latencies_us.end());
            auto pair_stats = LatencyStats::calculate(in_window_pair_latencies_us);
            run_report.pairs.latency_samples = pair_stats.count;
            run_report.pairs.p50_us = pair_stats.p50_us;
            run_report.pairs.p95_us = pair_stats.p95_us;
            run_report.pairs.p99_us = pair_stats.p99_us;
        }
        if (!config_.report_json_path.empty()) {
            std::string trace_path = config_.report_json_path + ".pairs.samples.bin";
            std::vector<OperationRecord> pair_trace_records;
            pair_trace_records.reserve(pair_records.size());
            for (const auto& pr : pair_records) {
                pair_trace_records.push_back({pr.submit_ns, pr.completion_ns});
            }
            dumpOperationTrace(trace_path, window.start_ns, window.deadline_ns, pair_trace_records);
            run_report.pairs.trace.path = trace_path;
            run_report.pairs.trace.count = pair_trace_records.size();
            run_report.pairs.trace.format = "exp9ts01_le";
        }
    }
    writeReportJson(run_report);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Thread Topology: Strict Per-Iteration Validation Mode
// -------------------------------------------------------------------------------------------------

bool ControlledRunner::runThreadValidation() {
    if (config_.iterations <= 0) {
        std::cerr << "[ControlledRunner] Validation mode requires iterations > 0" << std::endl;
        return false;
    }

    auto guard = verifyPhysicalDeviceIdentity(config_.target_device);
    if (!guard.matched) {
        std::cerr << "[ControlledRunner] Physical device guard check failed: "
                  << guard.failure_reason << std::endl;
        return false;
    }

    bool need_render = (config_.mode != "solo-compute");
    bool need_compute = (config_.mode != "solo-render");

    WorkerReport render_report;
    render_report.role = "render";
    render_report.physical_uuid = guard.cuda_uuid_hex;
    render_report.pci_bus_id = guard.pci_bus_id;

    WorkerReport compute_report;
    compute_report.role = "compute";
    compute_report.physical_uuid = guard.cuda_uuid_hex;
    compute_report.pci_bus_id = guard.pci_bus_id;

    std::atomic<bool> fatal_error{false};
    uint64_t start_ns = getMonotonicTimeNs();

    std::thread render_thread;
    std::thread compute_thread;

    if (need_render) {
        render_thread = std::thread([&]() {
            try {
                if (config_.render_cpu >= 0) pinCpu(config_.render_cpu);
                render_report.pid = static_cast<int>(getpid());
                render_report.tid = getCurrentTid();
                render_report.cpu_affinity = getAffinityCpus();

                EGLRenderer renderer(config_.render_width, config_.render_height);
                if (!renderer.initialize(config_.target_device)) throw std::runtime_error("Render init failed");
                if (!renderer.setupFBO()) throw std::runtime_error("Render setupFBO failed");
                if (!renderer.setupShaders()) throw std::runtime_error("Render setupShaders failed");

                render_report.first_submit_ns = getMonotonicTimeNs();

                for (int iter = 1; iter <= config_.iterations; ++iter) {
                    if (fatal_error.load()) break;

                    uint64_t sub_ns = getMonotonicTimeNs();
                    render_report.last_submit_ns = sub_ns;
                    renderer.renderTaggedFrame(iter);
                    render_report.last_completion_ns = getMonotonicTimeNs();

                    if (!renderer.verifyTaggedFrame(iter)) {
                        throw std::runtime_error("Per-iteration render tag verification failed at iteration " +
                                                 std::to_string(iter));
                    }
                    render_report.completed++;
                    render_report.final_tag = iter;
                }
                render_report.validation_passed = (render_report.completed == static_cast<uint64_t>(config_.iterations));
            } catch (const std::exception& e) {
                fatal_error.store(true);
                render_report.failure_error = e.what();
                std::cerr << "[RenderValidation Error] " << e.what() << std::endl;
            }
        });
    }

    if (need_compute) {
        compute_thread = std::thread([&]() {
            cudaStream_t stream = nullptr;
            try {
                if (config_.compute_cpu >= 0) pinCpu(config_.compute_cpu);
                compute_report.pid = static_cast<int>(getpid());
                compute_report.tid = getCurrentTid();
                compute_report.cpu_affinity = getAffinityCpus();

                CudaCompute compute(config_.cuda_elements, config_.cuda_intensity);
                if (!compute.initialize(config_.target_device)) throw std::runtime_error("Compute init failed");
                checkCudaError(cudaStreamCreate(&stream), "cudaStreamCreate");

                compute_report.first_submit_ns = getMonotonicTimeNs();

                for (int iter = 1; iter <= config_.iterations; ++iter) {
                    if (fatal_error.load()) break;

                    uint64_t sub_ns = getMonotonicTimeNs();
                    compute_report.last_submit_ns = sub_ns;
                    compute.launchTaggedCompute(iter, stream);
                    compute.synchronize(stream);
                    compute_report.last_completion_ns = getMonotonicTimeNs();

                    if (!compute.verifyTaggedOutput(iter)) {
                        throw std::runtime_error("Per-iteration compute tag verification failed at iteration " +
                                                 std::to_string(iter));
                    }
                    compute_report.completed++;
                    compute_report.final_tag = iter;
                }
                compute_report.validation_passed = (compute_report.completed == static_cast<uint64_t>(config_.iterations));
                if (stream) cudaStreamDestroy(stream);
            } catch (const std::exception& e) {
                fatal_error.store(true);
                compute_report.failure_error = e.what();
                std::cerr << "[ComputeValidation Error] " << e.what() << std::endl;
                if (stream) cudaStreamDestroy(stream);
            }
        });
    }

    if (render_thread.joinable()) render_thread.join();
    if (compute_thread.joinable()) compute_thread.join();

    uint64_t observed_end_ns = getMonotonicTimeNs();

    if (fatal_error.load()) return false;
    if (need_render && !render_report.validation_passed) return false;
    if (need_compute && !compute_report.validation_passed) return false;

    RunReport run_report;
    run_report.schema_version = 1;
    run_report.valid = true;
    run_report.mode = config_.mode;
    run_report.topology = config_.topology;
    run_report.validation_strength = "per_iteration";
    run_report.config = config_;
    run_report.start_ns = start_ns;
    run_report.deadline_ns = start_ns + static_cast<uint64_t>(config_.duration_seconds * 1e9);
    run_report.observed_end_ns = observed_end_ns;

    if (need_render) run_report.workers.push_back(render_report);
    if (need_compute) run_report.workers.push_back(compute_report);

    writeReportJson(run_report);
    return true;
}

// -------------------------------------------------------------------------------------------------
// Process Topology Worker Entrypoint
// -------------------------------------------------------------------------------------------------

int ControlledRunner::runInternalWorker(const ControlledConfig& config) {
    int fd_in = config.ipc_sync_fd_in;
    int fd_out = config.ipc_sync_fd_out;

    if (fd_in < 0 || fd_out < 0) {
        std::cerr << "[InternalWorker] Invalid sync pipe fds" << std::endl;
        return 1;
    }

    const size_t MAX_SAMPLES = 5000000;
    std::vector<double> latencies_us;
    latencies_us.reserve(MAX_SAMPLES);
    std::vector<OperationRecord> op_records;
    op_records.reserve(MAX_SAMPLES);
    uint64_t trace_start_ns = 0;
    uint64_t trace_deadline_ns = 0;

    WorkerReport report;
    report.role = config.internal_worker_role;
    report.pid = static_cast<int>(getpid());
    report.tid = getCurrentTid();
    report.late_latencies_us.reserve(1);

    try {
        if (report.role == "render") {
            if (config.render_cpu >= 0) pinCpu(config.render_cpu);
            report.cpu_affinity = getAffinityCpus();

            auto guard = verifyPhysicalDeviceIdentity(config.target_device);
            if (!guard.matched) {
                throw std::runtime_error("Guard check failed: " + guard.failure_reason);
            }
            report.physical_uuid = guard.cuda_uuid_hex;
            report.pci_bus_id = guard.pci_bus_id;

            EGLRenderer renderer(config.render_width, config.render_height);
            if (!renderer.initialize(config.target_device)) throw std::runtime_error("Render initialize failed");
            if (!renderer.setupFBO()) throw std::runtime_error("Render setupFBO failed");
            if (!renderer.setupShaders()) throw std::runtime_error("Render setupShaders failed");

            if (!config.validation) {
                ScopedNvtx nvtx("Render_Process_Warmup");
                report.warmup_start_ns = getMonotonicTimeNs();
                uint64_t warmup_deadline = report.warmup_start_ns +
                    static_cast<uint64_t>(config.warmup_seconds * 1e9);
                uint64_t w_iter = 0;
                while (getMonotonicTimeNs() < warmup_deadline) {
                    renderer.renderTaggedFrame(++w_iter);
                    report.warmup_completed++;
                }
                report.warmup_end_ns = getMonotonicTimeNs();
            }

            report.ready_ns = getMonotonicTimeNs();

            IpcCommand ready_cmd;
            ready_cmd.type = IpcCommand::CMD_READY;
            ready_cmd.timestamp_ns = report.ready_ns;
            if (!writeExact(fd_out, ready_cmd)) throw std::runtime_error("Failed to write CMD_READY");

            IpcCommand start_cmd;
            if (!readExact(fd_in, start_cmd)) throw std::runtime_error("Failed to read start command");

            if (config.validation) {
                // Strict per-iteration validation path
                report.first_submit_ns = getMonotonicTimeNs();
                for (int iter = 1; iter <= config.iterations; ++iter) {
                    uint64_t sub_ns = getMonotonicTimeNs();
                    report.last_submit_ns = sub_ns;
                    renderer.renderTaggedFrame(iter);
                    report.last_completion_ns = getMonotonicTimeNs();

                    if (!renderer.verifyTaggedFrame(iter)) {
                        throw std::runtime_error("Render process tag verification failed at iter " + std::to_string(iter));
                    }
                    report.completed++;
                    report.final_tag = iter;
                }
                report.validation_passed = (report.completed == static_cast<uint64_t>(config.iterations));
            } else {
                // Timed measurement path
                uint64_t start_ns = start_cmd.start_ns;
                uint64_t deadline_ns = start_cmd.deadline_ns;
                trace_start_ns = start_ns;
                trace_deadline_ns = deadline_ns;
                MeasurementWindow window(start_ns, deadline_ns);
                while (getMonotonicTimeNs() < start_ns) {}

                {
                    ScopedNvtx nvtx("Render_Process_Measurement");
                    uint64_t iter = 0;

                    if (config.mode == "serial") {
                        while (true) {
                            IpcCommand step_cmd;
                            if (!readExact(fd_in, step_cmd)) break;
                            if (step_cmd.type == IpcCommand::CMD_TERMINATE) break;
                            if (step_cmd.type != IpcCommand::CMD_STEP_RENDER) continue;

                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) {
                                IpcCommand done_cmd;
                                done_cmd.type = IpcCommand::CMD_RENDER_DONE;
                                done_cmd.timestamp_ns = submit_ns;
                                done_cmd.iteration = iter;
                                writeExact(fd_out, done_cmd);
                                break;
                            }

                            if (report.first_submit_ns == 0) {
                                report.first_submit_ns = submit_ns;
                                report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            report.last_submit_ns = submit_ns;

                            iter++;
                            renderer.renderTaggedFrame(iter);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (latencies_us.size() >= MAX_SAMPLES) throw std::runtime_error("Capacity exhausted");
                                latencies_us.push_back(lat_us);
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                report.late++;
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                appendMeasurementSample(report.late_latencies_us, lat_us);
                            }

                            IpcCommand done_cmd;
                            done_cmd.type = IpcCommand::CMD_RENDER_DONE;
                            done_cmd.timestamp_ns = comp_ns;
                            done_cmd.start_ns = submit_ns;
                            done_cmd.iteration = iter;
                            if (!writeExact(fd_out, done_cmd)) break;
                        }
                    } else { // solo-render or concurrent
                        while (true) {
                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) break;

                            if (report.first_submit_ns == 0) {
                                report.first_submit_ns = submit_ns;
                                report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            report.last_submit_ns = submit_ns;

                            iter++;
                            renderer.renderTaggedFrame(iter);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (latencies_us.size() >= MAX_SAMPLES) throw std::runtime_error("Capacity exhausted");
                                latencies_us.push_back(lat_us);
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                report.late++;
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                appendMeasurementSample(report.late_latencies_us, lat_us);
                            }
                        }
                    }
                    report.final_tag = iter;
                }

                uint64_t drain_start = getMonotonicTimeNs();
                glFinish();
                uint64_t drain_end = getMonotonicTimeNs();
                uint64_t post_deadline_tail = (report.last_completion_ns > start_cmd.deadline_ns) ?
                    (report.last_completion_ns - start_cmd.deadline_ns) : 0;
                report.drain_ns = post_deadline_tail + (drain_end - drain_start);

                {
                    ScopedNvtx nvtx("Render_Process_Validation");
                    uint64_t val_start = getMonotonicTimeNs();
                    if (report.final_tag > 0) {
                        if (!renderer.verifyTaggedFrame(report.final_tag)) {
                            throw std::runtime_error("Render process final tag verification failed");
                        }
                    }
                    report.validation_passed = true;
                    report.validation_ns = getMonotonicTimeNs() - val_start;
                }
            }

        } else if (report.role == "compute") {
            if (config.compute_cpu >= 0) pinCpu(config.compute_cpu);
            report.cpu_affinity = getAffinityCpus();

            auto guard = verifyPhysicalDeviceIdentity(config.target_device);
            if (!guard.matched) {
                throw std::runtime_error("Guard check failed: " + guard.failure_reason);
            }
            report.physical_uuid = guard.cuda_uuid_hex;
            report.pci_bus_id = guard.pci_bus_id;

            CudaCompute compute(config.cuda_elements, config.cuda_intensity);
            if (!compute.initialize(config.target_device)) throw std::runtime_error("Compute initialize failed");

            cudaStream_t stream = nullptr;
            checkCudaError(cudaStreamCreate(&stream), "cudaStreamCreate");

            if (!config.validation) {
                ScopedNvtx nvtx("Compute_Process_Warmup");
                report.warmup_start_ns = getMonotonicTimeNs();
                uint64_t warmup_deadline = report.warmup_start_ns +
                    static_cast<uint64_t>(config.warmup_seconds * 1e9);
                uint64_t w_iter = 0;
                while (getMonotonicTimeNs() < warmup_deadline) {
                    compute.launchTaggedCompute(++w_iter, stream);
                    compute.synchronize(stream);
                    report.warmup_completed++;
                }
                report.warmup_end_ns = getMonotonicTimeNs();
            }

            report.ready_ns = getMonotonicTimeNs();

            IpcCommand ready_cmd;
            ready_cmd.type = IpcCommand::CMD_READY;
            ready_cmd.timestamp_ns = report.ready_ns;
            if (!writeExact(fd_out, ready_cmd)) throw std::runtime_error("Failed to write CMD_READY");

            IpcCommand start_cmd;
            if (!readExact(fd_in, start_cmd)) throw std::runtime_error("Failed to read start command");

            if (config.validation) {
                report.first_submit_ns = getMonotonicTimeNs();
                for (int iter = 1; iter <= config.iterations; ++iter) {
                    uint64_t sub_ns = getMonotonicTimeNs();
                    report.last_submit_ns = sub_ns;
                    compute.launchTaggedCompute(iter, stream);
                    compute.synchronize(stream);
                    report.last_completion_ns = getMonotonicTimeNs();

                    if (!compute.verifyTaggedOutput(iter)) {
                        throw std::runtime_error("Compute process tag verification failed at iter " + std::to_string(iter));
                    }
                    report.completed++;
                    report.final_tag = iter;
                }
                report.validation_passed = (report.completed == static_cast<uint64_t>(config.iterations));
            } else {
                uint64_t start_ns = start_cmd.start_ns;
                uint64_t deadline_ns = start_cmd.deadline_ns;
                trace_start_ns = start_ns;
                trace_deadline_ns = deadline_ns;
                MeasurementWindow window(start_ns, deadline_ns);
                while (getMonotonicTimeNs() < start_ns) {}

                {
                    ScopedNvtx nvtx("Compute_Process_Measurement");
                    uint64_t iter = 0;

                    if (config.mode == "serial") {
                        while (true) {
                            IpcCommand step_cmd;
                            if (!readExact(fd_in, step_cmd)) break;
                            if (step_cmd.type == IpcCommand::CMD_TERMINATE) break;
                            if (step_cmd.type != IpcCommand::CMD_STEP_COMPUTE) continue;

                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) {
                                IpcCommand done_cmd;
                                done_cmd.type = IpcCommand::CMD_COMPUTE_DONE;
                                done_cmd.timestamp_ns = submit_ns;
                                done_cmd.iteration = iter;
                                writeExact(fd_out, done_cmd);
                                break;
                            }

                            if (report.first_submit_ns == 0) {
                                report.first_submit_ns = submit_ns;
                                report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            report.last_submit_ns = submit_ns;

                            iter++;
                            compute.launchTaggedCompute(iter, stream);
                            compute.synchronize(stream);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (latencies_us.size() >= MAX_SAMPLES) throw std::runtime_error("Capacity exhausted");
                                latencies_us.push_back(lat_us);
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                report.late++;
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                appendMeasurementSample(report.late_latencies_us, lat_us);
                            }

                            IpcCommand done_cmd;
                            done_cmd.type = IpcCommand::CMD_COMPUTE_DONE;
                            done_cmd.timestamp_ns = comp_ns;
                            done_cmd.start_ns = submit_ns;
                            done_cmd.iteration = iter;
                            if (!writeExact(fd_out, done_cmd)) break;
                        }
                    } else { // solo-compute or concurrent
                        while (true) {
                            uint64_t submit_ns = getMonotonicTimeNs();
                            if (submit_ns >= deadline_ns) break;

                            if (report.first_submit_ns == 0) {
                                report.first_submit_ns = submit_ns;
                                report.start_skew_ns = (submit_ns > start_ns) ? (submit_ns - start_ns) : 0;
                            }
                            report.last_submit_ns = submit_ns;

                            iter++;
                            compute.launchTaggedCompute(iter, stream);
                            compute.synchronize(stream);
                            uint64_t comp_ns = getMonotonicTimeNs();
                            report.last_completion_ns = comp_ns;

                            double lat_us = (comp_ns - submit_ns) / 1000.0;
                            auto decision = window.evaluateOperation(submit_ns, comp_ns);
                            if (decision == WindowDecision::InWindow) {
                                if (latencies_us.size() >= MAX_SAMPLES) throw std::runtime_error("Capacity exhausted");
                                latencies_us.push_back(lat_us);
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                report.completed++;
                            } else if (decision == WindowDecision::LateDrained) {
                                report.late++;
                                appendMeasurementSample(op_records, {submit_ns, comp_ns});
                                appendMeasurementSample(report.late_latencies_us, lat_us);
                            }
                        }
                    }
                    report.final_tag = iter;
                }

                uint64_t drain_start = getMonotonicTimeNs();
                compute.synchronize(stream);
                uint64_t drain_end = getMonotonicTimeNs();
                uint64_t post_deadline_tail = (report.last_completion_ns > start_cmd.deadline_ns) ?
                    (report.last_completion_ns - start_cmd.deadline_ns) : 0;
                report.drain_ns = post_deadline_tail + (drain_end - drain_start);

                {
                    ScopedNvtx nvtx("Compute_Process_Validation");
                    uint64_t val_start = getMonotonicTimeNs();
                    if (report.final_tag > 0) {
                        if (!compute.verifyTaggedOutput(report.final_tag)) {
                            throw std::runtime_error("Compute process final tag verification failed");
                        }
                    }
                    report.validation_passed = true;
                    report.validation_ns = getMonotonicTimeNs() - val_start;
                }
            }

            if (stream) {
                checkCudaError(cudaStreamDestroy(stream), "cudaStreamDestroy");
                stream = nullptr;
            }
        } else {
            throw std::runtime_error("Unknown worker role: " + report.role);
        }

        if (!latencies_us.empty()) {
            std::sort(latencies_us.begin(), latencies_us.end());
            auto stats = LatencyStats::calculate(latencies_us);
            report.latency_samples = stats.count;
            report.p50_us = stats.p50_us;
            report.p95_us = stats.p95_us;
            report.p99_us = stats.p99_us;
        }

        if (!config.trace_file_path.empty()) {
            dumpOperationTrace(config.trace_file_path, trace_start_ns,
                               trace_deadline_ns, op_records);
        }
        WorkerWireReport wire;
        reportToWire(report, wire);
        writeExact(fd_out, wire);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[InternalWorker Exception] " << e.what() << std::endl;
        WorkerWireReport err_wire;
        std::memset(&err_wire, 0, sizeof(err_wire));
        std::strncpy(err_wire.role, report.role.c_str(), sizeof(err_wire.role) - 1);
        std::strncpy(err_wire.error_text, e.what(), sizeof(err_wire.error_text) - 1);
        err_wire.validation_passed = 0;
        writeExact(fd_out, err_wire);
        return 1;
    }
}

// -------------------------------------------------------------------------------------------------
// Process Topology Coordinator Helpers
// -------------------------------------------------------------------------------------------------

bool ControlledRunner::runProcessTimed() {
    bool need_render = (config_.mode != "solo-compute");
    bool need_compute = (config_.mode != "solo-render");

    int pipe_render_c2w[2] = {-1, -1};
    int pipe_render_w2c[2] = {-1, -1};
    int pipe_compute_c2w[2] = {-1, -1};
    int pipe_compute_w2c[2] = {-1, -1};

    if (need_render) {
        if (pipe(pipe_render_c2w) != 0 || pipe(pipe_render_w2c) != 0) {
            throw std::runtime_error("Failed to create render sync pipes");
        }
    }
    if (need_compute) {
        if (pipe(pipe_compute_c2w) != 0 || pipe(pipe_compute_w2c) != 0) {
            throw std::runtime_error("Failed to create compute sync pipes");
        }
    }

    pid_t render_pid = -1;
    pid_t compute_pid = -1;

    auto cleanup_processes = [&](bool kill_them) {
        if (kill_them) {
            if (render_pid > 0) kill(render_pid, SIGKILL);
            if (compute_pid > 0) kill(compute_pid, SIGKILL);
        }
        if (render_pid > 0) {
            int status = 0;
            waitpid(render_pid, &status, 0);
        }
        if (compute_pid > 0) {
            int status = 0;
            waitpid(compute_pid, &status, 0);
        }
        if (pipe_render_c2w[0] >= 0) close(pipe_render_c2w[0]);
        if (pipe_render_c2w[1] >= 0) close(pipe_render_c2w[1]);
        if (pipe_render_w2c[0] >= 0) close(pipe_render_w2c[0]);
        if (pipe_render_w2c[1] >= 0) close(pipe_render_w2c[1]);
        if (pipe_compute_c2w[0] >= 0) close(pipe_compute_c2w[0]);
        if (pipe_compute_c2w[1] >= 0) close(pipe_compute_c2w[1]);
        if (pipe_compute_w2c[0] >= 0) close(pipe_compute_w2c[0]);
        if (pipe_compute_w2c[1] >= 0) close(pipe_compute_w2c[1]);
    };

    uint64_t bracket_start_ns = getMonotonicTimeNs();

    if (need_render) {
        render_pid = fork();
        if (render_pid < 0) {
            cleanup_processes(true);
            throw std::runtime_error("Fork render worker failed");
        }
        if (render_pid == 0) {
            close(pipe_render_c2w[1]); close(pipe_render_w2c[0]);
            if (need_compute) {
                close(pipe_compute_c2w[0]); close(pipe_compute_c2w[1]);
                close(pipe_compute_w2c[0]); close(pipe_compute_w2c[1]);
            }

            std::vector<std::string> args = {
                binary_path_,
                "--internal-worker-role", "render",
                "--ipc-fd-in", std::to_string(pipe_render_c2w[0]),
                "--ipc-fd-out", std::to_string(pipe_render_w2c[1]),
                "--mode", config_.mode,
                "--topology", config_.topology,
                "--duration-seconds", std::to_string(config_.duration_seconds),
                "--warmup-seconds", std::to_string(config_.warmup_seconds),
                "--width", std::to_string(config_.render_width),
                "--height", std::to_string(config_.render_height),
                "--device", std::to_string(config_.target_device)
            };
            if (!config_.report_json_path.empty()) {
                args.push_back("--trace-file");
                args.push_back(config_.report_json_path + ".render.samples.bin");
            }
            if (config_.render_cpu >= 0) {
                args.push_back("--render-cpu");
                args.push_back(std::to_string(config_.render_cpu));
            }

            std::vector<char*> c_args;
            for (auto& s : args) c_args.push_back(const_cast<char*>(s.c_str()));
            c_args.push_back(nullptr);

            execv(binary_path_.c_str(), c_args.data());
            _exit(127);
        }
        close(pipe_render_c2w[0]);
        close(pipe_render_w2c[1]);
    }

    if (need_compute) {
        compute_pid = fork();
        if (compute_pid < 0) {
            cleanup_processes(true);
            throw std::runtime_error("Fork compute worker failed");
        }
        if (compute_pid == 0) {
            close(pipe_compute_c2w[1]); close(pipe_compute_w2c[0]);
            if (need_render) {
                close(pipe_render_c2w[0]); close(pipe_render_c2w[1]);
                close(pipe_render_w2c[0]); close(pipe_render_w2c[1]);
            }

            std::vector<std::string> args = {
                binary_path_,
                "--internal-worker-role", "compute",
                "--ipc-fd-in", std::to_string(pipe_compute_c2w[0]),
                "--ipc-fd-out", std::to_string(pipe_compute_w2c[1]),
                "--mode", config_.mode,
                "--topology", config_.topology,
                "--duration-seconds", std::to_string(config_.duration_seconds),
                "--warmup-seconds", std::to_string(config_.warmup_seconds),
                "--elements", std::to_string(config_.cuda_elements),
                "--intensity", std::to_string(config_.cuda_intensity),
                "--device", std::to_string(config_.target_device)
            };
            if (!config_.report_json_path.empty()) {
                args.push_back("--trace-file");
                args.push_back(config_.report_json_path + ".compute.samples.bin");
            }
            if (config_.compute_cpu >= 0) {
                args.push_back("--compute-cpu");
                args.push_back(std::to_string(config_.compute_cpu));
            }

            std::vector<char*> c_args;
            for (auto& s : args) c_args.push_back(const_cast<char*>(s.c_str()));
            c_args.push_back(nullptr);

            execv(binary_path_.c_str(), c_args.data());
            _exit(127);
        }
        close(pipe_compute_c2w[0]);
        close(pipe_compute_w2c[1]);
    }

    // Children spawned; wait for READY messages
    IpcCommand render_ready_cmd;
    IpcCommand compute_ready_cmd;

    if (need_render) {
        if (!readExact(pipe_render_w2c[0], render_ready_cmd, 60000) ||
            render_ready_cmd.type != IpcCommand::CMD_READY) {
            cleanup_processes(true);
            return false;
        }
    }
    if (need_compute) {
        if (!readExact(pipe_compute_w2c[0], compute_ready_cmd, 60000) ||
            compute_ready_cmd.type != IpcCommand::CMD_READY) {
            cleanup_processes(true);
            return false;
        }
    }
    uint64_t bracket_end_ns = getMonotonicTimeNs();

    // Enforce strict clock bracketing: READY timestamp must be between bracket_start_ns and bracket_end_ns
    if (need_render) {
        if (render_ready_cmd.timestamp_ns < bracket_start_ns || render_ready_cmd.timestamp_ns > bracket_end_ns) {
            std::cerr << "[ControlledRunner] Rejecting run: render READY timestamp outside coordinator receive bracket" << std::endl;
            cleanup_processes(true);
            return false;
        }
    }
    if (need_compute) {
        if (compute_ready_cmd.timestamp_ns < bracket_start_ns || compute_ready_cmd.timestamp_ns > bracket_end_ns) {
            std::cerr << "[ControlledRunner] Rejecting run: compute READY timestamp outside coordinator receive bracket" << std::endl;
            cleanup_processes(true);
            return false;
        }
    }
    uint64_t now_ns = getMonotonicTimeNs();
    uint64_t planned_start_ns = now_ns + 100000000ULL;
    auto window = MeasurementWindow::fromDurationSeconds(planned_start_ns, config_.duration_seconds);

    IpcCommand start_cmd;
    start_cmd.type = IpcCommand::CMD_START_WINDOW;
    start_cmd.start_ns = window.start_ns;
    start_cmd.deadline_ns = window.deadline_ns;

    if (need_render) writeExact(pipe_render_c2w[1], start_cmd);
    if (need_compute) writeExact(pipe_compute_c2w[1], start_cmd);

    // True per-pair tracking for serial process mode
    struct PairRecord {
        uint64_t submit_ns;
        uint64_t completion_ns;
    };
    std::vector<PairRecord> pair_records;
    if (config_.mode == "serial") pair_records.reserve(5000000);

    if (config_.mode == "serial" && need_render && need_compute) {
        uint64_t iter = 0;
        while (true) {
            uint64_t pair_submit_ns = getMonotonicTimeNs();
            if (pair_submit_ns >= window.deadline_ns) break;

            IpcCommand step_r;
            step_r.type = IpcCommand::CMD_STEP_RENDER;
            step_r.iteration = iter + 1;
            if (!writeExact(pipe_render_c2w[1], step_r)) break;

            IpcCommand r_done;
            if (!readExact(pipe_render_w2c[0], r_done, 30000)) break;
            if (r_done.type != IpcCommand::CMD_RENDER_DONE) break;

            if (getMonotonicTimeNs() >= window.deadline_ns) break;

            IpcCommand step_c;
            step_c.type = IpcCommand::CMD_STEP_COMPUTE;
            step_c.iteration = iter + 1;
            if (!writeExact(pipe_compute_c2w[1], step_c)) break;

            IpcCommand c_done;
            if (!readExact(pipe_compute_w2c[0], c_done, 30000)) break;
            if (c_done.type != IpcCommand::CMD_COMPUTE_DONE) break;

            const auto pair = completedSerialPair(window, r_done, c_done, iter + 1);
            if (!pair) break;
            appendMeasurementSample(pair_records, {pair->submit_ns, pair->completion_ns});
            iter++;
        }
        IpcCommand term;
        term.type = IpcCommand::CMD_TERMINATE;
        writeExact(pipe_render_c2w[1], term);
        writeExact(pipe_compute_c2w[1], term);
    }

    WorkerWireReport render_wire;
    WorkerWireReport compute_wire;
    bool got_render_rep = false;
    bool got_compute_rep = false;

    if (need_render) {
        if (readExact(pipe_render_w2c[0], render_wire, 60000) && render_wire.validation_passed) {
            got_render_rep = true;
        }
    }
    if (need_compute) {
        if (readExact(pipe_compute_w2c[0], compute_wire, 60000) && compute_wire.validation_passed) {
            got_compute_rep = true;
        }
    }

    int render_status = 0, compute_status = 0;
    bool children_ok = true;
    if (render_pid > 0) {
        waitpid(render_pid, &render_status, 0);
        if (!WIFEXITED(render_status) || WEXITSTATUS(render_status) != 0) children_ok = false;
    }
    if (compute_pid > 0) {
        waitpid(compute_pid, &compute_status, 0);
        if (!WIFEXITED(compute_status) || WEXITSTATUS(compute_status) != 0) children_ok = false;
    }

    cleanup_processes(false);

    if (!children_ok || (need_render && !got_render_rep) || (need_compute && !got_compute_rep)) {
        return false;
    }

    RunReport run_report;
    run_report.schema_version = 1;
    run_report.valid = true;
    run_report.mode = config_.mode;
    run_report.topology = config_.topology;
    run_report.validation_strength = "final_tag_only";
    run_report.config = config_;
    run_report.start_ns = window.start_ns;
    run_report.deadline_ns = window.deadline_ns;
    run_report.observed_end_ns = getMonotonicTimeNs();

    double window_duration_sec = window.duration_ns() / 1e9;

    uint64_t req_warmup_ns = static_cast<uint64_t>(config_.warmup_seconds * 1e9);
    if (need_render) {
        WorkerReport wr;
        wireToReport(render_wire, wr);
        if (wr.warmup_completed == 0) return false;
        if (wr.warmup_end_ns < wr.warmup_start_ns + req_warmup_ns) return false;
        if (wr.completed == 0) return false;
        wr.throughput_ops_sec = wr.completed / window_duration_sec;
        if (!config_.report_json_path.empty()) {
            wr.trace.path = config_.report_json_path + ".render.samples.bin";
            wr.trace.count = wr.completed + wr.late;
            wr.trace.format = "exp9ts01_le";
        }
        run_report.workers.push_back(wr);
    }

    if (need_compute) {
        WorkerReport wc;
        wireToReport(compute_wire, wc);
        if (wc.warmup_completed == 0) return false;
        if (wc.warmup_end_ns < wc.warmup_start_ns + req_warmup_ns) return false;
        if (wc.completed == 0) return false;
        wc.throughput_ops_sec = wc.completed / window_duration_sec;
        if (!config_.report_json_path.empty()) {
            wc.trace.path = config_.report_json_path + ".compute.samples.bin";
            wc.trace.count = wc.completed + wc.late;
            wc.trace.format = "exp9ts01_le";
        }
        run_report.workers.push_back(wc);
    }

    // True pair distribution for serial process mode
    if (config_.mode == "serial" && need_render && need_compute) {
        run_report.has_pairs = true;
        std::vector<double> in_window_pair_latencies_us;
        in_window_pair_latencies_us.reserve(pair_records.size());

        for (const auto& rec : pair_records) {
            if (rec.submit_ns >= window.start_ns && rec.completion_ns < window.deadline_ns) {
                in_window_pair_latencies_us.push_back((rec.completion_ns - rec.submit_ns) / 1000.0);
            } else if (rec.submit_ns >= window.start_ns && rec.submit_ns < window.deadline_ns &&
                       rec.completion_ns >= window.deadline_ns) {
                run_report.pairs.late++;
                run_report.pairs.late_latencies_us.push_back((rec.completion_ns - rec.submit_ns) / 1000.0);
            }
        }

        run_report.pairs.completed = in_window_pair_latencies_us.size();
        if (!in_window_pair_latencies_us.empty()) {
            std::sort(in_window_pair_latencies_us.begin(), in_window_pair_latencies_us.end());
            auto pair_stats = LatencyStats::calculate(in_window_pair_latencies_us);
            run_report.pairs.latency_samples = pair_stats.count;
            run_report.pairs.p50_us = pair_stats.p50_us;
            run_report.pairs.p95_us = pair_stats.p95_us;
            run_report.pairs.p99_us = pair_stats.p99_us;
        }
        if (!config_.report_json_path.empty()) {
            std::string trace_path = config_.report_json_path + ".pairs.samples.bin";
            std::vector<OperationRecord> pair_trace_records;
            pair_trace_records.reserve(pair_records.size());
            for (const auto& pr : pair_records) {
                pair_trace_records.push_back({pr.submit_ns, pr.completion_ns});
            }
            dumpOperationTrace(trace_path, window.start_ns, window.deadline_ns, pair_trace_records);
            run_report.pairs.trace.path = trace_path;
            run_report.pairs.trace.count = pair_trace_records.size();
            run_report.pairs.trace.format = "exp9ts01_le";
        }
    }

    writeReportJson(run_report);
    return true;
}

bool ControlledRunner::runProcessValidation() {
    bool need_render = (config_.mode != "solo-compute");
    bool need_compute = (config_.mode != "solo-render");

    int pipe_render_c2w[2] = {-1, -1};
    int pipe_render_w2c[2] = {-1, -1};
    int pipe_compute_c2w[2] = {-1, -1};
    int pipe_compute_w2c[2] = {-1, -1};

    if (need_render) {
        if (pipe(pipe_render_c2w) != 0 || pipe(pipe_render_w2c) != 0) throw std::runtime_error("pipe failed");
    }
    if (need_compute) {
        if (pipe(pipe_compute_c2w) != 0 || pipe(pipe_compute_w2c) != 0) throw std::runtime_error("pipe failed");
    }

    pid_t render_pid = -1, compute_pid = -1;

    auto cleanup = [&](bool kill_them) {
        if (kill_them) {
            if (render_pid > 0) kill(render_pid, SIGKILL);
            if (compute_pid > 0) kill(compute_pid, SIGKILL);
        }
        if (render_pid > 0) { int s = 0; waitpid(render_pid, &s, 0); }
        if (compute_pid > 0) { int s = 0; waitpid(compute_pid, &s, 0); }
        if (pipe_render_c2w[0] >= 0) close(pipe_render_c2w[0]);
        if (pipe_render_c2w[1] >= 0) close(pipe_render_c2w[1]);
        if (pipe_render_w2c[0] >= 0) close(pipe_render_w2c[0]);
        if (pipe_render_w2c[1] >= 0) close(pipe_render_w2c[1]);
        if (pipe_compute_c2w[0] >= 0) close(pipe_compute_c2w[0]);
        if (pipe_compute_c2w[1] >= 0) close(pipe_compute_c2w[1]);
        if (pipe_compute_w2c[0] >= 0) close(pipe_compute_w2c[0]);
        if (pipe_compute_w2c[1] >= 0) close(pipe_compute_w2c[1]);
    };

    if (need_render) {
        render_pid = fork();
        if (render_pid == 0) {
            close(pipe_render_c2w[1]); close(pipe_render_w2c[0]);
            if (need_compute) { close(pipe_compute_c2w[0]); close(pipe_compute_c2w[1]); close(pipe_compute_w2c[0]); close(pipe_compute_w2c[1]); }
            std::vector<std::string> args = {
                binary_path_, "--internal-worker-role", "render",
                "--ipc-fd-in", std::to_string(pipe_render_c2w[0]),
                "--ipc-fd-out", std::to_string(pipe_render_w2c[1]),
                "--mode", config_.mode, "--topology", config_.topology,
                "--validation", "--iterations", std::to_string(config_.iterations),
                "--width", std::to_string(config_.render_width),
                "--height", std::to_string(config_.render_height),
                "--device", std::to_string(config_.target_device)
            };
            if (config_.render_cpu >= 0) { args.push_back("--render-cpu"); args.push_back(std::to_string(config_.render_cpu)); }
            std::vector<char*> c_args;
            for (auto& s : args) c_args.push_back(const_cast<char*>(s.c_str()));
            c_args.push_back(nullptr);
            execv(binary_path_.c_str(), c_args.data());
            _exit(127);
        }
        close(pipe_render_c2w[0]); close(pipe_render_w2c[1]);
    }

    if (need_compute) {
        compute_pid = fork();
        if (compute_pid == 0) {
            close(pipe_compute_c2w[1]); close(pipe_compute_w2c[0]);
            if (need_render) { close(pipe_render_c2w[0]); close(pipe_render_c2w[1]); close(pipe_render_w2c[0]); close(pipe_render_w2c[1]); }
            std::vector<std::string> args = {
                binary_path_, "--internal-worker-role", "compute",
                "--ipc-fd-in", std::to_string(pipe_compute_c2w[0]),
                "--ipc-fd-out", std::to_string(pipe_compute_w2c[1]),
                "--mode", config_.mode, "--topology", config_.topology,
                "--validation", "--iterations", std::to_string(config_.iterations),
                "--elements", std::to_string(config_.cuda_elements),
                "--intensity", std::to_string(config_.cuda_intensity),
                "--device", std::to_string(config_.target_device)
            };
            if (config_.compute_cpu >= 0) { args.push_back("--compute-cpu"); args.push_back(std::to_string(config_.compute_cpu)); }
            std::vector<char*> c_args;
            for (auto& s : args) c_args.push_back(const_cast<char*>(s.c_str()));
            c_args.push_back(nullptr);
            execv(binary_path_.c_str(), c_args.data());
            _exit(127);
        }
        close(pipe_compute_c2w[0]); close(pipe_compute_w2c[1]);
    }

    IpcCommand r_cmd, c_cmd;
    if (need_render && (!readExact(pipe_render_w2c[0], r_cmd, 60000) || r_cmd.type != IpcCommand::CMD_READY)) { cleanup(true); return false; }
    if (need_compute && (!readExact(pipe_compute_w2c[0], c_cmd, 60000) || c_cmd.type != IpcCommand::CMD_READY)) { cleanup(true); return false; }

    uint64_t start_ns = getMonotonicTimeNs();
    IpcCommand start_cmd;
    start_cmd.type = IpcCommand::CMD_START_VALIDATION;
    if (need_render) writeExact(pipe_render_c2w[1], start_cmd);
    if (need_compute) writeExact(pipe_compute_c2w[1], start_cmd);

    WorkerWireReport r_wire, c_wire;
    bool r_ok = false, c_ok = false;
    if (need_render && readExact(pipe_render_w2c[0], r_wire, 60000) && r_wire.validation_passed) r_ok = true;
    if (need_compute && readExact(pipe_compute_w2c[0], c_wire, 60000) && c_wire.validation_passed) c_ok = true;

    int rs = 0, cs = 0;
    if (render_pid > 0) waitpid(render_pid, &rs, 0);
    if (compute_pid > 0) waitpid(compute_pid, &cs, 0);
    cleanup(false);

    if ((need_render && !r_ok) || (need_compute && !c_ok)) return false;

    RunReport run_report;
    run_report.schema_version = 1;
    run_report.valid = true;
    run_report.mode = config_.mode;
    run_report.topology = config_.topology;
    run_report.validation_strength = "per_iteration";
    run_report.config = config_;
    run_report.start_ns = start_ns;
    run_report.deadline_ns = start_ns + static_cast<uint64_t>(config_.duration_seconds * 1e9);
    run_report.observed_end_ns = getMonotonicTimeNs();

    if (need_render) { WorkerReport w; wireToReport(r_wire, w); run_report.workers.push_back(w); }
    if (need_compute) { WorkerReport w; wireToReport(c_wire, w); run_report.workers.push_back(w); }

    writeReportJson(run_report);
    return true;
}
