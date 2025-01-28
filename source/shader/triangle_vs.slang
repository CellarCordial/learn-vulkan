
[[vk::binding(0, 0)]]
cbuffer constant : register(b0)
{
    float4x4 world_matrix;
    float4x4 view_proj;
};

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR;
    float2 uv : UV;
};

struct VertexInput
{
    float2 position : POSITION;
    float3 color : COLOR;
    float2 uv : UV;
}

[shader("vertex")]
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.sv_position = mul(float4(input.position, 0.0f, 1.0f), mul(world_matrix, view_proj));
    output.color = input.color;
    output.uv = input.uv;
    output.sv_position.xy *= 2.0f;
    return output;
}