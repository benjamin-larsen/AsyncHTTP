cmake_minimum_required(VERSION 4.1)
project(AsyncHTTP C)

set(CMAKE_C_STANDARD 23)

add_executable(AsyncHTTP main.c)

if(WIN32)
    target_link_libraries(HTTPServer PRIVATE ws2_32)
endif()