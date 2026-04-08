float4 main() : SV_Target
{
    return float4(1, 1, 0,1); // yellow
}

// compile: dxc -T ps_6_0 -E main line_fragment.hlsl -Fo line_fragment.dxil