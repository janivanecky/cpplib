#pragma once
#include <stdio.h>

#define log_print(s, ...) {printf(s, __VA_ARGS__); printf("\n");}
#define log_error(s, ...) {printf("ERROR in file %s on line %d: ", __FILE__, __LINE__); printf(s, __VA_ARGS__); printf("\n");}