#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct DeviceIdentityMatch {
    bool matched = false;
    std::string failure_reason;
    std::string cuda_uuid_hex;
    std::string egl_uuid_hex;
    std::string pci_bus_id;
    int target_device = 0;
};

// Strict physical device identity guard:
// 1. Verifies target_cuda_device is 0 and exactly 1 CUDA device is visible.
// 2. Queries CUDA UUID and PCI bus ID via cudaDeviceGetPCIBusId.
// 3. Verifies against EXP009_IDENTITY_FILE if set (fail-closed if missing/mismatch).
// 4. Queries EGL devices using eglQueryDeviceBinaryEXT (EGL_DEVICE_UUID_EXT) for the EXACT
//    device that EGLRenderer::initialize selects, matching 16-byte UUID directly.
DeviceIdentityMatch verifyPhysicalDeviceIdentity(int target_cuda_device);
