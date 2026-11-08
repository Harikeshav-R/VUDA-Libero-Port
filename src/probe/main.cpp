#include "benchmark_runner.h"
#include "controlled_runner.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <csignal>

int main(int argc, char** argv) {
    // Let checked IPC writes report EPIPE so peer cleanup runs instead of SIGPIPE termination.
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        std::cerr << "Failed to configure checked IPC broken-pipe handling" << std::endl;
        return 1;
    }
    ControlledConfig ctrl_cfg;
    BenchmarkConfig hist_cfg;

    bool has_mode = false;
    bool has_topology = false;
    bool has_internal_worker = false;

    // Default configuration values
    ctrl_cfg.mode = "";
    ctrl_cfg.topology = "threads";
    ctrl_cfg.duration_seconds = 15.0;
    ctrl_cfg.warmup_seconds = 5.0;
    ctrl_cfg.validation = false;
    ctrl_cfg.iterations = 0;
    ctrl_cfg.render_width = 128;
    ctrl_cfg.render_height = 128;
    ctrl_cfg.cuda_elements = 262144;
    ctrl_cfg.cuda_intensity = 100;
    ctrl_cfg.target_device = 0;
    ctrl_cfg.render_cpu = -1;
    ctrl_cfg.compute_cpu = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            ctrl_cfg.mode = argv[++i];
            has_mode = true;
        } else if (arg == "--topology" && i + 1 < argc) {
            ctrl_cfg.topology = argv[++i];
            has_topology = true;
        } else if (arg == "--duration-seconds" && i + 1 < argc) {
            ctrl_cfg.duration_seconds = std::strtod(argv[++i], nullptr);
        } else if (arg == "--warmup-seconds" && i + 1 < argc) {
            ctrl_cfg.warmup_seconds = std::strtod(argv[++i], nullptr);
        } else if (arg == "--validation") {
            ctrl_cfg.validation = true;
        } else if (arg == "--iterations" && i + 1 < argc) {
            ctrl_cfg.iterations = std::atoi(argv[++i]);
            hist_cfg.iterations = ctrl_cfg.iterations;
        } else if (arg == "--warmup" && i + 1 < argc) {
            hist_cfg.warmup = std::atoi(argv[++i]);
        } else if (arg == "--report-json" && i + 1 < argc) {
            ctrl_cfg.report_json_path = argv[++i];
        } else if (arg == "--render-cpu" && i + 1 < argc) {
            ctrl_cfg.render_cpu = std::atoi(argv[++i]);
        } else if (arg == "--compute-cpu" && i + 1 < argc) {
            ctrl_cfg.compute_cpu = std::atoi(argv[++i]);
        } else if (arg == "--device" && i + 1 < argc) {
            ctrl_cfg.target_device = std::atoi(argv[++i]);
            hist_cfg.target_device = ctrl_cfg.target_device;
        } else if (arg == "--intensity" && i + 1 < argc) {
            ctrl_cfg.cuda_intensity = std::atoi(argv[++i]);
            hist_cfg.cuda_intensity = ctrl_cfg.cuda_intensity;
        } else if (arg == "--elements" && i + 1 < argc) {
            ctrl_cfg.cuda_elements = std::strtoul(argv[++i], nullptr, 10);
            hist_cfg.cuda_elements = ctrl_cfg.cuda_elements;
        } else if (arg == "--width" && i + 1 < argc) {
            ctrl_cfg.render_width = std::atoi(argv[++i]);
            hist_cfg.render_width = ctrl_cfg.render_width;
        } else if (arg == "--height" && i + 1 < argc) {
            ctrl_cfg.render_height = std::atoi(argv[++i]);
            hist_cfg.render_height = ctrl_cfg.render_height;
        } else if (arg == "--internal-worker-role" && i + 1 < argc) {
            ctrl_cfg.internal_worker_role = argv[++i];
            has_internal_worker = true;
        } else if (arg == "--ipc-fd-in" && i + 1 < argc) {
            ctrl_cfg.ipc_sync_fd_in = std::atoi(argv[++i]);
        } else if (arg == "--ipc-fd-out" && i + 1 < argc) {
            ctrl_cfg.ipc_sync_fd_out = std::atoi(argv[++i]);
        } else if (arg == "--trace-file" && i + 1 < argc) {
            ctrl_cfg.trace_file_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --mode solo-render|solo-compute|serial|concurrent|historical\n"
                      << "  --topology threads|processes (default: threads)\n"
                      << "  --duration-seconds FLOAT (default: 15.0)\n"
                      << "  --warmup-seconds FLOAT (default: 5.0)\n"
                      << "  --validation (strict per-iteration validation path)\n"
                      << "  --iterations N (finite count for validation or historical)\n"
                      << "  --report-json PATH (write output report JSON)\n"
                      << "  --render-cpu INT (pin render worker)\n"
                      << "  --compute-cpu INT (pin compute worker)\n"
                      << "  --elements N (CUDA array size, default: 262144)\n"
                      << "  --intensity N (CUDA compute intensity, default: 100)\n"
                      << "  --width N (render width >= 16, default: 128)\n"
                      << "  --height N (render height >= 16, default: 128)\n"
                      << "  --device ID (CUDA/EGL device index, default: 0)\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    // Internal subprocess worker entry
    if (has_internal_worker) {
        return ControlledRunner::runInternalWorker(ctrl_cfg);
    }

    // Historical mode branch
    if (ctrl_cfg.mode == "historical") {
        try {
            BenchmarkRunner runner(hist_cfg);
            if (!runner.setup()) {
                std::cerr << "Historical probe setup failed!" << std::endl;
                return 1;
            }
            runner.runAll();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "[BenchmarkRunner Historical Error] " << e.what() << std::endl;
            return 1;
        }
    }

    // Contract validation checks for CLI arguments
    if (!has_mode) {
        std::cerr << "CLI Error: --mode is required (solo-render, solo-compute, serial, concurrent, historical)" << std::endl;
        return 1;
    }

    if (ctrl_cfg.mode != "solo-render" &&
        ctrl_cfg.mode != "solo-compute" &&
        ctrl_cfg.mode != "serial" &&
        ctrl_cfg.mode != "concurrent") {
        std::cerr << "CLI Error: Invalid --mode: " << ctrl_cfg.mode << std::endl;
        return 1;
    }

    if (ctrl_cfg.topology != "threads" && ctrl_cfg.topology != "processes") {
        std::cerr << "CLI Error: Invalid --topology: " << ctrl_cfg.topology << std::endl;
        return 1;
    }

    if (!std::isfinite(ctrl_cfg.duration_seconds) || ctrl_cfg.duration_seconds <= 0.0) {
        std::cerr << "CLI Error: --duration-seconds must be finite and positive" << std::endl;
        return 1;
    }

    if (!std::isfinite(ctrl_cfg.warmup_seconds) || ctrl_cfg.warmup_seconds <= 0.0) {
        std::cerr << "CLI Error: --warmup-seconds must be finite and positive" << std::endl;
        return 1;
    }

    // Preflight duration and warmup nanosecond conversion before spawning any threads/workers
    try {
        uint64_t now_ns = getMonotonicTimeNs();
        MeasurementWindow::fromDurationSeconds(now_ns, ctrl_cfg.duration_seconds);
        MeasurementWindow::fromDurationSeconds(now_ns, ctrl_cfg.warmup_seconds);
    } catch (const std::exception& e) {
        std::cerr << "CLI Error: duration or warmup validation failed: " << e.what() << std::endl;
        return 1;
    }

    if (ctrl_cfg.cuda_elements <= 1000) {
        std::cerr << "CLI Error: --elements must be > 1000 (got " << ctrl_cfg.cuda_elements << ")" << std::endl;
        return 1;
    }

    if (ctrl_cfg.cuda_intensity <= 0) {
        std::cerr << "CLI Error: --intensity must be > 0 (got " << ctrl_cfg.cuda_intensity << ")" << std::endl;
        return 1;
    }

    if (ctrl_cfg.render_width < 16 || ctrl_cfg.render_height < 16) {
        std::cerr << "CLI Error: render dimensions too small for tag (width and height must be >= 16)" << std::endl;
        return 1;
    }

    if (ctrl_cfg.validation && ctrl_cfg.iterations <= 0) {
        std::cerr << "CLI Error: --iterations must be > 0 when --validation is enabled" << std::endl;
        return 1;
    }

    try {
        ControlledRunner runner(ctrl_cfg);
        if (!runner.run()) {
            std::cerr << "[ControlledRunner] Run failed" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ControlledRunner Fatal Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
