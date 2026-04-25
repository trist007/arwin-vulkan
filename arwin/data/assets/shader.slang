// =============================================
// Vertex Shader
// =============================================
struct VSInput {
    float3 Pos      : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 Texcoord : TEXCOORD2;
    float4 Color    : TEXCOORD3;
    uint4  Joints   : TEXCOORD4;
    float4 Weights  : TEXCOORD5;
};

struct VSOutput {
    float4 Pos      : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float4 Color    : TEXCOORD2;
};

struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model;
    float4 lightPos;
    float4 baseColorFactor[8];
    uint32_t selected;
    float4x4 skinMatrices[64];
};

[[vk::binding(0, 0)]]
ConstantBuffer<ShaderData> shaderData;

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 skinnedPos = float4(input.Pos, 1.0);
    float3 skinnedNormal = input.Normal;

    output.Pos = mul(shaderData.projection, mul(shaderData.view, skinnedPos));
    output.WorldPos = skinnedPos.xyz;
    output.Normal = normalize(skinnedNormal);
    output.Color = input.Color;

    return output;
}

// =============================================
// Fragment Shader
// =============================================
[[vk::push_constant]]
float4 pushColor;

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    float3 albedo = pushColor.rgb;

    float3 N = normalize(input.Normal);
    float3 L = normalize(shaderData.lightPos.xyz - input.WorldPos);
    float diff = max(dot(N, L), 0.25);

    float3 finalColor = albedo * diff * 1.6;

    finalColor = pow(finalColor, 1.0 / 2.2);

    return float4(finalColor, 1.0);
}