cmake_minimum_required(VERSION 3.1)

option(smartserial_BUILD_EXAMPLE "build example" ON)

project(smartserial)

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall)

add_library(${PROJECT_NAME} STATIC SmartSerial.cpp)

# log
include_directories(modules/log)

# serialport
add_subdirectory(modules/serial)
include_directories(modules/serial/include)
link_libraries(serial)

# example
if(smartserial_BUILD_EXAMPLE)
    include_directories(.)
    link_libraries(${PROJECT_NAME})
    add_executable(smartserial_exampple example/smartserial_example.cpp)
endif()
