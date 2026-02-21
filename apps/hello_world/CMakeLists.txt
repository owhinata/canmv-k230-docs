cmake_minimum_required(VERSION 3.16)
project(hello_world C)

add_executable(hello_world src/hello.c)

# RT-Smart SDK includes (set by toolchain file, absent for littlecore)
if(RTT_INCLUDE)
    target_include_directories(hello_world PRIVATE ${RTT_INCLUDE})
endif()
