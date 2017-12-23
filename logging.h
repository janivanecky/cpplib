#pragma once
#include <stdint.h>

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define print_error(format, ...) print_error_with_location(format, __FILENAME__, __LINE__, __VA_ARGS__)

namespace logging
{
	void print(char *format, ...);
	void print_error_with_location(char *format, char *filename, uint32_t line, ...);
}