#pragma once

#include <cuda_runtime.h>
#include <GL/gl.h>

#include <cstddef>
#include <vector>
#include <string>

struct CudaDeviceSpecs {
    int device_id;
    std::string name;
    int major;
    int minor;
    int sm_count;
    size_t total_memory_bytes;
    int pci_bus_id;
    int pci_device_id;
};

class CudaCompute {
public:
    CudaCompute(size_t num_elements = 262144, int compute_intensity = 100);
    ~CudaCompute();

    bool initialize(int target_device = 0);
    void launchCompute(cudaStream_t stream = nullptr);
    void synchronize(cudaStream_t stream = nullptr);
    bool verifyOutput();

    // Probe CUDA-OpenGL interop support for device-resident memory handoff
    bool testOpenGLInterop(GLuint rbo_color, int width, int height);

    const CudaDeviceSpecs& getSpecs() const { return specs_; }
    size_t getElementCount() const { return num_elements_; }
    int getComputeIntensity() const { return compute_intensity_; }

    void teardown();

private:
    size_t num_elements_;
    int compute_intensity_;
    int target_device_;
    CudaDeviceSpecs specs_;

    float* d_in_;
    float* d_out_;
    float* h_in_;
    float* h_out_;

    bool initialized_;
};
