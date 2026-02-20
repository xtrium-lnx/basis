struct VSInput
{
    float3 position;
    float3 normal;
    float2 uv;
    float3 tangent;
}

struct VSOutput
{
    float4 position : SV_Position;

    float3 worldspacePosition;
    float3 normal;
    float3 eyePos;
};

struct PushConstantData
{
    float4x4 modelMatrix;
};

[[vk::push_constant()]]
ConstantBuffer<PushConstantData> pushConstantData;

/**
 * Slang provides this nice thing called "parameter blocks".
 * Long story short, they encapsulate descriptor sets. Anything that's not a Sampler, a ConstantBuffer or a StorageBuffer
 * is put in their own uniform buffer at binding 0.
 * Textures and buffers are put in the next bindings, in order.
 * ParameterBlocks take set IDs in order too, so the first ParameterBlock is set 0, the next is set 1, etc.
 * Here we have set 0 using only one binding (binding 0) for a uniform buffer containing two matrices.
 */
struct CameraMatrices
{
    float4x4 view;
    float4x4 viewInverse;
    float4x4 projection;
};
ParameterBlock<CameraMatrices> camera;

[shader("vertex")]
VSOutput vs_main(VSInput input)
{
    VSOutput output;

    float4 wsPos = mul(pushConstantData.modelMatrix, float4(input.position, 1.0f));
    output.position = mul(camera.projection, mul(camera.view, wsPos));

    output.worldspacePosition = wsPos.xyz;
    output.normal             = input.normal;
    output.eyePos             = mul(camera.viewInverse, float4(0.0f.xxx, 1.0f)).xyz;
    return output;
}

// ----------------------------------------------------------------------------

[shader("fragment")]
float4 fs_main(VSOutput input)
    : SV_Target0
{
    float3 L = normalize(1.0f.xxx);
    float3 N = normalize(input.normal);
    float3 V = normalize(input.eyePos - input.worldspacePosition.xyz);
    float3 H = normalize(L + V);

    float diffuse  = 1.0f / 3.14159f;
    float specular = pow(max(0.0f, dot(N, H)), 64.0f);

    float3 result = (diffuse * (1.0f - specular) + float3(specular)) * max(0.0f, dot(N, L));

    return float4(result, 1.0f);
}
