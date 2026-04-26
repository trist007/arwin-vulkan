#include "arena.h"

void
arenaInit(Arena *a, void *buf, size_t size)
{
    a->base   = (uint8_t *)buf;
    a->size   = size;
    a->offset = 0;
}

void *
arenaAlloc(Arena *a, size_t s)
{
    if(a->offset + s > a->size)
        return NULL;
    void *ptr = a->base + a->offset;
    a->offset += s;
    
    return ptr;
}