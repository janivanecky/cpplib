#pragma once

#include "graphics.h"

namespace jfx {
    VertexShader get_vertex_shader_from_file(char *file_path, char **macro_defines=NULL, uint32_t macro_defines_count=0);
    PixelShader get_pixel_shader_from_file(char *file_path, char **macro_defines=NULL, uint32_t macro_defines_count=0);
    ComputeShader get_compute_shader_from_file(char *file_path, char **macro_defines=NULL, uint32_t macro_defines_count=0);

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