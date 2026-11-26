cmake_minimum_required(VERSION 3.22.1)
project(gpulab LANGUAGES CXX)
add_library(gpulab SHARED scanner.cpp compute.cpp opencl_compute.cpp)
target_compile_features(gpulab PRIVATE cxx_std_17)
if(REGISTRY_HEADERS)
 target_include_directories(gpulab BEFORE PRIVATE ${REGISTRY_HEADERS})
endif()
target_link_libraries(gpulab PRIVATE vulkan EGL GLESv3 dl log)

if(NOT REGISTRY_HEADERS OR NOT EXISTS "${REGISTRY_HEADERS}/../registry/vk.xml")
 message(FATAL_ERROR "Set VULKAN_HEADERS to the include directory of the pinned Vulkan-Headers checkout (registry/vk.xml required)")
endif()
find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
set(REGISTRY_XML "${REGISTRY_HEADERS}/../registry/vk.xml")
set(REGISTRY_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/generate_registry.py")
execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${REGISTRY_GENERATOR}" "${REGISTRY_XML}" "${CMAKE_CURRENT_BINARY_DIR}/registry_generated.inc" RESULT_VARIABLE gen_result)
if(NOT gen_result EQUAL 0)
 message(FATAL_ERROR "Vulkan registry generation failed")
endif()
target_include_directories(gpulab PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Compile SPIR-V on the host; no shader compiler is shipped in the app.
find_program(GLSLC NAMES glslc HINTS "${CMAKE_ANDROID_NDK}/shader-tools/linux-x86_64" "${ANDROID_NDK}/shader-tools/linux-x86_64" NO_CMAKE_FIND_ROOT_PATH REQUIRED)
set(COMPUTE_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../tools/generate_compute.py")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/compute_generated.inc"
 COMMAND "${PYTHON_EXECUTABLE}" "${COMPUTE_GENERATOR}" "${GLSLC}" "${CMAKE_CURRENT_SOURCE_DIR}/probe.comp" "${CMAKE_CURRENT_BINARY_DIR}/compute_generated.inc"
 DEPENDS "${COMPUTE_GENERATOR}" "${CMAKE_CURRENT_SOURCE_DIR}/probe.comp"
 VERBATIM)
add_custom_target(gpulab_compute_headers DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/compute_generated.inc")
add_dependencies(gpulab gpulab_compute_headers)
