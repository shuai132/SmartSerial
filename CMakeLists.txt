cmake_minimum_required(VERSION 3.1)

option(smartserial_BUILD_EXAMPLE "build example" ON)

project(smartserial)

# serialport
add_subdirectory(modules/serial)
link_libraries(serial)

# log
include_directories(modules/log)

# common
set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

# smartserial
add_library(${PROJECT_NAME} STATIC SmartSerial.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
        modules/serial/include
        .)

# example
if(smartserial_BUILD_EXAMPLE)
    link_libraries(${PROJECT_NAME})
    add_executable(smartserial_example example/smartserial_example.cpp)
endif()
