# Processor specification, possible values: x86_64, armv7le, aarach64, ...
set(CMAKE_SYSTEM_PROCESSOR aarch64)
# Specify arch to be aarch64le
set(arch gcc_ntoaarch64le)
set(ARCH_NAME aarch64le)

# Command settings for QNX
include(${CMAKE_CURRENT_LIST_DIR}/qnx.cmake)
