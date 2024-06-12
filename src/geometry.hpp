#include "bindless.hpp"

inline mesh g_cube_mesh = {
    .vertices =
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

    .indices = {0,  1,  2,  2,  3,  0,  4,  7,  6,  6,  5,  4,
                   8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                   16, 19, 18, 18, 17, 16, 20, 21, 22, 22, 23, 20}
};
