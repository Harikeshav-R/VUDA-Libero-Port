#include "egl_renderer.h"

#include <iostream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <vector>

EGLRenderer::EGLRenderer(int width, int height)
    : width_(width),
      height_(height),
      display_(EGL_NO_DISPLAY),
      context_(EGL_NO_CONTEXT),
      fbo_(0),
      rbo_color_(0),
      rbo_depth_(0),
      program_(0),
      vao_(0),
      vbo_(0),
      initialized_(false) {}

EGLRenderer::~EGLRenderer() {
    teardown();
}

std::vector<EGLDeviceInfo> EGLRenderer::enumerateDevices() {
    std::vector<EGLDeviceInfo> results;

    auto pQueryDevices = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
    if (!pQueryDevices) {
        std::cerr << "[EGLRenderer] eglQueryDevicesEXT not available" << std::endl;
        return results;
    }

    auto pQueryDeviceString = (PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT");
    auto pQueryDeviceAttrib = (PFNEGLQUERYDEVICEATTRIBEXTPROC)eglGetProcAddress("eglQueryDeviceAttribEXT");

    EGLint num_devices = 0;
    if (!pQueryDevices(0, nullptr, &num_devices) || num_devices <= 0) {
        std::cerr << "[EGLRenderer] No EGL devices reported" << std::endl;
        return results;
    }

    std::vector<EGLDeviceEXT> devices(num_devices);
    if (!pQueryDevices(num_devices, devices.data(), &num_devices)) {
        std::cerr << "[EGLRenderer] Failed to retrieve EGL devices" << std::endl;
        return results;
    }

    for (int i = 0; i < num_devices; ++i) {
        EGLDeviceInfo info;
        info.index = i;
        info.handle = devices[i];
        info.cuda_device_ordinal = -1;

        if (pQueryDeviceString) {
            const char* ext = pQueryDeviceString(devices[i], EGL_EXTENSIONS);
            if (ext) {
                info.extensions = ext;
            }
            const char* drm = pQueryDeviceString(devices[i], EGL_DRM_DEVICE_FILE_EXT);
            if (drm) {
                info.drm_device_file = drm;
            }
        }

        if (pQueryDeviceAttrib) {
            EGLAttrib cuda_dev = -1;
            if (pQueryDeviceAttrib(devices[i], EGL_CUDA_DEVICE_NV, &cuda_dev)) {
                info.cuda_device_ordinal = static_cast<int>(cuda_dev);
            }
        }

        results.push_back(info);
    }

    return results;
}

bool EGLRenderer::initialize(int target_cuda_device) {
    if (initialized_) {
        return true;
    }

    auto devices = enumerateDevices();
    EGLDeviceEXT chosen_device = EGL_NO_DEVICE_EXT;
    int chosen_index = -1;

    for (const auto& dev : devices) {
        if (dev.cuda_device_ordinal == target_cuda_device) {
            chosen_device = dev.handle;
            chosen_index = dev.index;
            std::cout << "[EGLRenderer] Matched EGL Device " << dev.index
                      << " with target CUDA device " << target_cuda_device << std::endl;
            break;
        }
    }

    if (chosen_device == EGL_NO_DEVICE_EXT && !devices.empty()) {
        int idx = (target_cuda_device >= 0 && target_cuda_device < static_cast<int>(devices.size()))
                      ? target_cuda_device
                      : 0;
        chosen_device = devices[idx].handle;
        chosen_index = idx;
        std::cout << "[EGLRenderer] Using EGL Device index " << idx
                  << " (CUDA ordinal match not returned by extension)" << std::endl;
    }

    auto pGetPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (pGetPlatformDisplay && chosen_device != EGL_NO_DEVICE_EXT) {
        display_ = pGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, chosen_device, nullptr);
    } else {
        std::cout << "[EGLRenderer] Falling back to eglGetDisplay(EGL_DEFAULT_DISPLAY)" << std::endl;
        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }

    if (display_ == EGL_NO_DISPLAY) {
        std::cerr << "[EGLRenderer] Failed to obtain EGLDisplay" << std::endl;
        return false;
    }

    EGLint major = 0, minor = 0;
    if (!eglInitialize(display_, &major, &minor)) {
        std::cerr << "[EGLRenderer] eglInitialize failed (error 0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }

    std::cout << "[EGLRenderer] EGL initialized: " << major << "." << minor << std::endl;

    if (!eglBindAPI(EGL_OPENGL_API)) {
        std::cerr << "[EGLRenderer] Failed to bind EGL_OPENGL_API (error 0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs = 0;
    if (!eglChooseConfig(display_, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
        std::cerr << "[EGLRenderer] eglChooseConfig failed (error 0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }

    const EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };

    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, ctx_attribs);
    if (context_ == EGL_NO_CONTEXT) {
        std::cout << "[EGLRenderer] Core 3.3 context failed; trying without profile mask..." << std::endl;
        const EGLint fallback_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
        };
        context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, fallback_attribs);
    }

    if (context_ == EGL_NO_CONTEXT) {
        std::cerr << "[EGLRenderer] Failed to create EGL context (error 0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }

    if (!eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_)) {
        std::cerr << "[EGLRenderer] eglMakeCurrent failed (error 0x" << std::hex << eglGetError() << std::dec << ")" << std::endl;
        return false;
    }

    const char* gl_vendor = (const char*)glGetString(GL_VENDOR);
    const char* gl_renderer = (const char*)glGetString(GL_RENDERER);
    const char* gl_version = (const char*)glGetString(GL_VERSION);

    std::cout << "[EGLRenderer] OpenGL Context Established:" << std::endl;
    std::cout << "  Vendor:   " << (gl_vendor ? gl_vendor : "unknown") << std::endl;
    std::cout << "  Renderer: " << (gl_renderer ? gl_renderer : "unknown") << std::endl;
    std::cout << "  Version:  " << (gl_version ? gl_version : "unknown") << std::endl;

    if (!setupFBO()) {
        return false;
    }

    if (!setupShaders()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool EGLRenderer::setupFBO() {
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenRenderbuffers(1, &rbo_color_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_color_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_color_);

    glGenRenderbuffers(1, &rbo_depth_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth_);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[EGLRenderer] FBO incomplete! Status: 0x" << std::hex << status << std::dec << std::endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    return true;
}

bool EGLRenderer::setupShaders() {
    const char* vs_source = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 vColor;
void main() {
    gl_Position = vec4(aPos, 1.0);
    vColor = aColor;
}
)";

    const char* fs_source = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

    auto compileShader = [](GLenum type, const char* source) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "[EGLRenderer] Shader compilation failed:\n" << log << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    GLuint vs = compileShader(GL_VERTEX_SHADER, vs_source);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fs_source);
    if (!vs || !fs) {
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint success = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::cerr << "[EGLRenderer] Program link failed:\n" << log << std::endl;
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Colored triangle geometry
    float vertices[] = {
        // Positions        // Colors (RGB: 200, 50, 80)
        -0.6f, -0.6f, 0.0f,  0.8f, 0.2f, 0.3f,
         0.6f, -0.6f, 0.0f,  0.8f, 0.2f, 0.3f,
         0.0f,  0.6f, 0.0f,  0.8f, 0.2f, 0.3f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void EGLRenderer::renderFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);

    // Background color: (51, 102, 153) ~ (0.2, 0.4, 0.6)
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glFinish();
}

bool EGLRenderer::readPixelsAndVerify(uint8_t expected_bg_r, uint8_t expected_bg_g, uint8_t expected_bg_b) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    std::vector<uint8_t> pixels(width_ * height_ * 4);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Check corner pixel (background) at (2, 2)
    int corner_idx = (2 * width_ + 2) * 4;
    uint8_t bg_r = pixels[corner_idx + 0];
    uint8_t bg_g = pixels[corner_idx + 1];
    uint8_t bg_b = pixels[corner_idx + 2];

    // Check center pixel (triangle) at (width / 2, height / 2)
    int center_idx = ((height_ / 2) * width_ + (width_ / 2)) * 4;
    uint8_t tri_r = pixels[center_idx + 0];
    uint8_t tri_g = pixels[center_idx + 1];
    uint8_t tri_b = pixels[center_idx + 2];

    bool bg_match = (std::abs(bg_r - expected_bg_r) <= 3) &&
                    (std::abs(bg_g - expected_bg_g) <= 3) &&
                    (std::abs(bg_b - expected_bg_b) <= 3);

    // Triangle expected color is (204, 51, 76) ~ (0.8, 0.2, 0.3)
    bool tri_match = (std::abs(tri_r - 204) <= 4) &&
                     (std::abs(tri_g - 51) <= 4) &&
                     (std::abs(tri_b - 76) <= 4);

    if (!bg_match || !tri_match) {
        std::cerr << "[EGLRenderer] Pixel verification failed:\n"
                  << "  Corner pixel observed: RGB(" << (int)bg_r << ", " << (int)bg_g << ", " << (int)bg_b
                  << "), expected: RGB(" << (int)expected_bg_r << ", " << (int)expected_bg_g << ", " << (int)expected_bg_b << ")\n"
                  << "  Center pixel observed: RGB(" << (int)tri_r << ", " << (int)tri_g << ", " << (int)tri_b
                  << "), expected: RGB(204, 51, 76)" << std::endl;
        return false;
    }

    return true;
}

void EGLRenderer::teardown() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (fbo_) {
            glDeleteFramebuffers(1, &fbo_);
            fbo_ = 0;
        }
        if (rbo_color_) {
            glDeleteRenderbuffers(1, &rbo_color_);
            rbo_color_ = 0;
        }
        if (rbo_depth_) {
            glDeleteRenderbuffers(1, &rbo_depth_);
            rbo_depth_ = 0;
        }
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (vbo_) {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }

        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }

        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
    initialized_ = false;
}
