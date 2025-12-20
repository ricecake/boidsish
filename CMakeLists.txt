cmake_minimum_required(VERSION 3.20)
project(Boidsish VERSION 1.0 LANGUAGES CXX)
include(FetchContent)

# 1. C++ Standard and Flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # Corresponds to -std=gnu++23
set(FASTNOISE2_NOISETOOL ON CACHE BOOL "Enable NoiseTool build" FORCE)

# 2. Dependency Discovery
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG      master # Use a specific tag/commit for stability in real projects
)
set(IMGUI_BUILD_EXAMPLES OFF CACHE INTERNAL "") # Prevents building ImGui examples
set(IMGUI_BUILD_DEMO    OFF CACHE INTERNAL "") # Prevents building the demo file
set(IMGUI_BUILD_TESTS   OFF CACHE INTERNAL "")

FetchContent_MakeAvailable(imgui) # This downloads and makes the targets available


add_subdirectory(external/FastNoise2)

# 3. Define the Library
# Finds all .cpp files in src/
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")

add_library(boidsish STATIC ${LIB_SOURCES})

target_include_directories(boidsish
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        external/include
)

# 4. Platform-Specific Linking
target_link_libraries(boidsish
    PUBLIC
        OpenGL::GL
        GLEW::GLEW
        glfw
        FastNoise
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