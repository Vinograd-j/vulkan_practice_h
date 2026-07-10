struct VSInput {
    float2 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color;
    float2 fragTexCoord;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};

ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VertexOutput vertMain(VSInput input) {
    VertexOutput output;

    output.position = mul(ubo.projection, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(inVert.color * texture.Sample(inVert.fragTexCoord).rgb, 1.0);
}