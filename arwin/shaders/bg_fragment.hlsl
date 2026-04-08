Texture2D tex : register(t0, space2);
SamplerState samp : register(s0, space2);

cbuffer Uniforms : register(b0, space3)
{
    float ambientLight; // 0.0 = pitch black, 1.0 = full brightness
    float3 lightPos;
    float lightRadius;
};

struct PSInput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float4 color = tex.Sample(samp, input.uv);
    
    float worldX = (input.uv.x - 0.5f) * 10.0f;
    float worldY = (input.uv.y - 0.5f) * 10.0f;
    
    float dx = input.uv.x - lightPos.x;
    float dy = input.uv.y - lightPos.y;
    float dist = sqrt(dx*dx + dy*dy);
    
    float attenuation = clamp(1.0f - (dist / lightRadius), 0.0f, 1.0f);
    attenuation *= attenuation;
    
    float lighting = ambientLight + (1.0f - ambientLight) * attenuation;
    
    return float4(color.rgb * lighting, color.a);
}
// compile: dxc -T ps_6_0 -E main bg_fragment.hlsl -Fo bg_fragment.dxil