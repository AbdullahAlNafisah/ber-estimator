cmake_minimum_required(VERSION 3.16)
project(BER_SIMULATOR LANGUAGES CXX)

# Build type (CMake Tools gui also controls this)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(ber
  src/main.cpp
)

# Point to your IT++ install
target_include_directories(ber PRIVATE "C:/itpp/include")
# Tell the linker where to search
link_directories("C:/itpp/lib")

# MSVC build of IT++ typically produces itpp.lib (and a itpp.dll in bin)
target_link_libraries(ber PRIVATE itpp)

# If you built shared (DLL): make sure the runtime can find it
# (either add C:\itpp\bin to PATH, or copy the DLL next to your .exe)
