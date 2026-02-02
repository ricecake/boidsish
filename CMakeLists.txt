cmake_minimum_required(VERSION 3.20)
project(Boidsish VERSION 1.0 LANGUAGES CXX)

# 1. C++ Standard and Flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # Corresponds to -std=gnu++23

set(IMGUI_DIR external/imgui)
set(STB_DIR "${CMAKE_SOURCE_DIR}/external/stb")

# 2. Dependency Discovery
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# 1. Define ImGui FIRST so FastNoise2 can see it
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)

# 2. Tell FastNoise2 where the ImGui headers are explicitly
set(IMGUI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}" CACHE PATH "" FORCE)

get_directory_property(OLD_COMPILE_OPTIONS COMPILE_OPTIONS)
add_compile_options(-w) # Disable all warnings for GCC/Clang for external libraries
set(BUILD_TESTING OFF CACHE BOOL "Disable submodule tests" FORCE)
add_subdirectory(external/task-thread-pool)
add_subdirectory(external/poolSTL)
add_subdirectory(external/libmorton)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "${OLD_COMPILE_OPTIONS}")

# 3. Define the Library
# Finds all .cpp files in src/
file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")

add_library(boidsish STATIC ${LIB_SOURCES})
target_compile_definitions(boidsish PUBLIC GLM_ENABLE_EXPERIMENTAL)

target_include_directories(boidsish
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        external/include
        external/libmorton/include
        ${IMGUI_DIR}/
        ${IMGUI_DIR}/backends
        ${STB_DIR}/
        ${ASSIMP_INCLUDE_DIR}
        ${BULLET_INCLUDE_DIRS}
)

# 4. Platform-Specific Linking
target_link_libraries(boidsish
    PUBLIC
        OpenGL::GL
        GLEW::GLEW
        glfw
        imgui
        task-thread-pool::task-thread-pool
        poolSTL::poolSTL
        ${ASSIMP_LIBRARIES}
        ZLIB::ZLIB
        libmorton::libmorton
        Threads::Threads
        glm::glm
)

# 5. Build Examples
file(GLOB EXAMPLE_DIRS "examples/*")

foreach(EXAMPLE_DIR ${EXAMPLE_DIRS})
    if(IS_DIRECTORY ${EXAMPLE_DIR})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_DIR} NAME)

        # Find all source files in the example's directory
        file(GLOB EXAMPLE_SOURCES
            "${EXAMPLE_DIR}/main.cpp"
            "${EXAMPLE_DIR}/src/*.cpp"
        )

        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCES})

        target_link_libraries(${EXAMPLE_NAME} PRIVATE boidsish)
        target_include_directories(${EXAMPLE_NAME} PRIVATE "${EXAMPLE_DIR}/include")

        target_compile_definitions(${EXAMPLE_NAME} PRIVATE GLM_ENABLE_EXPERIMENTAL)
        set_target_properties(${EXAMPLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    endif()
endforeach()

# Copy assets to the root of the build directory
add_custom_command(TARGET boidsish POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_BINARY_DIR}/assets
)

# Find the clang-format executable
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    # Define the files to be formatted, matching your Makefile's scope
    file(GLOB_RECURSE FORMAT_SOURCES
        "src/*.cpp" "src/*.h" "src/*.hpp"
        "include/*.cpp" "include/*.h" "include/*.hpp"
        "examples/*.cpp" "examples/*.h" "examples/*.hpp"
        "shaders/*"
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_SOURCES}
        COMMENT "Running clang-format on all source files..."
    )
else()
    message(WARNING "clang-format not found. The 'format' target will not be available.")
endif()

# 6. Build Tests
enable_testing()
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(test_rigid_body tests/test_rigid_body.cpp)
target_link_libraries(test_rigid_body PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_rigid_body PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_rigid_body COMMAND test_rigid_body)

add_executable(test_terrain tests/test_terrain.cpp)
target_link_libraries(test_terrain PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_terrain PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_terrain COMMAND test_terrain)

add_executable(test_config tests/test_config.cpp)
target_link_libraries(test_config PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_config PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_config COMMAND test_config)

add_executable(test_concurrent_queue tests/test_concurrent_queue.cpp)
target_link_libraries(test_concurrent_queue PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_concurrent_queue PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_concurrent_queue COMMAND test_concurrent_queue)

add_executable(test_vector tests/test_vector.cpp)
target_link_libraries(test_vector PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_vector PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_vector COMMAND test_vector)

add_executable(test_thread_pool tests/test_thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_thread_pool PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_thread_pool COMMAND test_thread_pool)

add_executable(test_spline tests/test_spline.cpp)
target_link_libraries(test_spline PRIVATE boidsish GTest::GTest GTest::Main)
set_target_properties(test_spline PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME test_spline COMMAND test_spline)
