cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)
project(glsltest VERSION 0.1.0 LANGUAGES C CXX)

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "Adds a postfix for debug-built libraries.")
if(WIN32)
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

foreach(_glslang_option "BUILD_TESTING" "ENABLE_GLSLANG_BINARIES" "ENABLE_SPVREMAPPER" "ENABLE_HLSL")
	set(${_glslang_option} OFF CACHE INTERNAL "")
endforeach()
foreach(_glslang_option "SKIP_GLSLANG_INSTALL" "ENABLE_AMD_EXTENSIONS" "ENABLE_NV_EXTENSIONS" "ENABLE_OPT")
	set(${_glslang_option} ON CACHE INTERNAL "")
endforeach()
add_subdirectory(${PROJECT_SOURCE_DIR}/glslang)

add_executable(glsltest main.cpp)
add_dependencies(glsltest SPIRV)

configure_file(sample.vert "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sample.vert" COPYONLY)
configure_file(sample.frag "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sample.frag" COPYONLY)

target_compile_features(glsltest PUBLIC cxx_std_17)
target_include_directories(glsltest PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(glsltest PRIVATE SPIRV)
