#pragma once

#include <SDL3_ttf/SDL_ttf.h>
#include "vk_types.h"
#include "stb_truetype.h"

struct Glyph
{
    float u0, v0, u1, v1;   // UV coordinates in atlas
    float width, height;    // pixel size
    float xoff, yoff;       // offset from cursor position
};

struct GameFont {
    unsigned int textureID;
    stbtt_bakedchar cdata[96];   // 32 to 127
    int atlasWidth;
    int atlasHeight;
};
struct FontAtlas
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkSampler sampler;

    int atlasWidth;
    int atlasHeight;
    int glyphHeight;

    Glyph glyphs[128];      // ASCII 32-127
};

struct TextVertex
{
    HMM_Vec2 position;   // screen space (-1 to 1)
    HMM_Vec2 uv;
};