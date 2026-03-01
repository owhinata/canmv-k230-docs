cmake_minimum_required(VERSION 3.16)
project(face_detect CXX)

add_executable(face_detect
    src/main.cc
    src/face_ae_roi.cc
    src/model.cc
    src/mobile_retinaface.cc
    src/util.cc
    src/anchors_320.cc
)

target_compile_features(face_detect PRIVATE cxx_std_20)

# SDK sample code has warnings — disable -Werror for this target
target_compile_options(face_detect PRIVATE
    -Wno-error -Wno-register -Wno-multichar -Wno-pessimizing-move
    -Wno-deprecated-declarations -Wno-unused-result -Wno-unused-variable
    -Wno-format -Wno-return-type -Wno-sign-compare -Wno-unused-label
    -Wno-deprecated-enum-enum-conversion
)

# RT-Smart SDK includes (set by toolchain file)
if(RTT_INCLUDE)
    target_include_directories(face_detect PRIVATE ${RTT_INCLUDE})
endif()

if(NOT MPP_LIB_DIR)
    message(FATAL_ERROR "MPP_LIB_DIR not set. Use the bigcore toolchain file.")
endif()

# SDK root and path resolution
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../../k230_sdk" _SDK_ROOT)
set(_MPP_ROOT    ${_SDK_ROOT}/src/big/mpp)
set(_NNCASE_ROOT ${_SDK_ROOT}/src/big/nncase/riscv64)
set(_OPENCV_ROOT ${_SDK_ROOT}/src/big/utils/lib/opencv_thead)

target_include_directories(face_detect PRIVATE
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
    ${_OPENCV_ROOT}/include/opencv4
)

# MPP libraries
file(GLOB _MPP_LIBS "${MPP_LIB_DIR}/lib*.a")
list(TRANSFORM _MPP_LIBS REPLACE ".*/lib([^/]*)\\.a$" "-l\\1")
target_link_libraries(face_detect PRIVATE
    -L${MPP_LIB_DIR} -Wl,--start-group ${_MPP_LIBS} -Wl,--end-group
)

# nncase / AI libraries
target_link_libraries(face_detect PRIVATE
    -L${_NNCASE_ROOT}/rvvlib     -lrvv
    -L${_NNCASE_ROOT}/nncase/lib -lNncase.Runtime.Native
                                 -lnncase.rt_modules.k230
                                 -lfunctional_k230
)

# OpenCV libraries
target_link_libraries(face_detect PRIVATE
    -L${_OPENCV_ROOT}/lib
    -L${_OPENCV_ROOT}/lib/opencv4/3rdparty
    -lopencv_imgcodecs -lopencv_imgproc -lopencv_core
    -llibjpeg-turbo -llibopenjp2 -llibpng -llibtiff -llibwebp -lzlib
    -lcsi_cv
    -latomic
)
