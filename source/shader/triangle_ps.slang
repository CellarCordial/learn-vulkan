

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR;
};

[shader("pixel")]
float4 main(VertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0f);
}