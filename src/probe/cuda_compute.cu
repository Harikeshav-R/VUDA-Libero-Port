#include "cuda_compute.h"

#include <cuda_gl_interop.h>
#include <iostream>
#include <cmath>
#include <random>

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
    int grid_size = (num_elements_ + block_size - 1) / block_size;
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
