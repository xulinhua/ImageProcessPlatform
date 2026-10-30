/**
 * @file Style.h
 * @brief 图形样式和文本样式
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_STYLE_H
#define HSCOMMDATA_STYLE_H

#include <HsCommData/Color.h>

namespace HsCommData {

/** @brief 只描边，或填充并可带描边 */
enum DrawMode {
    DrawMargin = 0, ///< 只描边
    DrawFill   = 1  ///< 填充，可同时描边
};

/** @brief 整条图元的线型。骨架逐边虚线仍用 Graphic::edgeDashed */
enum LineStyle {
    LineSolid = 0, ///< 实线
    LineDash,      ///< 虚线
    LineDot,       ///< 点线
    LineDashDot    ///< 点划线
};

/**
 * @class GeomStyle
 * @brief 画 HsObject、十字、箭头、点。几何本身不带颜色
 */
struct HSCOMMDATA_EXPORT GeomStyle {
    Color4   stroke;   ///< 描边
    Color4   fill;     ///< 填充
    double   widthPx;  ///< 线宽
    LineStyle line;    ///< 线型
    DrawMode draw;     ///< 描边或填充
    double   opacity;  ///< 不透明度，0 到 1
    bool     cosmetic; ///< 为真时线宽是屏幕像素

    GeomStyle(); ///< 绿描边、淡绿填充、实线、只描边

    /** @brief 结果叠加的缺省 */
    static GeomStyle stackStyle();
    /** @brief 可拖 ROI 的缺省 */
    static GeomStyle drawingStyle();
    /** @brief 选中高亮 */
    static GeomStyle highlightStyle();
    /** @brief 标注 */
    static GeomStyle annotationStyle();
};

/**
 * @class TextStyle
 * @brief 画文字。字色、底色和字号在这一份里，正文和锚点仍在 HsText
 */
struct HSCOMMDATA_EXPORT TextStyle {
    Color4 color;  ///< 字色
    Color4 back;   ///< 底色。未设置则不画底
    double fontPx; ///< 屏幕字号

    TextStyle(); ///< 绿字、无底、16 像素

    /** @brief 结果文字的缺省 */
    static TextStyle stackStyle();
};

/** @brief 兼容旧名，Style 即 GeomStyle */
using Style = GeomStyle;

}

#endif
