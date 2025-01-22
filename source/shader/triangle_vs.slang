

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR;
};

struct VertexInput
{
    float2 position : POSITION;
    float3 color : COLOR;
}

[shader("vertex")]
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.sv_position = float4(input.position, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}