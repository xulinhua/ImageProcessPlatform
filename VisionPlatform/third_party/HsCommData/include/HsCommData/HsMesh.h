/**
 * @file HsMesh.h
 * @brief HsMesh头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSMESH_H
#define HSCOMMDATA_HSMESH_H

#include <vector>

#include <HsCommData/Geom3.h>

namespace HsCommData {

/**
 * @class Mesh3
 * @brief 三角网格。顶点加下标三元组，不存任意多边形。
 * 法向可选，非空时按顶点存放且必须与顶点数相同。整份拷贝，不共享缓冲。
 */
struct HSCOMMDATA_EXPORT Mesh3 {
    std::vector<Point3d> vertices; ///< 顶点，毫米
    std::vector<int> triangles;   ///< 三角形下标，三个一组，0 起。个数必须是 3 的倍数
    std::vector<Point3d> normals;  ///< 顶点法向。空表示没有；非空必须与顶点数相同

    /** @brief 空网格 */
    Mesh3();
    /** @brief 同类型拷贝，顶点和三角整份复制 */
    Mesh3(const Mesh3& other);
    /** @brief 同类型赋值，顶点和三角整份复制 */
    Mesh3& operator=(const Mesh3& other);

    /** @brief 没有顶点或没有三角形则为空 */
    bool isEmpty() const;
    /** @brief 三角形个数。下标长度不是 3 的倍数时只计完整的组 */
    int triangleCount() const;
    /** @brief 下标成组、都落在顶点范围内，法向要么为空要么与顶点数相同 */
    bool attribsOk() const;
    /** @brief 顶点的轴对齐包围盒。没有顶点时返回空 Aabb3 */
    Aabb3 bounds() const;
};

}

#endif
