#include "device_identity_guard.h"

#include <cuda_runtime.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cctype>

#ifndef EGL_DEVICE_UUID_EXT
#define EGL_DEVICE_UUID_EXT 0x335C
#endif

#ifndef EGL_CUDA_DEVICE_NV
#define EGL_CUDA_DEVICE_NV 0x323A
#endif

typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYDEVICEBINARYEXTPROC_LOCAL) (
    EGLDeviceEXT device,
    EGLint name,
    EGLint max_size,
    void *value,
    EGLint *size
);

static std::string formatBytesHex(const uint8_t* bytes, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return oss.str();
}

static std::string normalizeHex(const std::string& input) {
    std::string s;
    for (char c : input) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return s;
}

static std::string normalizePci(const std::string& input) {
    std::string s;
    for (char c : input) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return s;
}

DeviceIdentityMatch verifyPhysicalDeviceIdentity(int target_cuda_device) {
    DeviceIdentityMatch result;
    result.target_device = target_cuda_device;

    // 1. Require target_cuda_device == 0 (single allocated compute-node device contract)
    if (target_cuda_device != 0) {
        result.failure_reason = "Contract requires exactly target_device 0 in allocated container/job (got " +
                                std::to_string(target_cuda_device) + ")";
        return result;
    }

    // 2. Query CUDA device count; must be exactly 1 visible allocated device
    int device_count = 0;
    cudaError_t cuda_err = cudaGetDeviceCount(&device_count);
    if (cuda_err != cudaSuccess || device_count <= 0) {
        result.failure_reason = "CUDA get device count failed or no devices: " +
                                std::string(cudaGetErrorString(cuda_err));
        return result;
    }

    if (device_count != 1) {
        result.failure_reason = "Allocated grant must expose exactly 1 CUDA-visible device; observed " +
                                std::to_string(device_count);
        return result;
    }

    cudaDeviceProp prop;
    cuda_err = cudaGetDeviceProperties(&prop, 0);
    if (cuda_err != cudaSuccess) {
        result.failure_reason = "CUDA get device properties failed for device 0: " +
                                std::string(cudaGetErrorString(cuda_err));
        return result;
    }

    uint8_t cuda_uuid[16];
    std::memcpy(cuda_uuid, prop.uuid.bytes, 16);
    result.cuda_uuid_hex = formatBytesHex(cuda_uuid, 16);

    char pci_buf[64] = {0};
    cuda_err = cudaDeviceGetPCIBusId(pci_buf, sizeof(pci_buf), 0);
    if (cuda_err != cudaSuccess) {
        result.failure_reason = "cudaDeviceGetPCIBusId failed: " + std::string(cudaGetErrorString(cuda_err));
        return result;
    }
    result.pci_bus_id = pci_buf;

    // 3. Strict EXP009_IDENTITY_FILE verification (fail-closed if specified)
    const char* env_identity_file = std::getenv("EXP009_IDENTITY_FILE");
    if (env_identity_file && std::strlen(env_identity_file) > 0) {
        std::ifstream f(env_identity_file);
        if (!f.is_open()) {
            result.failure_reason = "EXP009_IDENTITY_FILE specified (" + std::string(env_identity_file) +
                                    ") but could not be opened";
            return result;
        }

        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        std::string norm_content = normalizeHex(content);
        std::string norm_uuid = normalizeHex(result.cuda_uuid_hex);
        if (norm_content.find(norm_uuid) == std::string::npos) {
            result.failure_reason = "EXP009_IDENTITY_FILE does not match CUDA UUID " + result.cuda_uuid_hex;
            return result;
        }

        std::string pci_clean = normalizePci(result.pci_bus_id);
        std::string content_clean = normalizePci(content);
        if (content_clean.find(pci_clean) == std::string::npos) {
            result.failure_reason = "EXP009_IDENTITY_FILE does not match CUDA PCI " + result.pci_bus_id;
            return result;
        }
    }

    // 4. Query EGL devices and resolve the EXACT device chosen by EGLRenderer::initialize(0)
    auto pQueryDevices = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
    if (!pQueryDevices) {
        result.failure_reason = "eglQueryDevicesEXT not available";
        return result;
    }

    EGLint num_devices = 0;
    if (!pQueryDevices(0, nullptr, &num_devices) || num_devices <= 0) {
        result.failure_reason = "No EGL devices reported by eglQueryDevicesEXT";
        return result;
    }

    std::vector<EGLDeviceEXT> devices(num_devices);
    if (!pQueryDevices(num_devices, devices.data(), &num_devices)) {
        result.failure_reason = "Failed to retrieve EGL devices list";
        return result;
    }

    auto pQueryDeviceBinary = (PFNEGLQUERYDEVICEBINARYEXTPROC_LOCAL)eglGetProcAddress("eglQueryDeviceBinaryEXT");
    if (!pQueryDeviceBinary) {
        result.failure_reason = "eglQueryDeviceBinaryEXT not available; cannot query EGL_DEVICE_UUID_EXT";
        return result;
    }

    auto pQueryDeviceAttrib = (PFNEGLQUERYDEVICEATTRIBEXTPROC)eglGetProcAddress("eglQueryDeviceAttribEXT");

    // Mirror EGLRenderer::initialize exact selection logic:
    // First, search for dev.cuda_device_ordinal == target_cuda_device (0).
    // If not found, use devices[0].handle.
    EGLDeviceEXT chosen_device = EGL_NO_DEVICE_EXT;
    int chosen_idx = -1;

    if (pQueryDeviceAttrib) {
        for (int i = 0; i < num_devices; ++i) {
            EGLAttrib cuda_dev = -1;
            if (pQueryDeviceAttrib(devices[i], EGL_CUDA_DEVICE_NV, &cuda_dev)) {
                if (static_cast<int>(cuda_dev) == 0) {
                    chosen_device = devices[i];
                    chosen_idx = i;
                    break;
                }
            }
        }
    }

    if (chosen_device == EGL_NO_DEVICE_EXT && !devices.empty()) {
        chosen_device = devices[0];
        chosen_idx = 0;
    }

    if (chosen_device == EGL_NO_DEVICE_EXT) {
        result.failure_reason = "No candidate EGL device could be selected";
        return result;
    }

    // Now query the UUID of THAT chosen_device specifically
    uint8_t egl_uuid[16] = {0};
    EGLint size = 0;
    if (!pQueryDeviceBinary(chosen_device, EGL_DEVICE_UUID_EXT, sizeof(egl_uuid), egl_uuid, &size) || size != 16) {
        result.failure_reason = "eglQueryDeviceBinaryEXT failed to retrieve 16-byte UUID for chosen EGL device " +
                                std::to_string(chosen_idx);
        return result;
    }

    result.egl_uuid_hex = formatBytesHex(egl_uuid, 16);

    // Exact 16-byte physical UUID match required
    if (std::memcmp(cuda_uuid, egl_uuid, 16) != 0) {
        result.failure_reason = "Physical device identity mismatch: chosen EGL device " +
                                std::to_string(chosen_idx) + " UUID " + result.egl_uuid_hex +
                                " does NOT match CUDA UUID " + result.cuda_uuid_hex;
        return result;
    }

    result.matched = true;
    return result;
}
