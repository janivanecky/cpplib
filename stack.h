#pragma once
#include <stdint.h>
#include <string.h>
#include <cassert>
#include "memory.h"

template <typename T>
struct Stack
{
    T *data;
    uint32_t top;
    uint32_t size;
};

namespace stack
{
    template <typename T>
    void init(Stack<T> *stack, uint32_t size)
    {
        stack->size = size;
        stack->top = 0;
        stack->data = memory::alloc_heap<T>(size);
        assert(stack->data);
    }

    template <typename T>
    Stack<T> get(uint32_t size)
    {
        Stack<T> stack;
        stack::init(&stack, size);
        return stack;
    }

    template <typename T>
    void reset(Stack<T> * stack)
    {
        stack->top = 0;
    }

    template <typename T>
    void push(Stack<T> *stack, T item)
    {
        if(stack->size == stack->top)
        {
            T *new_mem = memory::alloc_heap<T>(stack->size * 2);
            assert(new_mem);
            memcpy(new_mem, stack->data, sizeof(T) * stack->size);
            memory::free_heap(stack->data);
            stack->data = new_mem;
            stack->size *= 2;
        }

        stack->data[stack->top++] = item;
    }

    template <typename T>
    T pop(Stack<T> *stack)
    {
        assert(stack->top > 0);
        return stack->data[stack->top--];
    }
}
