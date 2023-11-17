import quaternion;

struct vertex_t {
    float3 position : POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
    uint32_t id;
};

struct vertex_output_t {
    float4 position : SV_Position;
    float4 color    : COLOR;
    float2 uv       : UV;
    float3 normal   : NORMAL;
    uint32_t id;
}

struct camera_consts {
    float4x4 view;
    float4x4 projection;
};

[vk::push_constant]
camera_consts camera;

// Align to 32 bytes.
struct entity_t {
    float4 world_transform;
    float4 rotation; // make quaternion
};

[vk::binding(0, 0)]
StructuredBuffer<entity_t> buffer;

static constexpr float4x4 identity = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

[shader("vertex")]
vertex_output_t vertex_main(vertex_t vertex) {
    float3 local_position = vertex.position;
    float3 normal = vertex.normal;
    float2 uv = vertex.uv;
    float4 color = vertex.color;
    entity_t entity = buffer.Load(vertex.id);
    float4 world_transform = entity.world_transform;

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
    output.id = vertex.id;

    return output;
}

[shader("fragment")]
void fragment_main(in vertex_output_t vertex, out float4 color : SV_Target0, out uint32_t id : SV_Target1) {
    //float3 color = float3(1, 0.5, 0.5);
    // return vertex.color;
    color = vertex.color;
    id = vertex.id;
    //return float4(color, 1);
}

// Generate a triangle that covers the screen.
[shader("vertex")]
float4 composite_vertex_main(int id : SV_VertexID) : SV_Position {
    float4 position;
    position.x = (id == 2) ?  3.0 : -1.0;
    position.y = (id == 1) ? -3.0 :  1.0;
    position.zw = float2(1.0,1.0);
    return position;
}

[shader("fragment")]
float4 composite_fragment_main(float4 color : SV_Target0)
{
    return 0;
}
