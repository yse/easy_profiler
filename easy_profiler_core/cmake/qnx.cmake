# Cross compilation for QNX
set(CMAKE_SYSTEM_NAME QNX)
set(QNX YES)

# Suppose you have already set QNX_HOST and QNX_TARGET Add compiler and linker
# flags -std=gnu++14 is needed because QNX header files only cover GNU and POSIX
set(CMAKE_CXX_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -lang-c++ -V${arch} -std=gnu++14")

# Add definitions command for diffent platforms So we can use macro definition
# "#ifdef PLATFORM_QNX" in source code
set(PLATFORM_QNX ON)
add_definitions(-DPLATFORM_QNX)
# Build crossguid library for qnx
set(GUID_LIBUUID ON)
add_definitions(-DGUID_LIBUUID)

# Upstream settings
set(CMAKE_C_COMPILER qcc)
set(CMAKE_C_COMPILER_TARGET ${arch})
set(CMAKE_CXX_COMPILER q++)
set(CMAKE_CXX_COMPILER_TARGET ${arch})

# Set the path to the qnx host include and lib
include_directories($ENV{QNX_HOST}/usr/include)
link_directories($ENV{QNX_HOST}/usr/lib)

# Set the path to the qnx target include and lib
include_directories($ENV{QNX_TARGET}/usr/include)
link_directories($ENV{QNX_TARGET}/usr/lib)
include_directories($ENV{QNX_TARGET}/${ARCH_NAME}/usr/include)
link_directories($ENV{QNX_TARGET}/${ARCH_NAME}/lib)
link_directories($ENV{QNX_TARGET}/${ARCH_NAME}/usr/lib)
