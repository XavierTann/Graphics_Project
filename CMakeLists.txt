cmake_minimum_required(VERSION 3.10)
project(FireSimulator VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/BillboardRenderer.cpp
    src/Particles.cpp
    src/glad.c
    includes/imgui/imgui.cpp
    includes/imgui/imgui_demo.cpp
    includes/imgui/imgui_draw.cpp
    includes/imgui/imgui_tables.cpp
    includes/imgui/imgui_widgets.cpp
    includes/imgui/imgui_impl_glfw.cpp
    includes/imgui/imgui_impl_opengl3.cpp
)

add_executable(FireSimulator ${SOURCES})

target_include_directories(FireSimulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

find_package(OpenGL REQUIRED)
target_link_libraries(FireSimulator PRIVATE OpenGL::GL)

find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
    target_link_libraries(FireSimulator PRIVATE glfw)
elseif(TARGET glfw3::glfw)
    target_link_libraries(FireSimulator PRIVATE glfw3::glfw)
elseif(TARGET glfw3)
    target_link_libraries(FireSimulator PRIVATE glfw3)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW3 QUIET glfw3)
    endif()
    if(GLFW3_FOUND)
        target_link_libraries(FireSimulator PRIVATE ${GLFW3_LIBRARIES})
        target_include_directories(FireSimulator PRIVATE ${GLFW3_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "GLFW3 was not found. Install GLFW3 (e.g., 'brew install glfw' on macOS).")
    endif()
endif()

if(APPLE)
    target_link_libraries(FireSimulator PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

if(APPLE)
    target_compile_definitions(FireSimulator PRIVATE
        GLFW_EXPOSE_NATIVE_COCOA
        GLFW_EXPOSE_NATIVE_NSGL
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(FireSimulator PRIVATE pthread dl)
endif()

add_custom_command(TARGET FireSimulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:FireSimulator>/data
)
