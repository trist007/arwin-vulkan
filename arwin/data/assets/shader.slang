/* Copyright (c) 2025-2026, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

 struct VSInput {
    float3 Pos;
    float3 Normal;
    float2 Texcoord;
};

struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model[3];
    float4 lightPos;
    uint32_t selected;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 Texcoord;
    float3 Factor;
    float3 LightVec;
    float3 ViewVec;
    uint32_t InstanceIndex;
};

Sampler2D textures[];

// Correct way: Use ConstantBuffer for uniform data
[[vk::binding(1, 0)]]
ConstantBuffer<ShaderData> shaderData;

[shader("vertex")]
VSOutput main(VSInput input, uint instanceIndex : SV_VulkanInstanceID)
{
    VSOutput output;

    float4x4 modelMat = shaderData.model[instanceIndex];

    output.Normal = mul((float3x3)mul(shaderData.view, modelMat), input.Normal);
    output.Texcoord = input.Texcoord;
    output.Pos = mul(shaderData.projection, mul(shaderData.view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
    output.Factor = (shaderData.selected == instanceIndex ? 3.0f : 1.0f);
    output.InstanceIndex = instanceIndex;

    float4 fragPos = mul(mul(shaderData.view, modelMat), float4(input.Pos.xyz, 1.0));
    output.LightVec = shaderData.lightPos.xyz - fragPos.xyz;
    output.ViewVec = -fragPos.xyz;

    return output;
}

[shader("fragment")]
float4 main(VSOutput input)
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(input.LightVec);
    float3 V = normalize(input.ViewVec);
    float3 R = reflect(-L, N);

    float3 diffuse = max(dot(N, L), 0.0025);
    float3 specular = pow(max(dot(R, V), 0.0), 16.0) * 0.75;

    float3 color = textures[NonUniformResourceIndex(input.InstanceIndex)].Sample(input.Texcoord).rgb * input.Factor;

    return float4(diffuse * color.rgb + specular, 1.0);
}