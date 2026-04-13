#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D displayTexture;

layout(location = 0) out vec4 outFragColor;

void main()
{
    vec4 texColor = texture(displayTexture, inUV);
    outFragColor = texColor * inColor;     // multiply texture with vertex color
}