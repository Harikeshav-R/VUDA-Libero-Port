#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>

#include <string>
#include <vector>
#include <cstdint>

#ifndef EGL_CUDA_DEVICE_NV
#define EGL_CUDA_DEVICE_NV 0x323A
#endif
#ifndef EGL_DRM_DEVICE_FILE_EXT
#define EGL_DRM_DEVICE_FILE_EXT 0x3233
#endif

struct EGLDeviceInfo {
    int index;
    EGLDeviceEXT handle;
    std::string extensions;
    int cuda_device_ordinal;
    std::string drm_device_file;
};

class EGLRenderer {
public:
    EGLRenderer(int width = 128, int height = 128);
    ~EGLRenderer();

    static std::vector<EGLDeviceInfo> enumerateDevices();

    bool initialize(int target_cuda_device = 0);
    bool setupFBO();
    bool setupShaders();
    void renderFrame();
    bool readPixelsAndVerify(uint8_t expected_bg_r = 51,
                             uint8_t expected_bg_g = 102,
                             uint8_t expected_bg_b = 153);

    // EXP-009 Tagged iteration API
    void renderTaggedFrame(uint64_t iteration);
    bool verifyTaggedFrame(uint64_t iteration);


    GLuint getColorRenderbuffer() const { return rbo_color_; }
    GLuint getFBO() const { return fbo_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    EGLDisplay getDisplay() const { return display_; }
    EGLContext getContext() const { return context_; }
    void teardown();

private:
    void renderSceneInternal();
    int width_;
    int height_;
    EGLDisplay display_;
    EGLContext context_;
    GLuint fbo_;
    GLuint rbo_color_;
    GLuint rbo_depth_;
    GLuint program_;
    GLuint vao_;
    GLuint vbo_;
    bool initialized_;
    std::vector<uint8_t> readback_buffer_;
};
