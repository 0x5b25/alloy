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

Mesh BuildTetrahedronMesh(float length, float width, float height);
