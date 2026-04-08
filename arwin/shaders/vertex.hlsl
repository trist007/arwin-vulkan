struct VSInput
{
    float3      pos : TEXCOORD0;
    float3   normal : TEXCOORD1;
    int4     joints : TEXCOORD2;
    float4  weights : TEXCOORD3;
};

struct VSOutput
{
    float4       pos : SV_Position;
    float3  worldPos : TEXCOORD0;
    float3    normal : TEXCOORD1;
};

cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp;
    float4x4 skinMatrices[64];
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4x4 skin = input.weights.x * skinMatrices[input.joints.x]
        + input.weights.y * skinMatrices[input.joints.y]
        + input.weights.z * skinMatrices[input.joints.z]
        + input.weights.w * skinMatrices[input.joints.w];
    
    float4 skinnedPos = mul(skin, float4(input.pos, 1.0));
    output.pos = mul(mvp, skinnedPos);
    output.worldPos = skinnedPos.xyz;
    output.normal = mul((float3x3)skin, input.normal);
    
    //output.pos = mul(mvp, mul(skin, float4(input.pos, 1.0)));
    return output;
}

// compile: dxc -T vs_6_0 -E main vertex.hlsl -Fo vertex.dxil
