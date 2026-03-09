cmake_minimum_required(VERSION 3.10)
project(FireSimulationUI)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Include directories
include_directories(include)

# ImGui sources
set(IMGUI_SOURCES
    include/imgui.cpp
    include/imgui_draw.cpp
    include/imgui_widgets.cpp
    include/imgui_tables.cpp
    include/backends/imgui_impl_glfw.cpp
    include/backends/imgui_impl_opengl3.cpp
)

# Add executable
add_executable(${PROJECT_NAME} src/main.cpp ${IMGUI_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} OpenGL::GL glfw)