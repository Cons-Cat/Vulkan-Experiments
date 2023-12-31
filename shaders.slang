/*
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
float4 composite_vertex_main(uint32_t id : SV_VertexID) : SV_Position {
    // Supposedly this compiles better than the bit shifting soluion.
    float4 position;
    position.x = (id == 2) ?  3.0 : -1.0;
    position.y = (id == 1) ? -3.0 :  1.0;
    position.zw = float2(1.0,1.0);
    return position;
}

[shader("fragment")]
float4 composite_fragment_main(float4 color) : SV_Target0
{
    return color;
}

*/
// demo ///////////////////////////////////////////////////////////////////////

struct moo {
    float4 a;
}

[vk::binding(0, 0)]
RWStructuredBuffer<moo> buffer;

// Generate a demo triangle.
[shader("vertex")]
float4 demo_vertex_main(
    in uint32_t vertex : SV_VertexID,
    out float3 color
    )
: SV_Position {
    float2 positions[3] = {
        float2 (0.0, -0.5), float2 (0.5, 0.5), float2 (-0.5, 0.5),
    };

    float3 colors[3] = {
       float3 (1.0, 0.0, 0.0), float3 (0.0, 1.0, 0.0), float3 (0.0, 0.0, 1.0),
    };

    //color = buffer[0].a.rgb;

    //buffer[0].a = {1,1,1,1};
    color = colors[vertex];

    // Extract hard coded triangle position.
    return float4(positions[vertex],0,1);
}

struct frag_out {
    [vk::location(0)]
    float4 color : SV_Target0;

    [vk::location(1)]
    float4 normal : SV_Target1;

    [vk::location(2)]
    uint32_t id : SV_Target2;
};

[shader("fragment")]
frag_out demo_fragment_main(in float3 color) {
    frag_out output = {};
    output.color = float4(color, 1);
    output.normal = float4(color.xy, 0, 1);
    output.id = 32000;
    return output;
}

[vk::binding(1, 0)]
Sampler2D<float4> color_texture;

[vk::binding(2, 0)]
Sampler2D<float4> normal_texture;

[vk::binding(3, 0)]
Sampler2D<uint32_t> id_texture;

// Generate a triangle that covers the screen.
[shader("vertex")]
float4 composite_vertex_main(uint32_t vertex : SV_VertexID)
: SV_Position {
    float2 uv = float2((vertex << 1) & 2, vertex & 2);
    //return float4(uv, 0, 1);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}

[shader("fragment")]
float4 composite_fragment_main(float2 uv : SV_Position)
: SV_Target0 {
    // return color_texture.Sample(uv);

    return color_texture.Load(int3(uv.x, uv.y, 0));
}
