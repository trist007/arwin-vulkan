struct VSOutput
{
    float4  pos : SV_Position;
    float2   uv : TEXCOORD0;
};

VSOutput main(uint id : SV_VertexID)
{
    float2 pos = float2((id == 1) ? 3.0 : -1.0,
                        (id == 2) ? 3.0 : -1.0);
    VSOutput o;
    o.pos = float4(pos, 0.0, 1.0);
    o.uv = pos * float2(0.5, -0.5) + 0.5;
    return o;
}

// compile: dxc -T vs_6_0 -E main bg_vertex.hlsl   -Fo bg_vertex.dxil