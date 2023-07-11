## Copyright 2018 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

set(DNNL_SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/mkl-dnn")
set(DNNL_BINARY_DIR "${PROJECT_BINARY_DIR}/external/mkl-dnn")

set(DNNL_VERSION_MAJOR 3)
set(DNNL_VERSION_MINOR 0)
set(DNNL_VERSION_PATCH 1)
set(DNNL_VERSION_HASH  "N/A")

set(DNNL_CPU_RUNTIME "TBB")
set(DNNL_CPU_THREADING_RUNTIME "TBB")
set(DNNL_GPU_RUNTIME "NONE")

option(DNNL_ENABLE_JIT_PROFILING
  "Enable registration of oneDNN kernels that are generated at runtime with
  VTune Amplifier. Without the registrations, VTune Amplifier would report
  data collected inside the kernels as `outside any known module`."
  OFF)
mark_as_advanced(DNNL_ENABLE_JIT_PROFILING)

option(DNNL_ENABLE_ITT_TASKS
  "Enable ITT Tasks tagging feature and tag all primitive execution. VTune
  Amplifier can group profiling results based on those ITT tasks and show
  corresponding timeline information."
  OFF)
mark_as_advanced(DNNL_ENABLE_ITT_TASKS)

set(BUILD_INFERENCE TRUE)
set(BUILD_CONVOLUTION TRUE)
set(BUILD_PRIMITIVE_CPU_ISA_ALL TRUE)

configure_file(
  "${DNNL_SOURCE_DIR}/include/oneapi/dnnl/dnnl_config.h.in"
  "${DNNL_BINARY_DIR}/include/oneapi/dnnl/dnnl_config.h"
)
configure_file(
  "${DNNL_SOURCE_DIR}/include/oneapi/dnnl/dnnl_version.h.in"
  "${DNNL_BINARY_DIR}/include/oneapi/dnnl/dnnl_version.h"
)

file(GLOB DNNL_SOURCES
  ${DNNL_SOURCE_DIR}/src/common/*.[ch]pp
  ${DNNL_SOURCE_DIR}/src/cpu/jit_utils/*.[ch]pp
)

if(OIDN_ARCH STREQUAL "X64")
  file(GLOB DNNL_SOURCES_X64
  ${DNNL_SOURCE_DIR}/src/cpu/x64/injectors/*.[ch]pp
  ${DNNL_SOURCE_DIR}/src/cpu/x64/xbyak/*.h
)
  list(APPEND DNNL_SOURCES ${DNNL_SOURCES_X64})
elseif(OIDN_ARCH STREQUAL "ARM64")
  file(GLOB DNNL_SOURCES_ARM64
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/injectors/*.[ch]pp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/xbyak_aarch64/xbyak_aarch64/*.h
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/xbyak_aarch64/src/*.h
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/xbyak_aarch64/src/*.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm/*.[ch]pp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm/f32/*.[ch]pp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm/s8x8s32/*.[ch]pp
  )
  list(APPEND DNNL_SOURCES ${DNNL_SOURCES_ARM64})
else()
  message(fatal_error "Unknown Architecture")
endif()

list(APPEND DNNL_SOURCES
  ${DNNL_SOURCE_DIR}/src/cpu/bfloat16.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/binary_injector_utils.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/binary_injector_utils.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_concat.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_convolution_list.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_convolution_pd.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_eltwise_pd.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_engine.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_engine.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_memory_storage.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_stream.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/cpu_sum.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/float16.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/platform.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/platform.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/primitive_attr_postops.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/primitive_attr_postops.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/scale_utils.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/scale_utils.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/simple_q10n.hpp
  ${DNNL_SOURCE_DIR}/src/cpu/zero_point_utils.cpp
  ${DNNL_SOURCE_DIR}/src/cpu/zero_point_utils.hpp
)

if(OIDN_ARCH STREQUAL "X64")
  list(APPEND DNNL_SOURCES
    ${DNNL_SOURCE_DIR}/src/cpu/x64/amx_tile_configure.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/amx_tile_configure.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/cpu_barrier.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/cpu_barrier.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/cpu_isa_traits.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/cpu_isa_traits.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/cpu_reducer.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/cpu_reducer.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx2_conv_kernel_f32.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx2_conv_kernel_f32.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx2_convolution.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx2_convolution.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx512_common_convolution.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx512_common_convolution.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx512_common_conv_kernel.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_avx512_common_conv_kernel.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_generator.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_primitive_conf.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_sse41_conv_kernel_f32.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_sse41_conv_kernel_f32.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_sse41_convolution.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_sse41_convolution.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_transpose_utils.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/x64/jit_transpose_utils.hpp
  )
elseif(OIDN_ARCH STREQUAL "ARM64")
  list(APPEND DNNL_SOURCES
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/cpu_barrier.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/cpu_barrier.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/cpu_isa_traits.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/cpu_isa_traits.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/cpu_reducer.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/cpu_reducer.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm_convolution_utils.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm_convolution_utils.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm_convolution.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm_convolution.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm/gemm.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/gemm/gemm.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/ref_convolution.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/ref_convolution.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/ref_convolution_utils.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/jit_generator.hpp
    ${DNNL_SOURCE_DIR}/src/cpu/aarch64/jit_primitive_conf.hpp
)
endif()

if(DNNL_ENABLE_JIT_PROFILING OR DNNL_ENABLE_ITT_TASKS)
  file(GLOB ITT_SOURCES ${DNNL_SOURCE_DIR}/src/common/ittnotify/*.[ch])
  list(APPEND DNNL_SOURCES ${ITT_SOURCES})
endif()
if(DNNL_ENABLE_JIT_PROFILING)
  list(APPEND DNNL_SOURCES
    ${DNNL_SOURCE_DIR}/src/cpu/jit_utils/linux_perf/linux_perf.cpp
    ${DNNL_SOURCE_DIR}/src/cpu/jit_utils/linux_perf/linux_perf.hpp
  )
endif()

if(MSVC)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set_source_files_properties(${DNNL_SOURCES} PROPERTIES COMPILE_FLAGS "/bigobj")
  endif()
endif()

add_library(dnnl STATIC ${DNNL_SOURCES})

target_include_directories(dnnl
  PUBLIC
    $<BUILD_INTERFACE:${DNNL_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${DNNL_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${DNNL_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${DNNL_SOURCE_DIR}/src/common>
    $<BUILD_INTERFACE:${DNNL_SOURCE_DIR}/src/cpu>
    $<BUILD_INTERFACE:${DNNL_SOURCE_DIR}/src/cpu/xbyak>
)

target_compile_definitions(dnnl
  PUBLIC
    -DDNNL_ENABLE_CONCURRENT_EXEC
)

set(DNNL_COMPILE_OPTIONS "")
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # Correct 'jnl' macro/jit issue
  list(APPEND DNNL_COMPILE_OPTIONS "/Qlong-double")
endif()
target_compile_options(dnnl PRIVATE ${DNNL_COMPILE_OPTIONS})

target_link_libraries(dnnl
  PUBLIC
    ${CMAKE_THREAD_LIBS_INIT}
    TBB::tbb
)

if(DNNL_ENABLE_JIT_PROFILING OR DNNL_ENABLE_ITT_TASKS)
  if(UNIX AND NOT APPLE)
    # Not every compiler adds -ldl automatically
    target_link_libraries(dnnl PUBLIC ${CMAKE_DL_LIBS})
  endif()
endif()
if(DNNL_ENABLE_ITT_TASKS)
  target_compile_definitions(dnnl PUBLIC -DDNNL_ENABLE_ITT_TASKS)
endif()
if(NOT DNNL_ENABLE_JIT_PROFILING)
  target_compile_definitions(dnnl PUBLIC -DDNNL_ENABLE_JIT_PROFILING=0)
endif()