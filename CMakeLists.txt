cmake_minimum_required(VERSION 3.1)

option(smartserial_BUILD_EXAMPLE "build example" ON)

project(smartserial)

# serialportb
add_subdirectory(modules/serial)
include_directories(modules/serial/include)
link_libraries(serial)

# log
include_directories(modules/log)

# common
set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

# smartserial
add_library(${PROJECT_NAME} STATIC SmartSerial.cpp)

# example
if(smartserial_BUILD_EXAMPLE)
    include_directories(.)
    link_libraries(${PROJECT_NAME})
    add_executable(smartserial_exampple example/smartserial_example.cpp)
endif()
