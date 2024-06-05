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
    return float4(positions[vertex], 0, 1);
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
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}

[shader("fragment")]
float4 composite_fragment_main(float2 uv : SV_Position)
: SV_Target0 {
    uint adjacencies = 0;
    
    const uint32_t x = uint(uv.x);    
    const uint32_t y = uint(uv.y);

    uint32_t self = id_texture.Load(uint3(x, y, 0));

    // Left adjacency.
    if (x > 0) {
       uint32_t other = id_texture.Load(uint3(x - 1, y, 0));
       if (self != other) {
           adjacencies += 1;
       }
    }

    // Right adjacency.
    if (x < 480) {
        uint32_t other = id_texture.Load(uint3(x + 1, y, 0));
        if (self != other) {
            adjacencies += 1;
        }
    }
    
    // Top adjacency.
    if (y > 0) {
        uint32_t other = id_texture.Load(uint3(x, y - 1, 0));
        if (self != other) {
            adjacencies += 1;
        }
    }
    
    // Right adjacency.
    if (y < 320) {
        uint32_t other = id_texture.Load(uint3(x, y + 1, 0));
        if (self != other) {
            adjacencies += 1;
        }
    }
    
    if (adjacencies > 0) {
        return float4(1, 1, 1, 1);
    }

    return normal_texture.Load(uint3(uint(uv.x), uint(uv.y), 0));
}
