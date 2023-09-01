cmake_minimum_required(VERSION 3.25)

project(akaiutil)

set(CMAKE_CXX_STANDARD 17)

add_executable(
        akaiutil
        akaiutil.c
        akaiutil.h
        akaiutil_file.c
        akaiutil_file.h
        akaiutil_io.c
        akaiutil_io.h
        akaiutil_main.c
        akaiutil_take.c
        akaiutil_take.h
        akaiutil_tar.c
        akaiutil_tar.h
        akaiutil_wav.c
        akaiutil_wav.h
        commoninclude.h
        commonlib.c
        fdrawcmd.h
)

# For now, link libm on UNIX
# TODO(tdial): check on mac
if (UNIX)
    # Link to libm, the math library.
    target_link_libraries(akaiutil m)
endif()

# From original makefile
# ifdef FILE_OFFSET_BITS_64
# CFLAGS	+=	-D_FILE_OFFSET_BITS=64