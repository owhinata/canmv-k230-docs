# cmake/k230-deploy.cmake — shared K230 deploy/run infrastructure

# --- SDK toolchain existence check ---
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../k230_sdk" _K230_SDK_ROOT)
if(NOT EXISTS "${_K230_SDK_ROOT}/toolchain")
    message(FATAL_ERROR
        "K230 SDK toolchain not found.\n"
        "Run: ./build_sdk.sh")
endif()

# --- K230 device cache variables ---
set(K230_IP "" CACHE STRING "K230 IP address (empty = auto-detect)")
set(K230_USER "root" CACHE STRING "SSH user for K230")
set(K230_SERIAL_LC "/dev/ttyACM0" CACHE STRING "Littlecore serial port")
set(K230_BAUD "115200" CACHE STRING "Serial baud rate")

if(K230_CORE STREQUAL "big")
    set(K230_SERIAL "/dev/ttyACM1" CACHE STRING "Serial port for run target")
else()
    set(K230_SERIAL "${K230_SERIAL_LC}" CACHE STRING "Serial port for run target")
endif()

set(_K230_SCRIPTS_DIR "${CMAKE_CURRENT_LIST_DIR}/scripts")

# --- k230_add_deploy_target() ---
# Arguments: DEPLOY_DIR, DEPENDS, FILES (local:remote ...)
function(k230_add_deploy_target)
    cmake_parse_arguments(ARG "" "DEPLOY_DIR" "DEPENDS;FILES" ${ARGN})
    add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E env
                "K230_USER=${K230_USER}"
                "K230_IP=${K230_IP}"
                "K230_SERIAL_LC=${K230_SERIAL_LC}"
                "K230_BAUD=${K230_BAUD}"
                "K230_DEPLOY_DIR=${ARG_DEPLOY_DIR}"
                ${_K230_SCRIPTS_DIR}/deploy.sh
                ${ARG_FILES}
        DEPENDS ${ARG_DEPENDS}
        USES_TERMINAL
        COMMENT "Deploying to K230 (${ARG_DEPLOY_DIR})"
    )
endfunction()

# --- k230_add_run_target() ---
# Arguments: COMMAND (command string to execute on K230)
function(k230_add_run_target)
    cmake_parse_arguments(ARG "" "COMMAND" "" ${ARGN})
    add_custom_target(run
        COMMAND ${CMAKE_COMMAND} -E env
                "K230_SERIAL=${K230_SERIAL}"
                "K230_BAUD=${K230_BAUD}"
                ${_K230_SCRIPTS_DIR}/run.sh
                "${ARG_COMMAND}"
        USES_TERMINAL
        COMMENT "Running on K230 via ${K230_SERIAL}"
    )
endfunction()

# --- k230_sdk_paths() --- SDK path resolution macro for AI apps
macro(k230_sdk_paths)
    file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../../k230_sdk" _SDK_ROOT)
    set(_MPP_ROOT    ${_SDK_ROOT}/src/big/mpp)
    set(_NNCASE_ROOT ${_SDK_ROOT}/src/big/nncase/riscv64)
    set(_OPENCV_ROOT ${_SDK_ROOT}/src/big/utils/lib/opencv_thead)
endmacro()
