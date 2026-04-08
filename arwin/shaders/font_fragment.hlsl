Texture2D         fontTex : register(t0, space2);
SamplerState  fontSampler : register(s0, space2);

struct PSInput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

cbuffer Color : register(b0, space3)
{
    float4 color;
};

float4 main(PSInput input) : SV_Target
{
    float4 col = fontTex.Sample(fontSampler, input.uv);
    return col * color;
}

// compile: dxc -T ps_6_0 -E main font_fragment.hlsl -Fo font_fragment.dxil