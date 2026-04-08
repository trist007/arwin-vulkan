cbuffer Light : register(b0, space3)
{
    float4 color;
    float4 lightPosAndAmbient;
    float4 lightRadiusPad;
};

float4 main(float4 pos : SV_Position, float3 worldPos : TEXCOORD0, float3 normal : TEXCOORD1) : SV_Target
{
    float3 lightPos   = lightPosAndAmbient.xyz;
    float ambient     = lightPosAndAmbient.w;
    float radius      = lightRadiusPad.x;
    
    float3 n          = normalize(normal);
    float3 toLight    = lightPos - worldPos;
    float dist        = length(toLight);
    float3 l          = toLight / dist;
    
    float atten       = 1.0 - clamp(dist / radius, 0.0, 1.0);
    float diffuse     = max(dot(n, l), 0.0) * atten;
    
    float3 lit        = color.rgb * (ambient + diffuse);
    
    return float4(lit, 1.0);
}
