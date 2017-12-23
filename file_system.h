#pragma once
#include <stdint.h>

struct File
{
    void *data;
    uint32_t size;
};

namespace file_system
{
    File read_file(char *path);
    void release_file(File file);
    uint32_t write_file(char *path, void *data, uint32_t size);
}

