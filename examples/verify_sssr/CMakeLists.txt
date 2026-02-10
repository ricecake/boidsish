cmake_minimum_required(VERSION 3.22)
project(verify_sssr)

set(CMAKE_CXX_STANDARD 23)

add_executable(verify_sssr verify_sssr.cpp)
target_link_libraries(verify_sssr PRIVATE check)
target_include_directories(verify_sssr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
