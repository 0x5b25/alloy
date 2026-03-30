#pragma once

#include "MeshRenderer.hpp"


/* Follow alloy default lefthand side coordinate system
 * 
 *    +Y          length +Z
 *  height         /
 *    |  v4 ----- v5
 *      / |     / |
 *   v0 ----- v1  |
 *    |   |   |   |
 *    |  v6 --|- v7
 *    | /     | /
 *   v2 ----- v3  ---> width +X
 */
Mesh BuildBoxMesh(float length, float width, float height);
/* Quad on XZ plane at y=0, normal pointing +Y
 *
 *        length +Z
 *         /
 *     v0 ----- v2
 *    /         /
 *   v1 ----- v3  ---> width +X
 */
Mesh BuildQuadMesh(float length, float width);

Mesh BuildTetrahedronMesh(float length, float width, float height);
Mesh BuildIcosphereMesh(float radius, uint32_t subdivision);
