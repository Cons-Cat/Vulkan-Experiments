struct camera {
    float4x4 mat;
};

struct buffer_storage {
    void set_at<T>(T t, uint byte_offset){
        buffer.Store<T>(byte_offset, t);
    }

    T get_at<T>(uint byte_offset){
        return buffer.Load<T>(byte_offset);
    }

    // void set_vertex(uint index) {
    //     return set_at<float4>(vertices_offset + (index * 16), 0);
    // }

    float4 get_vertex(uint index) {
        return get_at<float4>(vertices_offset + (index * 16));
    }

    // void set_index(uint index) {
    //     return set_at<float4>(vertices_offset + (index * 16), 0);
    // }

    uint get_index(uint index) {
        return get_at<uint>(get_index_offset() + (index * 4));
    }

    void set_vertex_count(uint index, member_type new_vertex_count) {
        return set_at<member_type>(0, new_vertex_count);
    }

    uint get_vertex_count(uint index) {
        return get_at<member_type>(0);
    }

    void set_index_count(uint index, member_type new_count) {
        return set_at<member_type>(member_stride, new_count);
    }

    uint get_index_count() {
        return get_at<member_type>(member_stride);
    }

    void set_index_offset(uint index, member_type new_offset) {
        return set_at<member_type>(member_stride * 2, new_offset);
    }

    uint get_index_offset() {
        return get_at<member_type>(member_stride * 2);
    }

    RWByteAddressBuffer buffer;
    
    // This matches `buffer_storage` in `main.cpp`:
    static const uint vertices_offset = 64;
    static const uint member_stride = 4;
    typedef uint member_type;
};

[vk::binding(0, 0)]
buffer_storage bindless;

// Generate a demo triangle.
[shader("vertex")]
float4 demo_vertex_main(
    in uint32_t vertex_idx : SV_VertexID,
    out float3 color
    )
: SV_Position {
    uint index = bindless.get_index(vertex_idx);
    float4 vert = bindless.get_vertex(index);

    float3 colors[3] = {
        float3 (1.0, 0.0, 0.0), float3 (0.0, 1.0, 0.0), float3 (0.0, 0.0, 1.0),
    };

    color = colors[vertex_idx % 3];

    // Extract hard coded triangle position.
    return vert;
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

    return color_texture.Load(uint3(uint(uv.x), uint(uv.y), 0));
}
