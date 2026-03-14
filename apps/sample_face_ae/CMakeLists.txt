cmake_minimum_required(VERSION 3.16)
project(sample_face_ae CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/k230-deploy.cmake)
k230_sdk_paths()

set(_KMODEL ${CMAKE_CURRENT_SOURCE_DIR}/../face_detect/scripts/output/dump/mobile_retinaface.kmodel)

k230_add_deploy_target(
    DEPLOY_DIR /sharefs/sample_face_ae
    DEPENDS sample_face_ae
    FILES
        $<TARGET_FILE:sample_face_ae>:sample_face_ae
        ${_KMODEL}:mobile_retinaface.kmodel
)
k230_add_run_target(
    COMMAND "/sharefs/sample_face_ae/sample_face_ae /sharefs/sample_face_ae/mobile_retinaface.kmodel 1"
)

add_executable(sample_face_ae
    src/main.cc
    src/face_ae_roi.cc
    src/model.cc
    src/mobile_retinaface.cc
    src/util.cc
    src/anchors_320.cc
)

target_compile_features(sample_face_ae PRIVATE cxx_std_20)

# SDK sample code has warnings — disable -Werror for this target
target_compile_options(sample_face_ae PRIVATE
    -Wno-error -Wno-register -Wno-multichar -Wno-pessimizing-move
    -Wno-deprecated-declarations -Wno-unused-result -Wno-unused-variable
    -Wno-format -Wno-return-type -Wno-sign-compare -Wno-unused-label
)

# RT-Smart SDK includes (set by toolchain file)
if(RTT_INCLUDE)
    target_include_directories(sample_face_ae PRIVATE ${RTT_INCLUDE})
endif()

if(NOT MPP_LIB_DIR)
    message(FATAL_ERROR "MPP_LIB_DIR not set. Use the bigcore toolchain file.")
endif()

target_include_directories(sample_face_ae PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${_MPP_ROOT}/include
    ${_MPP_ROOT}/include/comm
    ${_MPP_ROOT}/include/ioctl
    ${_MPP_ROOT}/userapps/api
    ${_NNCASE_ROOT}
    ${_NNCASE_ROOT}/nncase/include
    ${_NNCASE_ROOT}/nncase/include/nncase
    ${_NNCASE_ROOT}/nncase/include/nncase/runtime
    ${_NNCASE_ROOT}/rvvlib/include
)

# MPP libraries
file(GLOB _MPP_LIBS "${MPP_LIB_DIR}/lib*.a")
list(TRANSFORM _MPP_LIBS REPLACE ".*/lib([^/]*)\\.a$" "-l\\1")
target_link_libraries(sample_face_ae PRIVATE
    -L${MPP_LIB_DIR} -Wl,--start-group ${_MPP_LIBS} -Wl,--end-group
)

# nncase / AI libraries
target_link_libraries(sample_face_ae PRIVATE
    -L${_NNCASE_ROOT}/rvvlib     -lrvv
    -L${_NNCASE_ROOT}/nncase/lib -lNncase.Runtime.Native
                                 -lnncase.rt_modules.k230
                                 -lfunctional_k230
)
