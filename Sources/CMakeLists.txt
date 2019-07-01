file(GLOB_RECURSE GLSLTEST_HEADER_FILES "${CMAKE_SOURCE_DIR}/Sources/*.hpp")
file(GLOB_RECURSE GLSLTEST_SOURCE_FILES "${CMAKE_SOURCE_DIR}/Sources/*.cpp")
set(GLSLTEST_SOURCES
		${GLSLTEST_HEADER_FILES}
		${GLSLTEST_SOURCE_FILES}
		)

add_executable(glsltest ${GLSLTEST_SOURCES})

target_compile_features(glsltest PUBLIC cxx_std_17)
target_include_directories(glsltest PRIVATE 
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		)
target_link_libraries(glsltest
		PRIVATE
		Threads::Threads
		Vulkan::Vulkan
		${CMAKE_DL_LIBS}
		SPIRV
		)

set_target_properties(glsltest PROPERTIES
		FOLDER "glsltest"
		)

get_filename_component(CURRENT_PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} PATH)
add_custom_command(TARGET glsltest POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E create_symlink ${CURRENT_PARENT_DIR}/Shaders ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/Shaders
		)