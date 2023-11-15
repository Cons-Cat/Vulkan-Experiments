import quaternion;

struct vertex_t {
    float3 position : POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
};

struct vertex_output_t {
    float4 position : SV_Position;
    float4 color    : COLOR;
    float2 uv       : UV;
    float3 normal   : NORMAL;
}

struct camera_consts {
    float4x4 view;
    float4x4 projection;
};

[vk::push_constant]
camera_consts camera;

// Align to 32 bytes.
struct entity_t {
    float3 world_transform;
    quaternion rotation;
};

[vk::binding(0, 0)]
ByteAddressBuffer buffer;

static constexpr float4x4 identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

[shader("vertex")]
vertex_output_t vertex_main(vertex_t vertex) {
    float3 local_position = vertex.position;
    float3 normal = vertex.normal;
    float2 uv = vertex.uv;
    float4 color = vertex.color;
    float3 world_transform = buffer.Load<entity_t>(0).world_transform;

    float4x4 view_proj = mul(camera.projection, camera.view);
    float4x4 translation = {
        {1,0,0,world_transform.x},
        {0,1,0,world_transform.y},
        {0,0,1,world_transform.z},
        {0,0,0,1}
    };

    vertex_output_t output;
    output.normal = normal;
    output.uv = uv;
    output.color = color;
    output.position = mul(view_proj, mul(translation, float4(local_position, 1)));

    return output;
}

[shader("fragment")]
float4 fragment_main(vertex_output_t vertex) : SV_Target {
    float3 color = float3(1, 0.5, 0.5);
    return float4(color, 1);
}
