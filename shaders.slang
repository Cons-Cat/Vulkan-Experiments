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

struct entity {
    float3 world_position;
    quaternion rotation;
};

[vk::binding(0, 0)]
StructuredBuffer<entity> buffer;

static constexpr float4x4 identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

[shader("vertex")]
vertex_output_t vertex_main(vertex_t vertex) {
    float3 position = vertex.position;
    float3 normal = vertex.normal;
    float2 uv = vertex.uv;
    float4 color = vertex.color;

    float4x4 view_proj = mul(camera.projection, camera.view);

    vertex_output_t output;
    output.normal = normal;
    output.uv = uv;
//    output.color=color;
    output.position = mul(view_proj, mul(identity, float4(position, 1)));
    output.color = float4(1,1,1, 1);
//    output.position = float4(position, 1);

    return output;
}

[shader("fragment")]
float4 fragment_main(vertex_output_t vertex) : SV_Target {
    float3 color = float3(1, 0.5, 0.5);
    return float4(color, 1);
}
