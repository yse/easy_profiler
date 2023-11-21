# Processor specification, possible values: x86_64, armv7le, aarach64, ...
set(CMAKE_SYSTEM_PROCESSOR x86_64)
# Specify arch to be x86_64
set(arch gcc_ntox86_64)
set(ARCH_NAME x86_64)

# Command settings for QNX
include(${CMAKE_CURRENT_LIST_DIR}/qnx.cmake)
