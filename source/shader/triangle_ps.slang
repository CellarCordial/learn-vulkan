

struct VertexOutput
{
    float4 sv_position : SV_Position;
    float3 color : COLOR;
    float2 uv : UV;
};

[vk_binding(1, 0)]
Texture2D test_texture : register(t0);

[vk_binding(2, 0)]
SamplerState linear_wrap_sampler : register(s0);


[shader("pixel")]
float4 main(VertexOutput input) : SV_Target0
{
    return test_texture.Sample(linear_wrap_sampler, input.uv) + float4(input.color, 1.0f) * 0.2f;
}