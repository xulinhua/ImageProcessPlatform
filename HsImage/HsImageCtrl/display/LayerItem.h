/**
 * @file LayerItem.h
 * @brief LayerItem头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_LAYERITEM_H
#define HSIMAGE_LAYERITEM_H

#include <QGraphicsItem>

namespace HsCtrl {

/**
 * @class LayerItem
 * @brief 空内容图层，只作 Stack / Drawing 的父节点（z=10 / z=20）
 */
class LayerItem : public QGraphicsItem {
public:
    explicit LayerItem(QGraphicsItem* parent = 0)
        : QGraphicsItem(parent)
    {
        setFlag(QGraphicsItem::ItemHasNoContents, true);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    QRectF boundingRect() const override
    {
        return QRectF();
    }

    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override
    {
    }
};

}

#endif
