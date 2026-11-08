cmake_minimum_required(VERSION 3.18)
project(vuda_native_probe LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 80)
endif()

add_definitions(-DGL_GLEXT_PROTOTYPES=1)

find_package(CUDAToolkit REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(vuda_native_probe
    main.cpp
    egl_renderer.cpp
    benchmark_runner.cpp
    controlled_runner.cpp
    device_identity_guard.cpp
    cuda_compute.cu
)

target_include_directories(vuda_native_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vuda_native_probe PRIVATE
    EGL
    OpenGL::GL
    CUDA::cudart
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

add_executable(test_measurement_window
    tests/test_measurement_window.cpp
)
target_include_directories(test_measurement_window PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_measurement_window PRIVATE
    Threads::Threads
)
