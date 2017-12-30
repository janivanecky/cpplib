#pragma once
#include <stdint.h>
#include <string.h>
#include <cassert>
#include "memory.h"

// Array is a data struct used as a dynamically allocated list.
template <typename T>
struct Array
{
    T *data;
    uint32_t count;
    uint32_t size;

    T& operator[](uint32_t index)
    {
        return (this->data[index]);
    }
};


// `array` namespace handles operations on Array.
namespace array
{
    // array_expansion_ratio defines the ratio of new array size to old array size when reallocating.
    const uint32_t array_expansion_ratio = 2;
    
    //  Initialize Array
    template <typename T>
    void init(Array<T> *array, uint32_t size)
    {
        array->size = size;
        array->count = 0;
        array->data = memory::alloc_heap<T>(size);
        assert(array->data);
    }

    //  Returns initialized Array
    template <typename T>
    Array<T> get(uint32_t size)
    {
        Array<T> array;
        array::init(&array, size);
        return array;
    }

    // Reset the array, does not deallocate allocated memory
    template <typename T>
    void reset(Array<T> *array)
    {
        array->count = 0;
    }

    // Add an element into array, possibly reallocate and move all the contents
    template <typename T>
    void add(Array<T> *array, T item)
    {
        // In case the array is full, reallocate
        if(array->size == array->count)
        {
            T *new_mem = memory::alloc_heap<T>(array->size * array_expansion_ratio);
            assert(new_mem);
            memcpy(new_mem, array->data, sizeof(T) * array->size);
            memory::free_heap(array->data);
            array->data = new_mem;
            array->size *= array_expansion_ratio;
        }

        array->data[array->count++] = item;
    }
}