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

file(GLOB PROJECT_HEADERS "headers/*.h")
file(GLOB PROJECT_SOURCES "sources/*.cpp")

add_executable(${TARGET_NAME}
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
