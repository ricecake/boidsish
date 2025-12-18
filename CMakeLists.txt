cmake_minimum_required(VERSION 3.20)
project(Boidsish VERSION 1.0 LANGUAGES CXX)

# 1. C++ Standard and Flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # Corresponds to -std=gnu++23

# 2. Dependency Discovery
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

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
)

# 5. Build Examples
file(GLOB EXAMPLE_SOURCES "examples/*.cpp")

foreach(EXAMPLE_PATH ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_PATH} NAME_WE)

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_PATH})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE boidsish)
endforeach()