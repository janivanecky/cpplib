#include "jfx.h"
#include "file_system.h"

 bool jfx::hot_reload_compute_shader(
    ComputeShader *shader, char *file_path, FILETIME *previous_write_time, char **macro_defines, uint32_t macro_defines_count
) {
    // Get the latest shader file write time.
    FILETIME current_write_time = file_system::get_last_write_time(file_path);

    // Check if we've seen this shader before. If not, attempt reload.
    if (CompareFileTime(&current_write_time, previous_write_time) != 0) {
        // Try to compile the new shader.
        File shader_file = file_system::read_file(file_path);
        ComputeShader new_shader = graphics::get_compute_shader_from_code(
            (char *)shader_file.data, shader_file.size, macro_defines, macro_defines_count
        );
        file_system::release_file(shader_file);

        // If the compilation was successful, release the old shader and replace with the new one.
        bool reload_success = graphics::is_ready(&new_shader);
        if(reload_success) {
            graphics::release(shader);
            *shader = new_shader;
        }

        // Remember the current shader's write time.
        *previous_write_time = current_write_time;
        return true;
    }
    return false;
}

 bool jfx::hot_reload_pixel_shader(
    PixelShader *shader, char *file_path, FILETIME *previous_write_time, char **macro_defines, uint32_t macro_defines_count
) {
    // Get the latest shader file write time.
    FILETIME current_write_time = file_system::get_last_write_time(file_path);

    // Check if we've seen this shader before. If not, attempt reload.
    if (CompareFileTime(&current_write_time, previous_write_time) != 0) {
        // Try to compile the new shader.
        File shader_file = file_system::read_file(file_path);
        PixelShader new_shader = graphics::get_pixel_shader_from_code(
            (char *)shader_file.data, shader_file.size, macro_defines, macro_defines_count
        );
        file_system::release_file(shader_file);

        // If the compilation was successful, release the old shader and replace with the new one.
        bool reload_success = graphics::is_ready(&new_shader);
        if(reload_success) {
            graphics::release(shader);
            *shader = new_shader;
        }

        // Remember the current shader's write time.
        *previous_write_time = current_write_time;
        return true;
    }
    return false;
}

