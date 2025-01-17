

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR;
};

static const float2 vertex[3] =
{
    float2(0.0f, 0.5f),
    float2(0.5f, 0.5f),
    float2(0.5f, 0.5f)
};

static const float3 colors[3] =
{ 
    float3(1.0f, 0.0f, 0.0f),
    float3(0.0f, 1.0f, 0.0f),
    float3(0.0f, 0.0f, 1.0f)
};

[shader("vertex")]
VertexOutput main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.sv_position = float4(vertex[vertex_id], 0.0f, 1.0f);
    output.color = colors[vertex_id];
    return output;
}