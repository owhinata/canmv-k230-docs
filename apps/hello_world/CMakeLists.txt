cmake_minimum_required(VERSION 3.16)
project(hello_world C)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/k230-deploy.cmake)

add_executable(hello_world src/hello.c)

# RT-Smart SDK includes (set by toolchain file, absent for littlecore)
if(RTT_INCLUDE)
    target_include_directories(hello_world PRIVATE ${RTT_INCLUDE})
endif()

if(K230_CORE STREQUAL "big")
    k230_add_deploy_target(
        DEPLOY_DIR /sharefs
        DEPENDS hello_world
        FILES $<TARGET_FILE:hello_world>:hello_world
    )
    k230_add_run_target(COMMAND "/sharefs/hello_world")
elseif(K230_CORE STREQUAL "little")
    k230_add_deploy_target(
        DEPLOY_DIR /root
        DEPENDS hello_world
        FILES $<TARGET_FILE:hello_world>:hello_world
    )
    k230_add_run_target(COMMAND "/root/hello_world")
endif()
