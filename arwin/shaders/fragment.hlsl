cbuffer FragUniforms : register(b0, space3)
{
    float4 color;
    float  ambientLight;
    float3 lightPos;
    float  lightRadius;
};

struct PSInput
{
    float4       pos : SV_Position;
    float3  worldPos : TEXCOORD0;
    float3    normal : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target
{
    //return color * ambientLight;
    float3 toLight = lightPos  - input.worldPos;
    float dist = length(toLight);
    float3 L = normalize(toLight);
    
    float atten = saturate(1.0 - (dist / lightRadius));
    atten *= atten;
    
    float diff = saturate(dot(normalize(input.normal), L));
    
    float3 lighting = ambientLight + diff *atten;
    return float4(color.rgb * lighting, 1.0);
}

// compile: dxc -T ps_6_0 -E main fragment.hlsl -Fo fragment.dxil