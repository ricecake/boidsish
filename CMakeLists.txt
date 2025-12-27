cmake_minimum_required(VERSION 3.20)
project(Boidsish VERSION 1.0 LANGUAGES CXX)

# 1. C++ Standard and Flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # Corresponds to -std=gnu++23
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "Enable NoiseTool build" FORCE)
set(IMGUI_DIR external/imgui)

# 2. Dependency Discovery
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Bullet REQUIRED)
find_package(assimp REQUIRED)

# 1. Define ImGui FIRST so FastNoise2 can see it
set(IMGUI_DIR external/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Important: NoiseTool needs to find the headers within the imgui target
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# 2. Tell FastNoise2 where the ImGui headers are explicitly
# set(FASTNOISE2_NOISETOOL_GLFW OFF CACHE BOOL "" FORCE)
# set(FASTNOISE2_NOISETOOL_IMGUI OFF CACHE BOOL "" FORCE)
# set(FASTNOISE2_NOISETOOL ON CACHE BOOL "Enable NoiseTool build" FORCE)
set(IMGUI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}" CACHE PATH "" FORCE)

# 3. Now add FastNoise2
add_subdirectory(external/FastNoise2)
add_subdirectory(external/task-thread-pool)
add_subdirectory(external/poolSTL)
# add_subdirectory(external/bullet3)
# add_subdirectory(external/assimp)

# 3. Define the Library
# Finds all .cpp files in src/
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp" "src/post_processing/*.cpp")

add_library(boidsish STATIC ${LIB_SOURCES})

target_include_directories(boidsish
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        external/include
        ${IMGUI_DIR}/
        ${IMGUI_DIR}/backends
        ${ASSIMP_INCLUDE_DIR}
        ${BULLET_INCLUDE_DIRS}
)

# 4. Platform-Specific Linking
target_link_libraries(boidsish
    PUBLIC
        OpenGL::GL
        GLEW::GLEW
        glfw
        FastNoise
        imgui
        task-thread-pool::task-thread-pool
        poolSTL::poolSTL
        ${ASSIMP_LIBRARIES}
        ${BULLET_LIBRARIES}
)

# 5. Build Examples
file(GLOB EXAMPLE_SOURCES "examples/*.cpp")

foreach(EXAMPLE_PATH ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_PATH} NAME_WE)

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_PATH})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE boidsish)
endforeach()

# Find the clang-format executable
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    # Define the files to be formatted, matching your Makefile's scope
    file(GLOB_RECURSE FORMAT_SOURCES
        "src/*.cpp" "src/*.h" "src/*.hpp"
        "include/*.cpp" "include/*.h" "include/*.hpp"
        "examples/*.cpp" "examples/*.h" "examples/*.hpp"
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_SOURCES}
        COMMENT "Running clang-format on all source files..."
    )
else()
    message(WARNING "clang-format not found. The 'format' target will not be available.")
endif()