#include "file_system.h"
#include "logging.h"
#include <windows.h>

namespace file_system
{
    File read_file(char *path)
    {
        File file = {};

        HANDLE file_handle = CreateFileA(path, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            logging::print_error("Unable to open read handle to file %s.", path);
            return file;
        } 

        WIN32_FILE_ATTRIBUTE_DATA file_attributes;
        if (!GetFileAttributesExA(path, GetFileExInfoStandard, &file_attributes))
        {
            logging::print_error("Unable to get attributes of file %s.", path);
            CloseHandle(file_handle);
            return file;
        }

        uint32_t file_size = file_attributes.nFileSizeLow;
        HANDLE heap = GetProcessHeap();
        file.data = HeapAlloc(heap, HEAP_ZERO_MEMORY, file_size);
        DWORD bytes_read_from_file = 0;
        if (ReadFile(file_handle, file.data, file_size, &bytes_read_from_file, NULL) &&
            bytes_read_from_file == file_size)
        {
            file.size = bytes_read_from_file;
        }
        else
        {
            logging::print_error("Unable to read opened file %s.", path);
            HeapFree(heap, 0, file.data);
            CloseHandle(file_handle);
            return file;
        }

        CloseHandle(file_handle);
        return file;
    }

    void release_file(File file)
    {
        HANDLE heap = GetProcessHeap();
        HeapFree(heap, 0, file.data);
    }

    uint32_t write_file(char *path, void *data, uint32_t size)
    {
        HANDLE file_handle = 0;
        file_handle = CreateFileA(path, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE)
        {
            logging::print_error("Unable to open write handle to file %s.", path);
            return 0;
        }

        DWORD bytes_written = 0;
        if(!WriteFile(file_handle, data, size, &bytes_written, NULL))
        {
            logging::print_error("Unable to write to file %s.", path);
            return 0;
        }
        
        CloseHandle(file_handle);
        return bytes_written;
    }
}
