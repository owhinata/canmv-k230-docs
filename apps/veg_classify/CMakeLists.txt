cmake_minimum_required(VERSION 3.16)
project(veg_classify CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/k230-deploy.cmake)
k230_sdk_paths()

# Python training target (not built by default)
# - Creates .venv and installs dependencies if needed
# - Skips training if dataset unchanged (hash-based detection)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_VENV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../.venv)
set(_VENV_PYTHON ${_VENV_DIR}/bin/python)
set(_REQUIREMENTS ${CMAKE_CURRENT_SOURCE_DIR}/../../requirements.txt)
set(_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/output)

# Dataset directory (cmake -DDATA_DIR=/path/to/data to override)
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data" CACHE PATH "Dataset directory for training")

add_custom_target(train
    COMMAND ${Python3_EXECUTABLE} -m venv ${_VENV_DIR}
    COMMAND ${_VENV_PYTHON} -m pip install -r ${_REQUIREMENTS} --quiet
    COMMAND ${_K230_SCRIPTS_DIR}/check_data_hash.sh
            ${DATA_DIR} ${_OUTPUT_DIR}
            ${_VENV_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/train.py ${_OUTPUT_DIR} ${DATA_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    USES_TERMINAL
    COMMENT "Training model (skipped if dataset unchanged)"
)

k230_add_deploy_target(
    DEPLOY_DIR /sharefs/veg_classify
    DEPENDS veg_classify train
    FILES
        $<TARGET_FILE:veg_classify>:veg_classify
        ${_OUTPUT_DIR}/best.kmodel:veg_classify.kmodel
        ${_OUTPUT_DIR}/labels.txt:labels.txt
)
k230_add_run_target(
    COMMAND "/sharefs/veg_classify/veg_classify /sharefs/veg_classify/veg_classify.kmodel /sharefs/veg_classify/labels.txt"
)

add_executable(veg_classify
    src/main.cc
    src/model.cc
    src/classifier.cc
    src/util.cc
)

target_compile_features(veg_classify PRIVATE cxx_std_20)

# SDK sample code has warnings — disable -Werror for this target
target_compile_options(veg_classify PRIVATE
    -Wno-error -Wno-register -Wno-multichar -Wno-pessimizing-move
    -Wno-deprecated-declarations -Wno-unused-result -Wno-unused-variable
    -Wno-format -Wno-return-type -Wno-sign-compare -Wno-unused-label
    -Wno-deprecated-enum-enum-conversion
)

# RT-Smart SDK includes (set by toolchain file)
if(RTT_INCLUDE)
    target_include_directories(veg_classify PRIVATE ${RTT_INCLUDE})
endif()

if(NOT MPP_LIB_DIR)
    message(FATAL_ERROR "MPP_LIB_DIR not set. Use the bigcore toolchain file.")
endif()

target_include_directories(veg_classify PRIVATE
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
target_link_libraries(veg_classify PRIVATE
    -L${MPP_LIB_DIR} -Wl,--start-group ${_MPP_LIBS} -Wl,--end-group
)

# nncase / AI libraries
target_link_libraries(veg_classify PRIVATE
    -L${_NNCASE_ROOT}/rvvlib     -lrvv
    -L${_NNCASE_ROOT}/nncase/lib -lNncase.Runtime.Native
                                 -lnncase.rt_modules.k230
                                 -lfunctional_k230
)

# OpenCV libraries
target_link_libraries(veg_classify PRIVATE
    -L${_OPENCV_ROOT}/lib
    -L${_OPENCV_ROOT}/lib/opencv4/3rdparty
    -lopencv_imgcodecs -lopencv_imgproc -lopencv_core
    -llibjpeg-turbo -llibopenjp2 -llibpng -llibtiff -llibwebp -lzlib
    -lcsi_cv
    -latomic
)
