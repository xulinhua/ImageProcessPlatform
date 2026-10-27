/**
 * @file HsMesh.cpp
 * @brief HsMesh实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsMesh.h>

#include <climits>

namespace HsCommData {

// 空网格
Mesh3::Mesh3()
{
}

// 同类型拷贝，顶点和三角整份复制
Mesh3::Mesh3(const Mesh3& other)
    : vertices(other.vertices)
    , triangles(other.triangles)
    , normals(other.normals)
{
}

// 同类型赋值，顶点和三角整份复制
Mesh3& Mesh3::operator=(const Mesh3& other)
{
    if (this != &other) {
        vertices = other.vertices;
        triangles = other.triangles;
        normals = other.normals;
    }
    return *this;
}

// 没有顶点或没有三角形则为空
bool Mesh3::isEmpty() const
{
    return vertices.empty() || triangles.empty();
}

// 三角形个数
int Mesh3::triangleCount() const
{
    return static_cast<int>(triangles.size() / 3);
}

// 下标和法向是否自洽
bool Mesh3::attribsOk() const
{
    if (triangles.size() % 3 != 0) {
        return false;
    }
    if (!normals.empty() && normals.size() != vertices.size()) {
        return false;
    }
    if (vertices.size() > static_cast<size_t>(INT_MAX)) {
        return false;
    }
    const int n = static_cast<int>(vertices.size());
    for (size_t i = 0; i < triangles.size(); ++i) {
        const int idx = triangles[i];
        if (idx < 0 || idx >= n) {
            return false;
        }
    }
    return true;
}

// 顶点的轴对齐包围盒
Aabb3 Mesh3::bounds() const
{
    if (vertices.empty()) {
        return Aabb3();
    }
    Aabb3 box = Aabb3::fromPoint(vertices[0]);
    for (size_t i = 1; i < vertices.size(); ++i) {
        box.include(vertices[i]);
    }
    return box;
}

}
