#include "benchmark_runner.h"

#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char** argv) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            config.iterations = std::atoi(argv[++i]);
        } else if (arg == "--warmup" && i + 1 < argc) {
            config.warmup = std::atoi(argv[++i]);
        } else if (arg == "--device" && i + 1 < argc) {
            config.target_device = std::atoi(argv[++i]);
        } else if (arg == "--intensity" && i + 1 < argc) {
            config.cuda_intensity = std::atoi(argv[++i]);
        } else if (arg == "--elements" && i + 1 < argc) {
            config.cuda_elements = std::strtoul(argv[++i], nullptr, 10);
        } else if (arg == "--width" && i + 1 < argc) {
            config.render_width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.render_height = std::atoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --iterations N   Benchmark iterations (default: 500)\n"
                      << "  --warmup N       Warmup iterations (default: 50)\n"
                      << "  --device ID      CUDA/EGL device index (default: 0)\n"
                      << "  --intensity N    CUDA kernel arithmetic loop iterations (default: 100)\n"
                      << "  --elements N     CUDA float array size (default: 262144)\n"
                      << "  --width N        FBO width (default: 128)\n"
                      << "  --height N       FBO height (default: 128)\n"
                      << "  --help           Show this help message\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }

    try {
        BenchmarkRunner runner(config);
        if (!runner.setup()) {
            std::cerr << "Probe setup failed!" << std::endl;
            return 1;
        }

        runner.runAll();
    } catch (const std::exception& e) {
        std::cerr << "[BenchmarkRunner] Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
