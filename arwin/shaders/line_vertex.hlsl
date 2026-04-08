cbuffer Uniforms : register(b0, space1)
{
    float4x4 vp;
};

float4 main(float3 pos : TEXCOORD0) : SV_Position
{
    //return mul(float4(pos, 1.0), mvp);
    return mul(vp, float4(pos, 1.0));
}

// compile: dxc -T vs_6_0 -E main line_vertex.hlsl -Fo line_vertex.dxil