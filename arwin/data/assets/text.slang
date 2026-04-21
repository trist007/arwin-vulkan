struct VSInput {
    float2 Position : TEXCOORD0;
    float2 UV       : TEXCOORD1;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Pos = float4(input.Position, 0.0, 1.0);
    output.UV  = input.UV;
    return output;
}

[[vk::binding(0, 0)]]
Sampler2D fontTexture;

[[vk::push_constant]]
float4 pushColor;

[shader("fragment")]
float4 PSMain(VSOutput input) : SV_Target
{
    float alpha = fontTexture.Sample(input.UV).r;

    // Boost and sharpen the alpha significantly
    alpha = step(0.2, alpha * 3.5);

    // Premultiplied alpha (important for correct blending)
    return float4(pushColor.rgb, alpha);
}