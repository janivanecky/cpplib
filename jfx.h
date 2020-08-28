#pragma once

#include "graphics.h"

namespace jfx {
    bool hot_reload_compute_shader(
        ComputeShader *shader, char *file_path, FILETIME *previous_write_time, char **macro_defines=NULL, uint32_t macro_defines_count=0
    );
    bool hot_reload_pixel_shader(
        PixelShader *shader, char *file_path, FILETIME *previous_write_time, char **macro_defines=NULL, uint32_t macro_defines_count=0
    );
}

#ifdef CPPLIB_JFX_IMPL
#include "jfx.cpp"
#endif