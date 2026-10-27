/**
 * @file QtGeom.h
 * @brief QtGeom头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_QTGEOM_H
#define HSIMAGE_QTGEOM_H

#include <HsCommData/HsCommData.h>

#include <QByteArray>
#include <QColor>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>

#include <string>

namespace HsCtrl {

/** @brief HsCommData::Point2d → QPointF（显示层） */
inline QPointF toQ(const HsCommData::Point2d& p)
{
    return QPointF(p.x, p.y);
}

/** @brief QPointF → HsCommData::Point2d */
inline HsCommData::Point2d fromQ(const QPointF& p)
{
    return HsCommData::Point2d(p.x(), p.y());
}

/** @brief HsCommData::Box2 → QRectF */
inline QRectF toQ(const HsCommData::Box2& b)
{
    return QRectF(b.x, b.y, b.w, b.h);
}

/** @brief QRectF → HsCommData::Box2 */
inline HsCommData::Box2 fromQ(const QRectF& r)
{
    return HsCommData::Box2(r.x(), r.y(), r.width(), r.height());
}

/** @brief HsCommData::IRect → QRect */
inline QRect toQ(const HsCommData::IRect& r)
{
    return QRect(r.x, r.y, r.width, r.height);
}

/** @brief QRect → HsCommData::IRect */
inline HsCommData::IRect fromQ(const QRect& r)
{
    return HsCommData::IRect(r.x(), r.y(), r.width(), r.height());
}

/** @brief 线型 → Qt 画笔。实线、虚线、点线、点划线 */
inline Qt::PenStyle toQ(HsCommData::LineStyle line)
{
    switch (line) {
    case HsCommData::LineDash:
        return Qt::DashLine;
    case HsCommData::LineDot:
        return Qt::DotLine;
    case HsCommData::LineDashDot:
        return Qt::DashDotLine;
    case HsCommData::LineSolid:
    default:
        return Qt::SolidLine;
    }
}

/** @brief HsCommData::Color4 → QColor */
inline QColor toQ(const HsCommData::Color4& c)
{
    return QColor(c.r, c.g, c.b, c.a);
}

/** @brief QColor → HsCommData::Color4 */
inline HsCommData::Color4 fromQ(const QColor& c)
{
    return HsCommData::Color4(static_cast<unsigned char>(c.red()),
                  static_cast<unsigned char>(c.green()),
                  static_cast<unsigned char>(c.blue()),
                  static_cast<unsigned char>(c.alpha()));
}

/** @brief std::string → QString（本地代码页） */
inline QString toQ(const std::string& s)
{
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

/** @brief QString → 本地代码页 std::string */
inline std::string fromQ(const QString& s)
{
    const QByteArray u = s.toLocal8Bit();
    return std::string(u.constData(), static_cast<size_t>(u.size()));
}

}

#endif
