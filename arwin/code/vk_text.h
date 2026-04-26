#ifndef VK_TEXT_H
#define VK_TEXT_H

#include "vk_types.h"
#include "stb_truetype.h"

typedef struct Glyph
{
    float u0, v0, u1, v1;   // UV coordinates in atlas
    float width, height;    // pixel size
    float xoff;             // offset from cursor position horizontally
    float yoff;             // offset from baseline to top 
    float yoff2;            // offset from baseline to bottom
    float xoff2;
    float advance;          // better than just width
} Glyph;

typedef struct GameFont {
    unsigned int textureID;
    stbtt_bakedchar cdata[96];   // 32 to 127
    int atlasWidth;
    int atlasHeight;
} GameFont;

typedef struct FontAtlas
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkSampler sampler;

    int atlasWidth;
    int atlasHeight;
    int glyphHeight;

    struct Glyph glyphs[128];      // ASCII 32-127

    float baseline;
    float lineHeight;
} FontAtlas;

typedef struct TextVertex
{
    HMM_Vec2 position;   // screen space (-1 to 1)
    HMM_Vec2 uv;
} TextVertex;

#endif