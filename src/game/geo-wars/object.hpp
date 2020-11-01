#ifndef PGW_GAME_GEO_WARS_OBJECT_HPP
#define PGW_GAME_GEO_WARS_OBJECT_HPP

#include <vector>

#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/vec2.hpp>

#include "visual/window.hpp"

namespace pgw {

inline const std::vector< glm::vec2 > shape_jet {
    { 1.0f, 0.0f },
    { 0.3090169943749475f, -0.9510565162951535f },
    { -0.8090169943749473f, -0.5877852522924732f },
    { -0.8090169943749476f, 0.5877852522924729f },
    { 0.3090169943749472f, 0.9510565162951536f }
};

inline const std::vector< glm::vec2 > shape_square {
    { 1.0f, 1.0f },
    { -1.0f, 1.0f },
    { -1.0f, -1.0f },
    { 1.0f, -1.0f }
};

struct ShapeTransform {
    float rotation = 0;
    float scale[2] = { 1.0f, 1.0f };
    float δ[2]     = { 0.0f, 0.0f };
};

//-------------------------------------
// Functions to build object
//-------------------------------------
inline auto transform_matrix(
    const ShapeTransform& transform
) {
    return
        glm::translate(
            glm::scale(
                glm::rotate(glm::mat3(), transform.rotation),
                { transform.scale[0], transform.scale[1] }
            ),
            { transform.δ[0], transform.δ[1] }
        );
}

inline void build_shape_append(
    std::vector< Vertex >&         vertex_list,
    const std::vector< glm::vec2 > shape_original,
    const ShapeTransform&          transform,
    const glm::vec3&               color
) {
    const int num_vertices = shape_original.size();
    const int original_num_vertices = vertex_list.size();
    vertex_list.resize(original_num_vertices + num_vertices);

    for(int i = 0; i < num_vertices; ++i) {
        auto& coord_original = shape_original[i];
        auto coord3 = transform_matrix(transform) * glm::vec3 { coord_original[0], coord_original[1], 1.0f };

        vertex_list[original_num_vertices + i].pos.x = coord3.x;
        vertex_list[original_num_vertices + i].pos.y = coord3.y;

        vertex_list[original_num_vertices + i].color = color;
    }
}

} // namespace pgw

#endif
