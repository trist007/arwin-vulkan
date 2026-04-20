#pragma once

#include <stdint.h>
#include <stddef.h>

// ################################################################################
// Arena
// ################################################################################

struct Arena
{
    uint8_t  *base;
    size_t   size;
    size_t   offset;
};

void arenaInit(Arena *a, void *buf, size_t size);

void * arenaAlloc(Arena *a, size_t s);