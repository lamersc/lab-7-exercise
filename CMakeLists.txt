cmake_minimum_required(VERSION 3.16)
project(ecs LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(main src/main.cpp)
target_compile_features(main PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(main PRIVATE /W4 /permissive- /Zc:__cplusplus)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
    target_compile_options(main PRIVATE -Wall -Wextra -Wpedantic)
endif ()
