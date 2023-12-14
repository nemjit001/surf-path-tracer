cmake_minimum_required(VERSION 3.26)
project("Surf" VERSION 0.0.0)
set(TARGET_NAME "surf")

file(GLOB PROJECT_HEADERS "headers/*.h")
file(GLOB PROJECT_SOURCES "sources/*.cpp")

add_executable(${TARGET_NAME}
	${PROJECT_HEADERS}
	${PROJECT_SOURCES}
)

target_include_directories(${TARGET_NAME} PUBLIC
	"headers/"
)
