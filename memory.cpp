#include "memory.h"
#include <cstdlib>

static StackAllocator allocator_temp = memory::get_stack_allocator(MEGABYTES(10));

StackAllocator memory::get_stack_allocator(uint32_t size)
{
    StackAllocator allocator = {};
    allocator.storage = malloc(size);
    allocator.size = size;
    return allocator;
}

StackAllocatorState memory::save_stack_state(StackAllocator *allocator)
{
    return allocator->top;
}

void memory::load_stack_state(StackAllocator *allocator, StackAllocatorState state)
{
    allocator->top = state;
}

StackAllocator *memory::get_temp_stack()
{
    return &allocator_temp;
}

void memory::free_temp()
{
    memory::get_temp_stack()->top = 0;
}

void memory::free_heap(void *ptr)
{
    free(ptr);
}
