cmake_minimum_required(VERSION 3.8)
project("Surf" VERSION 0.0.0)
set(TARGET_NAME "surf")

# Dependency setup
# OpenMP
find_package(OpenMP REQUIRED)

# Vulkan
find_package(Vulkan REQUIRED)

# GLFW
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory(dependencies/glfw/)

# GLM
add_subdirectory(dependencies/glm/)

# TINYOBJ
add_subdirectory(dependencies/tinyobjloader/)

# VMA
add_subdirectory(dependencies/VulkanMemoryAllocator/)

# Vulkan Bootstrap
option(VK_BOOTSTRAP_WERROR OFF)
option(VK_BOOTSTRAP_TEST OFF)
add_subdirectory(dependencies/vk-bootstrap/)

# ImGUI
file(GLOB IMGUI_SOURCES "dependencies/imgui/*.cpp")
file(GLOB IMGUI_HEADERS "dependencies/imgui/*.h")

set (IMGUI_SOURCES
	${IMGUI_SOURCES}
	"dependencies/imgui/backends/imgui_impl_glfw.cpp"
	"dependencies/imgui/backends/imgui_impl_vulkan.cpp"
)

set (IMGUI_HEADERS
	${IMGUI_SOURCES}
	"dependencies/imgui/backends/imgui_impl_glfw.h"
	"dependencies/imgui/backends/imgui_impl_vulkan.h"
)

# Surf setup
file(GLOB PROJECT_SHADERS "shaders/*.vert" "shaders/*.frag" "shaders/*.comp")
file(GLOB PROJECT_HEADERS "headers/*.h" "headers/**/*.h")
file(GLOB PROJECT_SOURCES "sources/*.cpp" "sources/**/*.cpp")

source_group("Shaders" ${PROJECT_SHADERS})
source_group("Headers" ${PROJECT_HEADERS})
source_group("Sources" ${PROJECT_SOURCES})

if (UNIX)
	# Setup SSE, SSE2, SSE3, AVX usage
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse -msse2 -msse3 -msse4.1")
endif()

add_executable(${TARGET_NAME}
	${PROJECT_SHADERS}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
	${IMGUI_HEADERS}
	${IMGUI_SOURCES}
)

target_include_directories(${TARGET_NAME} PUBLIC
	"headers/"
	${Vulkan_INCLUDE_DIRS}
	"dependencies/glfw/include/"
	"dependencies/VulkanMemoryAllocator/include/"
	"dependencies/vk-bootstrap/src/"
	"dependencies/glm/"
	"dependencies/imgui/"
	"dependencies/imgui/backends/"
)

target_link_libraries(${TARGET_NAME}
	${CMAKE_DL_LIBS}
	${Vulkan_LIBRARIES}
	glfw ${GLFW_LIBRARIES}
	tinyobjloader
	VulkanMemoryAllocator
	vk-bootstrap
	optimized OpenMP::OpenMP_CXX
)

# Copy asset output directory for Surf
add_custom_target(copy-asset-dir ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/" $<TARGET_FILE_DIR:${TARGET_NAME}>/assets
	COMMENT "Copying asset directory"
)

# Generate shader output directory + build shaders for Surf
add_custom_target(create-shader-dir ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders
	COMMENT "Creating shader output directory"
)

foreach(SHADER IN LISTS PROJECT_SHADERS)
	cmake_path(GET SHADER FILENAME SHADER_FILENAME)

	add_custom_target(${SHADER_FILENAME} ALL
		glslc ${SHADER} -o $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders/${SHADER_FILENAME}.spv
		COMMENT "Compiling shader ${SHADER_FILENAME}"
		DEPENDS ${SHADER} ${PROJECT_SHADERS}
	)
endforeach()
