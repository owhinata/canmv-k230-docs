cmake_minimum_required(VERSION 3.16)
project(face_detect CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/k230-deploy.cmake)
k230_sdk_paths()

# --- kmodel compilation target ---
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_VENV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../.venv)
set(_VENV_PYTHON ${_VENV_DIR}/bin/python)
set(_REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/../../requirements.txt)
set(_SCRIPTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scripts)
set(_KMODEL_OUTPUT ${_SCRIPTS_DIR}/output/dump/mobile_retinaface.kmodel)

add_custom_target(kmodel
    COMMAND ${Python3_EXECUTABLE} -m venv ${_VENV_DIR}
    COMMAND ${_VENV_PYTHON} -m pip install -r ${_REQUIREMENTS} --quiet
    COMMAND ${_VENV_PYTHON} ${_SCRIPTS_DIR}/step2_simplify_model.py
    COMMAND ${_VENV_PYTHON} ${_SCRIPTS_DIR}/step3_compile_kmodel.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    USES_TERMINAL
    COMMENT "Compiling kmodel (step2 simplify + step3 compile)"
)

k230_add_deploy_target(
    DEPLOY_DIR /sharefs/face_detect
    DEPENDS face_detect kmodel
    FILES
        $<TARGET_FILE:face_detect>:face_detect
        ${_KMODEL_OUTPUT}:mobile_retinaface.kmodel
)
k230_add_run_target(
    COMMAND "/sharefs/face_detect/face_detect /sharefs/face_detect/mobile_retinaface.kmodel 1"
)

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
