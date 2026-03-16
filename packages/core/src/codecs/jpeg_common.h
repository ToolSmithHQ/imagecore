#ifndef IC_JPEG_COMMON_H
#define IC_JPEG_COMMON_H

#include <cstdio>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>

struct ic_jpeg_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};

static inline void ic_jpeg_error_exit(j_common_ptr cinfo) {
    auto* err = reinterpret_cast<ic_jpeg_error_mgr*>(cinfo->err);
    longjmp(err->setjmp_buffer, 1);
}

#endif /* IC_JPEG_COMMON_H */
