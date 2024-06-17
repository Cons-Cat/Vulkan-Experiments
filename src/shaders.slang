struct light {
    float4x4 transform;
    float4x4 projection;
    float3 position;
};

struct vertex {
    float4 position;
    float3 normal;
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

    vertex get_vertex(uint index) {
        return get_at<vertex>(vertices_offset + (index * sizeof(vertex)));
    }

    uint get_index(uint index) {
        return get_at<uint>(get_index_offset() + (index * sizeof(uint)));
    }

    light get_light(uint index) {
        return get_at<light>(get_lights_offset() + (index * sizeof(light)));
    }

    float4x4 get_viewproj() {
        return mul(get_proj_matrix(), get_view_matrix());
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
    void set_lights_offset(uint index, member_type new_offset) {
        return set_at<member_type>(member_stride * 7, new_offset);
    }

    uint get_lights_offset() {
        return get_at<member_type>(member_stride * 7);
    }

    [mutating]
    void set_camera_position(float3 position) {
        return set_at<float3>(position, cameras_offset - 16);
    }

    float3 get_camera_position() {
        return get_at<float3>(cameras_offset - 16);
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

    float4x4 get_viewproj_matrix() {
        return mul(get_proj_matrix(), get_view_matrix());
    }

    [vk::binding(0, 0)]
    RWByteAddressBuffer buffer;
};

buffer_storage g_bindless;

struct vs_in {
    [vk::location(0)]
    float4 model_pos : POSITION0;
    
    [vk::location(1)]
    float3 normal : NORMAL;

    [vk::location(2)]
    float3 instance_pos;
    
    [vk::location(3)]
    float4 instance_rot;

    [vk::location(4)]
    float3 instance_scale;
    
    [vk::location(5)]
    uint id;
};

struct vs_out {
    float4 position : SV_Position;
    float3 normal;
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
    // TODO: Use constant generics rather than the preprocessor for this.
    // Bring vertex into projection space:
    // `is_camera` is defined in `../CMakeLists.txt`
#ifdef is_camera
    float4x4 view = g_bindless.get_view_matrix();
#else
    float4x4 view = g_bindless.get_light(0).transform;
#endif

    // TODO: Support per-light projections.
    float4x4 proj = g_bindless.get_proj_matrix();
    float4x4 view_proj = mul(proj, view);

    float4 out_pos = vert.model_pos;
    out_pos.xyz *= vert.instance_scale;
    float3 normal = vert.normal;
    
    // If no rotation is provided, the quaternion is 0.
    if (vert.instance_rot.w != 0) {
        out_pos.xyz = rotate_vector(out_pos.xyz, vert.instance_rot);
        normal = rotate_vector(normal, vert.instance_rot);
    }
    out_pos.xyz += vert.instance_pos;
    
    float3 xyz = out_pos.xyz;

    out_pos = mul(view_proj, out_pos);

    float3 colors[3] = {
        float3 (1.0, 0.0, 0.0), float3 (0.0, 1.0, 0.0), float3 (0.0, 0.0, 1.0),
    };

    float3 color = colors[invocation_index % 3];

    //normal = normalize(mul(view_proj, float4(normal, 1)).xyz);
    //normal = normalize(mul(view, float4(normal, 1)).xyz);

#ifdef is_camera
    return {out_pos, normal, xyz, color, vert.id};
#else
    return {out_pos};
#endif
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
    output.normal = float4(input.normal, 1);
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
Sampler2D<uint> id_texture[4]; // Only index 2 may be used.

[vk::binding(1, 0)]
Sampler2D<float> depth_texture[5]; // Only index 3 may be used.

[vk::binding(2, 0)]
Sampler2D<float> light_maps[];

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
    const uint3 coord = uint3(x, y, 0);

    uint self = id_texture[3].Load(coord);

    uint adjacencies = 0;
    float depth = depth_texture[4].Load(coord);

    float3 frag_xyz = color_textures[2].Load(coord).xyz;
    float3 frag_normal = color_textures[1].Load(coord).xyz;

    // Shadow mapping:
    float4x4 light_transform = mul(g_bindless.get_proj_matrix(),
    g_bindless.get_light(0).transform);

    float4x4 m = {
        0.5f,    0, 0, 0.5f,
           0, 0.5f, 0, 0.5f,
           0,    0, 1,    0,
           0,    0, 0,    1
    };
    light_transform = mul(m, light_transform);

    float ambient_light = 0.15f;

    let light_source = g_bindless.get_light(0);
    // let light_position = mul(g_bindless.get_proj_matrix(), float4(light_source.position, 1));
    // float4 proj_position = mul(g_bindless.get_viewproj(), float4(frag_xyz,1));

    float3 to_light = light_source.position - frag_xyz;
    float3 to_camera     = normalize(g_bindless.get_camera_position() - frag_xyz);
    float3 halfway = normalize(to_light + to_camera);
    
    float3 to_light_unitv = normalize(to_light);
    float3 reflected_light = normalize(-reflect(to_light_unitv, frag_normal));

    float4 light_space_vert = mul(light_transform, float4(frag_xyz, 1));
    float3 light_map_coord = light_space_vert.xyz / light_space_vert.w;
    // TODO: Move bias into rasterization state.
    float light = light_maps[0].Sample(light_map_coord.xy)
        < light_map_coord.z ? ambient_light : 1.f;
    light = min(light, clamp(dot(frag_normal, to_light_unitv), ambient_light, 1));

    if (light > ambient_light) {
        // If not in shadow, add specular highlights.
        float specular_intensity = max(dot(reflected_light, to_camera), 0);
        float specular_hardness = 8.f;
        float specular = pow(specular_intensity, specular_hardness);

        light += specular;
    }

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

    float3 color = color_textures[0].Load(coord).rgb;
    if (self != 0) { color *= light; }
    
    return float4(color, 1);
}
