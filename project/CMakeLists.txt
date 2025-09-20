cmake_minimum_required(VERSION 3.10)
project(BER_SIMULATOR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB SOURCES src/*.cpp)

find_package(Threads REQUIRED)
add_executable(ber ${SOURCES})
target_link_libraries(ber PRIVATE Threads::Threads)
