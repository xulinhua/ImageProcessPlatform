/**
 * @file Graphic.cpp
 * @brief Graphic实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Graphic.h>
#include <HsCommData/GeomOps.h>

#include <cmath>

namespace HsCommData {

// 旋转椭圆或矩形的轴对齐包围盒
static Box2 ellipseBounds(const Point2d& c, double ra, double rb, double phi)
{
    const double cosA = std::cos(phi);
    const double sinA = std::sin(phi);
    const double ux = ra * cosA;
    const double uy = ra * sinA;
    const double vx = -rb * sinA;
    const double vy = rb * cosA;
    const double hw = std::sqrt(ux * ux + vx * vx);
    const double hh = std::sqrt(uy * uy + vy * vy);
    return Box2(c.x - hw, c.y - hh, hw * 2.0, hh * 2.0);
}

// 图元默认圆、Stack、可见
Graphic::Graphic()
    : kind(KindObject)
    , role(RoleStack)
    , frameId(0)
    , visible(true)
    , markerSize(10.0)
{
    geom = CircleP();
}

// Drawing 只用第 0 个；Stack 按容器个数。点、十字、箭头、文字空容器为 0
int Graphic::geomSpan() const
{
    switch (kind) {
    case KindObject:
    case KindKeypoints:
    case KindSkeleton:
        if (role != RoleStack) {
            return 1;
        }
        return geom.isInitialized() ? geom.count() : 0;
    case KindCross:
        if (role != RoleStack) {
            return cross.empty() ? 0 : 1;
        }
        return static_cast<int>(cross.size());
    case KindArrow:
        if (role != RoleStack) {
            return arrow.empty() ? 0 : 1;
        }
        return static_cast<int>(arrow.size());
    case KindPoint:
        if (role != RoleStack) {
            return point.empty() ? 0 : 1;
        }
        return static_cast<int>(point.size());
    case KindText:
    case KindTextBadge:
        if (role != RoleStack) {
            return text.empty() ? 0 : 1;
        }
        return static_cast<int>(text.size());
    default:
        return 1;
    }
}

// 单个图标按自己的种类算盒子。图像和空元素没有盒子
static Box2 objectBox(const Graphic& g, int index)
{
    switch (g.geom.kindAt(index)) {
    case ObjCircle: {
        const CircleP* c = g.geom.circleAt(index);
        return Box2(c->c.x - c->r, c->c.y - c->r, c->r * 2.0, c->r * 2.0);
    }
    case ObjEllipse: {
        const EllipseP* e = g.geom.ellipseAt(index);
        return ellipseBounds(e->c, e->ra, e->rb, e->phi);
    }
    case ObjRect1:
        return normalized(g.geom.rect1At(index)->r);
    case ObjRect2: {
        const Rect2P* r = g.geom.rect2At(index);
        return ellipseBounds(r->c, r->l1, r->l2, r->phi);
    }
    case ObjLine: {
        const LineP* ln = g.geom.lineAt(index);
        return fromPoints(ln->a, ln->b);
    }
    case ObjContour:
        return g.geom.contourAt(index)->bounds();
    case ObjPoly:
        return g.geom.polyAt(index)->bounds();
    case ObjRegion: {
        const IRect& b = g.geom.regionAt(index)->bounds;
        return Box2(b.x, b.y, b.width, b.height);
    }
    default:
        return Box2();
    }
}

// 旋转后两臂端点的轴对齐盒子。半宽小于 0 时臂长不在图像坐标里
static Box2 crossBox(const HsCross& c)
{
    if (c.size < 0.0) {
        return Box2(c.center.x, c.center.y, 1.0, 1.0);
    }
    const double cph = std::cos(c.angle);
    const double sph = std::sin(c.angle);
    const double hx = c.size * std::fabs(cph);
    const double hy = c.size * std::fabs(sph);
    const double vx = c.size * std::fabs(sph);
    const double vy = c.size * std::fabs(cph);
    const double hw = hx > vx ? hx : vx;
    const double hh = hy > vy ? hy : vy;
    return Box2(c.center.x - hw, c.center.y - hh, hw * 2.0, hh * 2.0);
}

// 箭身加上两翼。水平或竖直时仍留出 1 像素，避免被当成空盒子丢掉
static Box2 arrowBox(const HsArrow& a)
{
    Point2d pts[4];
    pts[0] = a.tail;
    pts[1] = a.head;
    int n = 2;
    Point2d left;
    Point2d right;
    if (a.wings(&left, &right)) {
        pts[2] = left;
        pts[3] = right;
        n = 4;
    }
    double minX = pts[0].x;
    double maxX = pts[0].x;
    double minY = pts[0].y;
    double maxY = pts[0].y;
    for (int i = 1; i < n; ++i) {
        if (pts[i].x < minX) {
            minX = pts[i].x;
        }
        if (pts[i].x > maxX) {
            maxX = pts[i].x;
        }
        if (pts[i].y < minY) {
            minY = pts[i].y;
        }
        if (pts[i].y > maxY) {
            maxY = pts[i].y;
        }
    }
    double w = maxX - minX;
    double h = maxY - minY;
    if (w < 1.0) {
        w = 1.0;
    }
    if (h < 1.0) {
        h = 1.0;
    }
    return Box2(minX, minY, w, h);
}

// 点、十字、箭头、文字、掩膜不在 geom 里。index 是对应容器的下标
static Box2 fieldBox(const Graphic& g, int index)
{
    switch (g.kind) {
    case KindCross: {
        if (index < 0 || index >= static_cast<int>(g.cross.size())) {
            return Box2();
        }
        return crossBox(g.cross[static_cast<size_t>(index)]);
    }
    case KindArrow: {
        if (index < 0 || index >= static_cast<int>(g.arrow.size())) {
            return Box2();
        }
        return arrowBox(g.arrow[static_cast<size_t>(index)]);
    }
    case KindPoint: {
        if (index < 0 || index >= static_cast<int>(g.point.size())) {
            return Box2();
        }
        const Point2d& p = g.point[static_cast<size_t>(index)];
        return Box2(p.x - g.markerSize, p.y - g.markerSize, g.markerSize * 2.0, g.markerSize * 2.0);
    }
    case KindText:
    case KindTextBadge: {
        if (index < 0 || index >= static_cast<int>(g.text.size())) {
            return Box2();
        }
        const HsText& t = g.text[static_cast<size_t>(index)];
        return Box2(t.anchor.x, t.anchor.y, 1.0, 1.0);
    }
    case KindMask:
        return Box2(0, 0,
                    g.mask.isEmpty() ? 0.0 : static_cast<double>(g.mask.width),
                    g.mask.isEmpty() ? 0.0 : static_cast<double>(g.mask.height));
    default:
        return Box2();
    }
}

// 按 kind 重算图像坐标包围盒。Stack 混放时每个元素用自己的种类
void Graphic::updateBounds()
{
    if (role == RoleStack && geom.isInitialized() && geom.kind() == ObjMixed) {
        Box2 acc;
        bool any = false;
        const int n = geom.count();
        for (int i = 0; i < n; ++i) {
            const Box2 one = objectBox(*this, i);
            if (isEmpty(one)) {
                continue;
            }
            acc = any ? united(acc, one) : one;
            any = true;
        }
        // 再并上十字、箭头、点、文字等附加字段，字段个数按 kind 取
        int fieldN = 0;
        switch (kind) {
        case KindCross:
            fieldN = static_cast<int>(cross.size());
            break;
        case KindArrow:
            fieldN = static_cast<int>(arrow.size());
            break;
        case KindPoint:
            fieldN = static_cast<int>(point.size());
            break;
        case KindText:
        case KindTextBadge:
            fieldN = static_cast<int>(text.size());
            break;
        case KindMask:
            fieldN = 1;
            break;
        default:
            break;
        }
        for (int i = 0; i < fieldN; ++i) {
            const Box2 field = fieldBox(*this, i);
            if (isEmpty(field)) {
                continue;
            }
            acc = any ? united(acc, field) : field;
            any = true;
        }
        boundsImage = any ? acc : Box2();
        return;
    }
    // 单一种类：逐格合并。种类不认识或一格都没有时保留旧包围盒
    const int n = geomSpan();
    if (n <= 0) {
        boundsImage = Box2();
        return;
    }
    Box2 acc;
    bool any = false;
    for (int i = 0; i < n; ++i) {
        Box2 one;
        switch (kind) {
        case KindObject:
            one = objectBox(*this, i);
            break;
        case KindCross:
        case KindArrow:
        case KindPoint:
        case KindText:
        case KindTextBadge:
        case KindMask:
            one = fieldBox(*this, i);
            break;
        case KindKeypoints:
        case KindSkeleton:
            one = geom.contourAt(i)->bounds();
            break;
        default:
            return;
        }
        acc = any ? united(acc, one) : one;
        any = true;
    }
    if (any) {
        boundsImage = acc;
    }
}

}
