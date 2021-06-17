# Processor specification, possible values: x86_64, armv7le, aarach64, ...
set(CMAKE_SYSTEM_PROCESSOR armv7)
# Specify arch to be armv7le
set(arch gcc_ntoarmv7le)
set(ARCH_NAME armle-v7)

# Command settings for QNX
include(${CMAKE_CURRENT_LIST_DIR}/qnx.cmake)
