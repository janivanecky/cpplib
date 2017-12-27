#pragma once
#include <stdint.h>
#include <string.h>
#include <cassert>
#include "memory.h"

template <typename T>
struct Array
{
    T *data;
    uint32_t count;
    uint32_t size;
};

namespace array
{
    template <typename T>
    void init(Array<T> *array, uint32_t size)
    {
        array->size = size;
        array->count = 0;
        array->data = memory::alloc_heap<T>(size);
        assert(array->data);
    }

    template <typename T>
    Array<T> get(uint32_t size)
    {
        Array<T> array;
        array::init(&array, size);
        return array;
    }

    template <typename T>
    void reset(Array<T> *array)
    {
        array->count = 0;
    }

    template <typename T>
    void add(Array<T> *array, T item)
    {
        if(array->size == array->count)
        {
            T *new_mem = memory::alloc_heap<T>(array->size * 2);
            assert(new_mem);
            memcpy(new_mem, array->data, sizeof(T) * array->size);
            memory::free_heap(array->data);
            array->data = new_mem;
            array->size *= 2;
        }

        array->data[array->count++] = item;
    }
}

