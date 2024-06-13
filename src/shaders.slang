struct camera {
    float4x4 mat;
};

struct buffer_storage {
    // This matches `buffer_storage` in `main.cpp`:
    static const uint cameras_offset = 64u;
    static const uint vertices_offset = 256u;
    static const uint member_stride = 4u;
    typedef uint member_type;

    [mutating]
    void set_at<T>(T value, uint byte_offset){
        buffer.Store<T>(byte_offset, value);
    }

    T get_at<T>(uint byte_offset) {
        return buffer.Load<T>(byte_offset);
    }

    float4 get_vertex(uint index) {
        return get_at<float4>(vertices_offset + (index * sizeof(float4)));
    }

    uint get_index(uint index) {
        return get_at<uint>(get_index_offset() + (index * sizeof(uint)));
    }

    struct property {
        float3 position;
        float3 rotation;
        uint id;
    };

    property get_property(uint index) {
        return get_at<property>(get_properties_offset() + (index * 80));
    }

    [mutating]
    void set_vertex_count(uint index, member_type new_vertex_count) {
        return set_at<member_type>(0, new_vertex_count);
    }

    uint get_vertex_count(uint index) {
        return get_at<member_type>(0);
    }

    [mutating]
    void set_index_count(uint index, member_type new_count) {
        return set_at<member_type>(member_stride, new_count);
    }

    uint get_index_count() {
        return get_at<member_type>(member_stride);
    }

    [mutating]
    void set_index_offset(uint index, member_type new_offset) {
        return set_at<member_type>(member_stride * 2, new_offset);
    }

    uint get_index_offset() {
        return get_at<member_type>(member_stride * 2);
    }

    [mutating]
    void set_instance_offset(uint index, member_type new_offset) {
        return set_at<member_type>(member_stride * 5, new_offset);
    }

    uint get_instance_offset() {
        return get_at<member_type>(member_stride * 5);
    }
    

    [mutating]
    void set_properties_offset(uint index, member_type new_offset) {
        return set_at<member_type>(member_stride * 6, new_offset);
    }

    uint get_properties_offset() {
        return get_at<member_type>(member_stride * 6);
    }
    
    [mutating]
    void set_view_matrix(float4x4 mat) {
        return set_at<float4x4>(mat, cameras_offset);
    }

    float4x4 get_view_matrix() {
        return get_at<float4x4>(cameras_offset);
    }

    [mutating]
    void set_proj_matrix(float4x4 mat) {
        return set_at<float4x4>(mat, cameras_offset + sizeof(float4x4));
    }

    float4x4 get_proj_matrix() {
        return get_at<float4x4>(cameras_offset + sizeof(float4x4));
    }

    [vk::binding(0, 0)]
    RWByteAddressBuffer buffer;
};

buffer_storage g_bindless;

struct vs_in {
    [vk::location(0)]
    float4 model_pos : POSITION0;
    
    [vk::location(1)]
    float3 instance_pos : POSITION1;
    
    [vk::location(2)]
    float4 instance_rot;

    [vk::location(3)]
    float3 instance_scale : POSITION1;
    
    [vk::location(4)]
    uint id;
};

struct vs_out {
    float4 position : SV_Position;
    float3 xyz;
    float3 color;
    uint id;
}

// Quaternion multiplication
// http://mathworld.wolfram.com/Quaternion.html
float4 qmul(float4 q1, float4 q2)
{
    return float4(
        q2.xyz * q1.w + q1.xyz * q2.w + cross(q1.xyz, q2.xyz),
        q1.w * q2.w - dot(q1.xyz, q2.xyz)
    );
}

// Vector rotation with a quaternion
// http://mathworld.wolfram.com/Quaternion.html
float3 rotate_vector(float3 v, float4 r)
{
    float4 r_c = r * float4(-1, -1, -1, 1);
    return qmul(r, qmul(float4(v, 0), r_c)).xyz;
}

[shader("vertex")]
vs_out demo_vertex_main(in vs_in vert,
                        in uint invocation_index : SV_VertexID,
                        in uint instance_index : SV_InstanceID) {
    // Bring vertex into projection space:
    float4x4 view = g_bindless.get_view_matrix();
    float4x4 proj = g_bindless.get_proj_matrix();
    float4x4 view_proj = mul(proj, view);

    float4 out_pos = vert.model_pos;
    out_pos.xyz *= vert.instance_scale;
    // If no rotation is provided, the quaternion is 0.
    if (vert.instance_rot.w != 0) {
        out_pos.xyz = rotate_vector(out_pos.xyz, vert.instance_rot);
    }
    out_pos.xyz += vert.instance_pos;
    
    float3 xyz = out_pos.xyz;
    out_pos = mul(view_proj, out_pos);

    float3 colors[3] = {
        float3 (1.0, 0.0, 0.0), float3 (0.0, 1.0, 0.0), float3 (0.0, 0.0, 1.0),
    };

    const float3 color = colors[invocation_index % 3];

    return {out_pos, xyz, color, vert.id};
}

struct frag_out {
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 xyz : SV_Target2;
    uint id : SV_Target3;
};

[shader("fragment")]
frag_out demo_fragment_main(in vs_out input) {
    frag_out output = {};
    output.color = float4(input.color, 1);
    output.normal = float4(input.color.xy, 0, 1);
    output.xyz = float4(input.xyz, 1);
    output.id = input.id;

    return output;
}

[shader("compute")]
[numthreads(4,1,1)]
void culling_main(uint3 sv_dispatchThreadID : SV_DispatchThreadID) {
    printf("Message from shader!");
    return;
}

[vk::binding(1, 0)]
Sampler2D<float4> color_textures[];

[vk::binding(1, 0)]
Sampler2D<uint> id_texture[4];

[vk::binding(1, 0)]
Sampler2D<float> depth_texture[5];

// Generate a triangle that covers the screen.
[shader("vertex")]
float4 composite_vertex_main(uint vertex : SV_VertexID)
: SV_Position {
    float2 uv = float2((vertex << 1) & 2, vertex & 2);
    float4 color = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return color;
}

uint test_adjacency(uint3 coord, uint self, float depth) {
    if (depth_texture[4].Load(coord) > depth) {
        uint other = id_texture[3].Load(coord);
        return self != other;
    }
    return 0;
}

[shader("fragment")]
float4 composite_fragment_main(float2 uv : SV_Position) : SV_Target0 {
    const uint x = uint(uv.x);    
    const uint y = uint(uv.y);

    uint self = id_texture[3].Load(uint3(x, y, 0));

    uint adjacencies = 0;
    float depth = depth_texture[4].Load(uint3(x, y, 0));

    // Left adjacency.
    if (x > 0) {
        adjacencies += test_adjacency({x - 1, y, 0}, self, depth);
    }

    // Right adjacency.
    if (x < 480) {
        adjacencies += test_adjacency({x + 1, y, 0}, self, depth);
    }
    
    // Top adjacency.
    if (y > 0) {
        adjacencies += test_adjacency({x, y - 1, 0}, self, depth);
    }
    
    // Bottom adjacency.
    if (y < 320) {
        adjacencies += test_adjacency({x, y + 1, 0}, self, depth);
    }
    
    // Draw a solid white outline.
    if (adjacencies > 0) {
        return float4(1, 1, 1, 1);
    }

    return color_textures[0].Load(uint3(uint(uv.x), uint(uv.y), 0));
}
