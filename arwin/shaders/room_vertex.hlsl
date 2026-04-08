cbuffer Uniforms : register(b0, space1)
{
    float4x4 vp;
};

struct VSOut
{
    float4       pos : SV_Position;
    float3  worldPos : TEXCOORD0;
    float3    normal : TEXCOORD1;
};

VSOut main(float3 pos : TEXCOORD0, float3 normal : TEXCOORD1)
{
    VSOut o;
    o.pos      = mul(vp, float4(pos, 1.0));
    o.worldPos = pos;
    o.normal   = normal;
    
    return o;
}
