// colored_triangle_mesh.vert
#version 450

// Input from vertex buffer (must match your Vertex struct)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in float inUvX;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in float inUvY;
layout(location = 4) in vec4 inColor;

// Push constant (from C++)
layout(push_constant) uniform PushConstants {
    mat4 worldMatrix;
} pc;

// Outputs to fragment shader
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;

void main()
{
    gl_Position = pc.worldMatrix * vec4(inPosition, 1.0);

    outColor = inColor;
    outUV = vec2(inUvX, inUvY);
}