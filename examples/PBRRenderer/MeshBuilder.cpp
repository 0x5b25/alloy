#include "MeshBuilder.hpp"

#include <cmath>
#include <numbers>
#include <unordered_map>


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
            {vertPos[v0], uv[0], normal, tangent, bitangent},
            {vertPos[v2], uv[2], normal, tangent, bitangent},
            {vertPos[v1], uv[1], normal, tangent, bitangent},
            {vertPos[v1], uv[1], normal, tangent, bitangent},
            {vertPos[v2], uv[2], normal, tangent, bitangent},
            {vertPos[v3], uv[3], normal, tangent, bitangent}
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

Mesh BuildQuadMesh(float length, float width) {
    float half_x = width / 2;
    float half_z = length / 2;

    glm::vec3 v0{-half_x, 0, -half_z};
    glm::vec3 v1{-half_x, 0,  half_z};
    glm::vec3 v2{ half_x, 0, -half_z};
    glm::vec3 v3{ half_x, 0,  half_z};

    glm::vec3 tangent = glm::normalize(v2 - v0);
    glm::vec3 bitangent = glm::normalize(v1 - v0);
    glm::vec3 normal = glm::normalize(glm::cross(bitangent, tangent));

    std::vector<Vertex> vertices = {
        {v0, {0, 0}, normal, tangent, bitangent},
        {v2, {1, 0}, normal, tangent, bitangent},
        {v1, {0, 1}, normal, tangent, bitangent},
        {v1, {0, 1}, normal, tangent, bitangent},
        {v2, {1, 0}, normal, tangent, bitangent},
        {v3, {1, 1}, normal, tangent, bitangent}
    };

    return Mesh{.vertices = std::move(vertices)};
}

Mesh BuildIcosphereMesh(float radius, uint32_t subdivision) {
    const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    std::vector<glm::vec3> positions = {
        glm::normalize(glm::vec3{-1,  t,  0}),
        glm::normalize(glm::vec3{ 1,  t,  0}),
        glm::normalize(glm::vec3{-1, -t,  0}),
        glm::normalize(glm::vec3{ 1, -t,  0}),
        glm::normalize(glm::vec3{ 0, -1,  t}),
        glm::normalize(glm::vec3{ 0,  1,  t}),
        glm::normalize(glm::vec3{ 0, -1, -t}),
        glm::normalize(glm::vec3{ 0,  1, -t}),
        glm::normalize(glm::vec3{ t,  0, -1}),
        glm::normalize(glm::vec3{ t,  0,  1}),
        glm::normalize(glm::vec3{-t,  0, -1}),
        glm::normalize(glm::vec3{-t,  0,  1}),
    };

    struct Triangle { uint32_t v0, v1, v2; };
    std::vector<Triangle> triangles = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1},
    };

    // key is the ordered pair of vertex indices packed into a uint64_t, 
    // value is the index of the midpoint vertex in positions
    // Key: (min(p1,p2) << 32) | max(p1,p2), Value: index into positions
    std::unordered_map<uint64_t, uint32_t> midpointCache;

    auto getMiddlePoint = [&](uint32_t p1, uint32_t p2) -> uint32_t {
        uint64_t key = ((uint64_t)std::min(p1, p2) << 32) | std::max(p1, p2);

        auto it = midpointCache.find(key);
        if (it != midpointCache.end()) return it->second;

        glm::vec3 mid = glm::normalize((positions[p1] + positions[p2]) * 0.5f);
        uint32_t idx = (uint32_t)positions.size();
        positions.push_back(mid);
        midpointCache[key] = idx;
        return idx;
    };

    for (uint32_t i = 0; i < subdivision; i++) {
        std::vector<Triangle> newTriangles;
        newTriangles.reserve(triangles.size() * 4);
        midpointCache.clear();

        for (auto& tri : triangles) {
            uint32_t a = getMiddlePoint(tri.v0, tri.v1);
            uint32_t b = getMiddlePoint(tri.v1, tri.v2);
            uint32_t c = getMiddlePoint(tri.v0, tri.v2);

            newTriangles.push_back({tri.v0, a, c});
            newTriangles.push_back({a, tri.v1, b});
            newTriangles.push_back({c, b, tri.v2});
            newTriangles.push_back({a, b, c});
        }

        triangles = std::move(newTriangles);
    }

    constexpr float pi = std::numbers::pi_v<float>;
    std::vector<Vertex> vertices;
    vertices.reserve(triangles.size() * 3);

    for (auto& tri : triangles) {
        for (uint32_t idx : {tri.v0, tri.v1, tri.v2}) {
            glm::vec3 n = positions[idx];
            glm::vec3 pos = n * radius;

            float u = 0.5f + std::atan2(n.z, n.x) / (2.0f * pi);
            float v = 0.5f - std::asin(glm::clamp(n.y, -1.0f, 1.0f)) / pi;

            glm::vec3 up = (std::abs(n.y) < 0.999f)
                ? glm::vec3(0, 1, 0)
                : glm::vec3(1, 0, 0);
            glm::vec3 tangent = glm::normalize(glm::cross(n, up));
            glm::vec3 bitangent = glm::normalize(glm::cross(tangent, n));

            vertices.push_back({pos, {u, v}, n, tangent, bitangent});
        }
    }

    return Mesh{.vertices = std::move(vertices)};
}