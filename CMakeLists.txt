cmake_minimum_required(VERSION 3.8)
project("Surf" VERSION 0.0.0)
set(TARGET_NAME "surf")

# Dependency setup
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

file(GLOB PROJECT_SHADERS "shaders/*.glsl" "shaders/*.vert" "shaders/*.frag" "shaders/*.compute")
file(GLOB PROJECT_HEADERS "headers/*.h" "headers/**/*.h")
file(GLOB PROJECT_SOURCES "sources/*.cpp" "sources/**/*.cpp")

source_group("Shaders" ${PROJECT_SHADERS})
source_group("Headers" ${PROJECT_HEADERS})
source_group("Sources" ${PROJECT_SOURCES})

add_executable(${TARGET_NAME}
	${PROJECT_SHADERS}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
)

target_include_directories(${TARGET_NAME} PUBLIC
	"headers/"
	${Vulkan_INCLUDE_DIRS}
	"dependencies/glfw/include/"
	"dependencies/VulkanMemoryAllocator/include/"
	"dependencies/vk-bootstrap/src/"
	"dependencies/glm/"
)

target_link_libraries(${TARGET_NAME}
	${CMAKE_DL_LIBS}
	${Vulkan_LIBRARIES}
	glfw ${GLFW_LIBRARIES}
	tinyobjloader
	VulkanMemoryAllocator
	vk-bootstrap
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
