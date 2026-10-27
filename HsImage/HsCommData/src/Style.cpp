/**
 * @file Style.cpp
 * @brief 图形样式和文本样式
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/Style.h>

namespace HsCommData {

// 默认绿描边、淡绿填充、实线、只描边
GeomStyle::GeomStyle()
    : stroke(Color4::of(ColorGreen))
    , fill(Color4::of(ColorGreen, 40))
    , widthPx(2.0)
    , line(LineSolid)
    , draw(DrawMargin)
    , opacity(1.0)
    , cosmetic(true)
{
}

// 结果叠加用绿色
GeomStyle GeomStyle::stackStyle()
{
    GeomStyle s;
    s.stroke = Color4(0x52, 0xC4, 0x1A, 255);
    s.fill = Color4(0x52, 0xC4, 0x1A, 40);
    s.widthPx = 2.0;
    s.draw = DrawMargin;
    s.opacity = 1.0;
    s.cosmetic = true;
    return s;
}

// 可拖 ROI 用橙色
GeomStyle GeomStyle::drawingStyle()
{
    GeomStyle s;
    s.stroke = Color4(0xFA, 0xAD, 0x14, 255);
    s.fill = Color4(0xFA, 0xAD, 0x14, 40);
    s.widthPx = 2.0;
    s.draw = DrawMargin;
    s.opacity = 1.0;
    s.cosmetic = true;
    return s;
}

// 选中高亮用蓝色
GeomStyle GeomStyle::highlightStyle()
{
    GeomStyle s;
    s.stroke = Color4(0x18, 0x90, 0xFF, 255);
    s.fill = Color4(0x18, 0x90, 0xFF, 50);
    s.widthPx = 2.5;
    s.draw = DrawMargin;
    s.opacity = 1.0;
    s.cosmetic = true;
    return s;
}

// 标注用绿色
GeomStyle GeomStyle::annotationStyle()
{
    GeomStyle s;
    s.stroke = Color4::of(ColorGreen);
    s.fill = Color4::of(ColorGreen, 40);
    s.widthPx = 1.0;
    s.draw = DrawMargin;
    s.opacity = 1.0;
    s.cosmetic = false;
    return s;
}

// 绿字、无底、16 像素
TextStyle::TextStyle()
    : color(0x52, 0xC4, 0x1A, 255)
    , fontPx(16.0)
{
}

// 结果文字的缺省
TextStyle TextStyle::stackStyle()
{
    return TextStyle();
}

}
