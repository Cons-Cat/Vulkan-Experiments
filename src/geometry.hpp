#include "bindless.hpp"
#include "globals.hpp"

inline mesh g_cube_mesh = {
    {// Front face.
     {0.5, 0.5, 0.5},
     {-0.5, 0.5, 0.5},
     {-0.5, -0.5, 0.5},
     {0.5, -0.5, 0.5},
     // Back face.
     {0.5, 0.5, -0.5},
     {-0.5, 0.5, -0.5},
     {-0.5, -0.5, -0.5},
     {0.5, -0.5, -0.5},
     // Left face.
     {-0.5, 0.5, 0.5},
     {-0.5, 0.5, -0.5},
     {-0.5, -0.5, -0.5},
     {-0.5, -0.5, 0.5},
     // Right face.
     {0.5, 0.5, 0.5},
     {0.5, -0.5, 0.5},
     {0.5, -0.5, -0.5},
     {0.5, 0.5, -0.5},
     // Bottom face.
     {0.5, 0.5, 0.5},
     {-0.5, 0.5, 0.5},
     {-0.5, 0.5, -0.5},
     {0.5, 0.5, -0.5},
     // Top face.
     {0.5, -0.5, 0.5},
     {-0.5, -0.5, 0.5},
     {-0.5, -0.5, -0.5},
     {0.5, -0.5, -0.5}},

    {0,  1,  2,  2,  3,  0,  4,  7,  6,  6,  5,  4,  8,  9,  10, 10, 11, 8,
     12, 13, 14, 14, 15, 12, 16, 19, 18, 18, 17, 16, 20, 21, 22, 22, 23, 20}
};

inline mesh g_plane_mesh = {
    {{0.5, 0, 0.5}, {-0.5, 0, 0.5}, {-0.5, 0, -0.5}, {0.5, 0, -0.5}},

    {2, 1, 0, 0, 3, 2}
};

inline auto make_checkerboard_plane(
    glm::vec3 center, float scale_x, float scale_z, unsigned rows,
    unsigned columns, mesh_instance const& properties_even,
    mesh_instance const& properties_odd) -> std::vector<mesh_instance> {
    std::vector<mesh_instance> instances;
    instances.reserve(rows * columns);

    ++g_next_instance_id;
    unsigned id = g_next_instance_id;

    auto fill_in_grid = [&](unsigned offset, mesh_instance const& property) {
        for (unsigned j = offset; j < columns; j += 2) {
            for (unsigned i = 0; i < rows; ++i) {
                bool is_odd_row = i & 1u;
                bool is_odd_instance = offset == 1;
                glm::vec3 position;

                position =
                    center - glm::vec3{(static_cast<float>(j) -
                                        (static_cast<float>(columns) / 2.f) +
                                        (is_odd_row)) *
                                           scale_x,
                                       0,
                                       (static_cast<float>(i) -
                                        (static_cast<float>(rows) / 2.f)) *
                                           scale_z};

                mesh_instance instance = property;
                instance.position = position;
                instance.id = id;
                instances.push_back(instance);
            }
        }
    };
    // Even grid cells.
    fill_in_grid(0, properties_even);
    // Odd grid cells.
    fill_in_grid(1, properties_odd);

    return instances;
}
