cmake_minimum_required(VERSION 3.7 FATAL_ERROR)


project(AnyAlign)

find_package(AnyDSL_runtime REQUIRED)
include_directories(${AnyDSL_runtime_INCLUDE_DIRS})
set(ANYDSL_RUNTIME_LIBRARIES ${AnyDSL_runtime_LIBRARIES})

set(BACKEND ${BACKEND} CACHE STRING "select the backend from the following: CPU, AVX, NVVM, CUDA, OPENCL, HLS")
if(NOT BACKEND)
    set(BACKEND cpu CACHE STRING "select the backend from the following: CPU, AVX, NVVM, CUDA, OPENCL, HLS" FORCE)
endif()
string(TOLOWER "${BACKEND}" BACKEND)
message(STATUS "Selected backend: ${BACKEND}")
if(BACKEND STREQUAL "hls")
    if(NOT HLS_PART)
        set(HLS_PART xc7z020clg484-1 CACHE STRING "select the hardware the high level synthesis should use." FORCE)
    endif()
    string(TOLOWER "${HLS_PART}" HLS_PART)
    message(STATUS "Selected hls part: ${HLS_PART}")
else()
    unset(HLS_PART CACHE)
endif()

set(BACKEND_FILE src/backend/backend_${BACKEND}.impala)

if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
    set(DEVICE "cpu")
else()
    if(BACKEND STREQUAL "hls")
        set(DEVICE "fpga")
    else()
        set(DEVICE "acc")
    endif()
endif()

if(DEVICE STREQUAL "fpga")
    set(DEVICE_FILES src/sysarr.impala src/iteration_hls.impala src/scoring_hls.impala src/mapping_fpga.impala)
else()
    set(DEVICE_FILES src/iteration_${DEVICE}.impala src/mapping_${DEVICE}.impala src/scoring_${DEVICE}.impala)
endif()

anydsl_runtime_wrap(ANYSEQ_PROGRAM FILES 
    ${BACKEND_FILE} 
    src/align.impala
    src/dynprog.impala 
    src/export.impala
    ${DEVICE_FILES}
    src/predecessors.impala 
    src/scoring.impala 
    src/traceback.impala 
    src/traceback_lintime.impala 
    src/utils.impala
    ) 

add_executable(align 
    src/main.cpp 
    src/alignment_io.cpp 
    src/sequence_io.cpp 
    ${ANYSEQ_PROGRAM})

target_link_libraries(align 
    ${ANYDSL_RUNTIME_LIBRARY} 
    ${ANYDSL_RUNTIME_LIBRARIES})

set_target_properties(align PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)

