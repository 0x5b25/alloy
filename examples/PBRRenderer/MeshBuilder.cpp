#include "MeshBuilder.hpp"


Mesh BuildBoxMesh(float length, float width, float height) {
    float half_x = width / 2;
    float half_y = height / 2;
    float half_z = length / 2;

    static const glm::vec3 vertPos[] = {
        glm::vec3{-half_x,  half_y, -half_z},
        glm::vec3{ half_x,  half_y, -half_z},
        glm::vec3{-half_x, -half_y, -half_z},
        glm::vec3{ half_x, -half_y, -half_z},
        glm::vec3{-half_x,  half_y,  half_z},
        glm::vec3{ half_x,  half_y,  half_z},
        glm::vec3{-half_x, -half_y,  half_z},
        glm::vec3{ half_x, -half_y,  half_z},
    };

    static const glm::vec2 uv[] = {
        glm::vec2{0, 0},
        glm::vec2{1, 0},
        glm::vec2{0, 1},
        glm::vec2{1, 1}
    };

    std::vector<Vertex> vertices;

    auto _AddFace = [&](uint32_t v0,uint32_t v1,uint32_t v2,uint32_t v3) {
        const glm::vec3 tangent = glm::normalize(vertPos[v2] - vertPos[v0]);
        const glm::vec3 bitangent = glm::normalize(vertPos[v1] - vertPos[v0]);
        const glm::vec3 normal = glm::normalize(
            glm::cross(bitangent, tangent)
        );
        constexpr auto vertCnt = 6;
        const Vertex verts[vertCnt] = {
            {vertPos[v0], uv[v0], normal, tangent, bitangent},
            {vertPos[v2], uv[v2], normal, tangent, bitangent},
            {vertPos[v1], uv[v1], normal, tangent, bitangent},
            {vertPos[v1], uv[v1], normal, tangent, bitangent},
            {vertPos[v2], uv[v2], normal, tangent, bitangent},
            {vertPos[v3], uv[v3], normal, tangent, bitangent}
        };

        vertices.insert(vertices.end(), verts, verts+vertCnt);
    };

    _AddFace(0, 1, 2, 3);//Front
    _AddFace(1, 5, 3, 7);//right
    _AddFace(0, 4, 1, 5);//up
    _AddFace(2, 6, 0, 4);//left
    _AddFace(3, 7, 2, 6);//bottom
    _AddFace(5, 4, 7, 6);//back

    return Mesh {.vertices = std::move(vertices)};
}