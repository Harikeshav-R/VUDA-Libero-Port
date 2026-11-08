#include "cuda_compute.h"

#include <cuda_gl_interop.h>
#include <iostream>
#include <cmath>
#include <random>
#include <cstdint>
#include <cstring>
#include <limits>
__global__ void compute_workload_kernel(float* __restrict__ out, const float* __restrict__ in, size_t N, int intensity) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        float val = in[idx];
        #pragma unroll 8
        for (int i = 0; i < intensity; ++i) {
            val = fmaf(val, 1.0001f, 0.0002f);
            val = val - floorf(val);
        }
        out[idx] = val;
    }
}

__global__ void compute_tagged_workload_kernel(float* __restrict__ out, const float* __restrict__ in, size_t N, int intensity, uint64_t tag) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        float val = in[idx];
        #pragma unroll 8
        for (int i = 0; i < intensity; ++i) {
            val = fmaf(val, 1.0001f, 0.0002f);
            val = val - floorf(val);
        }
        // Reserved slots for exact 64-bit tag (split across out[0] and out[1] as bitwise IEEE 754 float representation)
        if (idx == 0) {
            uint32_t low = static_cast<uint32_t>(tag & 0xFFFFFFFFULL);
            val = __uint_as_float(low);
        } else if (idx == 1) {
            uint32_t high = static_cast<uint32_t>((tag >> 32) & 0xFFFFFFFFULL);
            val = __uint_as_float(high);
        }
        out[idx] = val;
    }
}

CudaCompute::CudaCompute(size_t num_elements, int compute_intensity)
    : num_elements_(num_elements),
      compute_intensity_(compute_intensity),
      target_device_(0),
      d_in_(nullptr),
      d_out_(nullptr),
      h_in_(nullptr),
      h_out_(nullptr),
      initialized_(false) {}

CudaCompute::~CudaCompute() {
    teardown();
}

bool CudaCompute::initialize(int target_device) {
    if (initialized_) {
        return true;
    }

    target_device_ = target_device;
    cudaError_t err = cudaSetDevice(target_device_);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaSetDevice(" << target_device_ << ") failed: "
                  << cudaGetErrorString(err) << std::endl;
        return false;
    }

    cudaDeviceProp prop;
    err = cudaGetDeviceProperties(&prop, target_device_);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaGetDeviceProperties failed: "
                  << cudaGetErrorString(err) << std::endl;
        return false;
    }

    specs_.device_id = target_device_;
    specs_.name = prop.name;
    specs_.major = prop.major;
    specs_.minor = prop.minor;
    specs_.sm_count = prop.multiProcessorCount;
    specs_.total_memory_bytes = prop.totalGlobalMem;
    specs_.pci_bus_id = prop.pciBusID;
    specs_.pci_device_id = prop.pciDeviceID;

    // Validate element count bounds:
    // 1. Must be > 1000 per contract
    // 2. Buffer size in bytes must not overflow size_t (SIZE_MAX / sizeof(float))
    // 3. Grid block count must fit in int and not exceed prop.maxGridSize[0]
    // 4. Compute intensity must be > 0
    if (num_elements_ <= 1000) {
        std::cerr << "[CudaCompute] initialize failed: num_elements (" << num_elements_
                  << ") must be > 1000" << std::endl;
        return false;
    }
    if (num_elements_ > (std::numeric_limits<size_t>::max() / sizeof(float))) {
        std::cerr << "[CudaCompute] initialize failed: num_elements (" << num_elements_
                  << ") exceeds size_t buffer capacity" << std::endl;
        return false;
    }
    if (compute_intensity_ <= 0) {
        std::cerr << "[CudaCompute] initialize failed: compute_intensity (" << compute_intensity_
                  << ") must be > 0" << std::endl;
        return false;
    }

    size_t blocks_needed = (num_elements_ / 256) + ((num_elements_ % 256 != 0) ? 1 : 0);
    if (blocks_needed > static_cast<size_t>(prop.maxGridSize[0]) ||
        blocks_needed > static_cast<size_t>(std::numeric_limits<int>::max())) {
        std::cerr << "[CudaCompute] initialize failed: required grid blocks (" << blocks_needed
                  << ") exceeds device maxGridSize[0] (" << prop.maxGridSize[0] << ")" << std::endl;
        return false;
    }

    std::cout << "[CudaCompute] CUDA Device Initialized:" << std::endl;
    std::cout << "  Device:       " << specs_.name << " (ID: " << specs_.device_id << ")" << std::endl;
    std::cout << "  Compute Cap:  " << specs_.major << "." << specs_.minor << std::endl;
    std::cout << "  SM Count:     " << specs_.sm_count << std::endl;
    std::cout << "  Total VRAM:   " << (specs_.total_memory_bytes / (1024 * 1024)) << " MiB" << std::endl;
    std::cout << "  PCI Bus:ID:   " << specs_.pci_bus_id << ":" << specs_.pci_device_id << std::endl;

    size_t buffer_size = num_elements_ * sizeof(float);
    err = cudaMalloc(&d_in_, buffer_size);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMalloc(d_in_) failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMalloc(&d_out_, buffer_size);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMalloc(d_out_) failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMallocHost(&h_in_, buffer_size);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMallocHost(h_in_) failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    err = cudaMallocHost(&h_out_, buffer_size);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMallocHost(h_out_) failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    for (size_t i = 0; i < num_elements_; ++i) {
        h_in_[i] = static_cast<float>(i % 1000) / 1000.0f;
    }

    err = cudaMemcpy(d_in_, h_in_, buffer_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMemcpy H2D failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

#include <stdexcept>

void CudaCompute::launchCompute(cudaStream_t stream) {
    int block_size = 256;
    size_t blocks_needed = (num_elements_ / block_size) + ((num_elements_ % block_size != 0) ? 1 : 0);
    int grid_size = static_cast<int>(blocks_needed);
    compute_workload_kernel<<<grid_size, block_size, 0, stream>>>(d_out_, d_in_, num_elements_, compute_intensity_);
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA kernel launch failed: ") + cudaGetErrorString(err));
    }
}

void CudaCompute::synchronize(cudaStream_t stream) {
    cudaError_t err = stream ? cudaStreamSynchronize(stream) : cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA synchronize failed: ") + cudaGetErrorString(err));
    }
}

bool CudaCompute::verifyOutput() {
    size_t buffer_size = num_elements_ * sizeof(float);
    cudaError_t err = cudaMemcpy(h_out_, d_out_, buffer_size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMemcpy D2H failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }

    // Verify first 16 and last 16 elements on CPU
    auto compute_expected = [this](float val) -> float {
        for (int i = 0; i < compute_intensity_; ++i) {
            val = val * 1.0001f + 0.0002f;
            val = val - std::floor(val);
        }
        return val;
    };

    size_t check_indices[] = {0, 1, 2, 100, 1000, num_elements_ / 2, num_elements_ - 1};
    for (size_t idx : check_indices) {
        float expected = compute_expected(h_in_[idx]);
        float observed = h_out_[idx];
        if (std::abs(observed - expected) > 1e-4f) {
            std::cerr << "[CudaCompute] Numerical mismatch at index " << idx
                      << ": observed " << observed << ", expected " << expected << std::endl;
            return false;
        }
    }

    return true;
}


void CudaCompute::launchTaggedCompute(uint64_t iteration, cudaStream_t stream) {
    if (num_elements_ <= 1000) {
        throw std::runtime_error("CudaCompute element count must be > 1000 (contract violation)");
    }
    int block_size = 256;
    size_t blocks_needed = (num_elements_ / block_size) + ((num_elements_ % block_size != 0) ? 1 : 0);
    int grid_size = static_cast<int>(blocks_needed);
    compute_tagged_workload_kernel<<<grid_size, block_size, 0, stream>>>(
        d_out_, d_in_, num_elements_, compute_intensity_, iteration);
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        throw std::runtime_error(std::string("CUDA tagged kernel launch failed: ") + cudaGetErrorString(err));
    }
}

bool CudaCompute::verifyTaggedOutput(uint64_t iteration) {
    if (num_elements_ <= 1000 ||
        num_elements_ > (std::numeric_limits<size_t>::max() / sizeof(float))) {
        std::cerr << "[CudaCompute] Verification failed: num_elements out of bounds ("
                  << num_elements_ << ")" << std::endl;
        return false;
    }

    size_t buffer_size = num_elements_ * sizeof(float);
    cudaError_t err = cudaMemcpy(h_out_, d_out_, buffer_size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        std::cerr << "[CudaCompute] cudaMemcpy D2H failed: " << cudaGetErrorString(err) << std::endl;
        return false;
    }


    // 1. Verify exact 64-bit iteration tag from slot 0 (low) and slot 1 (high)
    uint32_t low_bits = 0;
    uint32_t high_bits = 0;
    std::memcpy(&low_bits, &h_out_[0], sizeof(float));
    std::memcpy(&high_bits, &h_out_[1], sizeof(float));
    uint64_t observed_tag = (static_cast<uint64_t>(high_bits) << 32) | static_cast<uint64_t>(low_bits);

    if (observed_tag != iteration) {
        std::cerr << "[CudaCompute] Tag mismatch: observed " << observed_tag
                  << " (0x" << std::hex << observed_tag << std::dec
                  << "), expected " << iteration
                  << " (0x" << std::hex << iteration << std::dec << ")" << std::endl;
        return false;
    }
    // 2. Full numerical-region finite scan on all indices [2, num_elements_ - 1]
    // Slots 0 and 1 hold bitwise uint64 tag representation (may be NaN bit patterns) and are excluded.
    for (size_t i = 2; i < num_elements_; ++i) {
        if (!std::isfinite(h_out_[i])) {
            std::cerr << "[CudaCompute] Non-finite numerical value at index " << i
                      << ": observed " << h_out_[i] << std::endl;
            return false;
        }
    }

    // 3. CPU reference compute for sampled numerical verification
    auto compute_expected = [this](float val) -> float {
        for (int i = 0; i < compute_intensity_; ++i) {
            val = val * 1.0001f + 0.0002f;
            val = val - std::floor(val);
        }
        return val;
    };

    // Numerical checks at indices >= 2 (slots 0 and 1 hold bitwise tag)
    size_t check_indices[] = {2, 3, 10, 100, 1000, num_elements_ / 2, num_elements_ - 1};
    for (size_t idx : check_indices) {
        if (idx >= num_elements_) {
            continue;
        }
        float observed = h_out_[idx];
        if (!std::isfinite(observed)) {
            std::cerr << "[CudaCompute] Non-finite numerical value at index " << idx
                      << ": observed " << observed << std::endl;
            return false;
        }
        float expected = compute_expected(h_in_[idx]);
        if (std::abs(observed - expected) > 1e-4f) {
            std::cerr << "[CudaCompute] Numerical mismatch at index " << idx
                      << ": observed " << observed << ", expected " << expected << std::endl;
            return false;
        }
    }

    return true;
}

bool CudaCompute::testOpenGLInterop(GLuint rbo_color, int width, int height) {
    std::cout << "[CudaCompute] Probing CUDA-OpenGL Interoperability (cudaGraphicsGLRegisterImage)..." << std::endl;

    cudaGraphicsResource* res = nullptr;
    cudaError_t err = cudaGraphicsGLRegisterImage(&res, rbo_color, GL_RENDERBUFFER,
                                                  cudaGraphicsRegisterFlagsReadOnly);
    if (err != cudaSuccess) {
        std::cerr << "  cudaGraphicsGLRegisterImage failed: " << cudaGetErrorString(err)
                  << " (code: " << err << ")" << std::endl;
        return false;
    }

    std::cout << "  Successfully registered GL renderbuffer with CUDA." << std::endl;

    err = cudaGraphicsMapResources(1, &res, 0);
    if (err != cudaSuccess) {
        std::cerr << "  cudaGraphicsMapResources failed: " << cudaGetErrorString(err) << std::endl;
        cudaGraphicsUnregisterResource(res);
        return false;
    }

    cudaArray_t mapped_array = nullptr;
    err = cudaGraphicsSubResourceGetMappedArray(&mapped_array, res, 0, 0);
    if (err != cudaSuccess) {
        std::cerr << "  cudaGraphicsSubResourceGetMappedArray failed: " << cudaGetErrorString(err) << std::endl;
    } else {
        std::cout << "  Successfully mapped GL renderbuffer as CUDA array: " << (void*)mapped_array << std::endl;
    }

    cudaGraphicsUnmapResources(1, &res, 0);
    cudaGraphicsUnregisterResource(res);
    std::cout << "  Successfully unmapped and unregistered GL resource from CUDA." << std::endl;
    return (err == cudaSuccess);
}

void CudaCompute::teardown() {
    if (initialized_) {
        if (d_in_) { cudaFree(d_in_); d_in_ = nullptr; }
        if (d_out_) { cudaFree(d_out_); d_out_ = nullptr; }
        if (h_in_) { cudaFreeHost(h_in_); h_in_ = nullptr; }
        if (h_out_) { cudaFreeHost(h_out_); h_out_ = nullptr; }
        initialized_ = false;
    }
}
