cmake_minimum_required(VERSION 3.25)

project(akai-fs)

set(CMAKE_CXX_STANDARD 17)

add_executable(
        akaiutil
        akaiutil/akaiutil.cc
        include/akaiutil/akaiutil.h
        akaiutil/akaiutil_file.cc
        include/akaiutil/akaiutil_file.h
        akaiutil/akaiutil_io.cc
        include/akaiutil/akaiutil_io.h
        akaiutil/akaiutil_main.cc
        akaiutil/akaiutil_take.cc
        include/akaiutil/akaiutil_take.h
        akaiutil/akaiutil_tar.cc
        include/akaiutil/akaiutil_tar.h
        akaiutil/akaiutil_wav.cc
        include/akaiutil/akaiutil_wav.h
        include/akaiutil/commoninclude.h
        akaiutil/commonlib.cc
        include/akaiutil/fdrawcmd.h
        include/akaiutil/akaiutil_trace.h)

target_include_directories(akaiutil PRIVATE include)

# For now, link libm on UNIX
# TODO(tdial): check on mac
if (UNIX)
    # Link to libm, the math library.
    target_link_libraries(akaiutil m)
endif()

add_executable(
        akaifs
        akaifs/main.cc akaifs/fuse_operations.h akaifs/fuse_operations.cc)

target_include_directories(akaifs PRIVATE include)
target_link_libraries(akaifs fuse3)
# From original makefile
# ifdef FILE_OFFSET_BITS_64
# CFLAGS	+=	-D_FILE_OFFSET_BITS=64