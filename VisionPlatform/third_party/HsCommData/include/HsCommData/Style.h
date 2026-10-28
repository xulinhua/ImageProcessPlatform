/**
 * @file Style.h
 * @brief 图形样弝和文本样弝
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_STYLE_H
#define HSCOMMDATA_STYLE_H

#include <HsCommData/Color.h>

namespace HsCommData {

/** @brief 坪杝边，或填充并坯带杝边 */
enum DrawMode {
    DrawMargin = 0, ///< 坪杝边
    DrawFill   = 1  ///< 填充，坯坌时杝边
};

/** @brief 整条图元的线型。骨架逝边虚线仝用 Graphic::edgeDashed */
enum LineStyle {
    LineSolid = 0, ///< 实线
    LineDash,      ///< 虚线
    LineDot,       ///< 点线
    LineDashDot    ///< 点划线
};

/**
 * @class GeomStyle
 * @brief 画 HsObject〝坝字〝箭头〝点。几何本身丝带颜色
 */
struct HSCOMMDATA_EXPORT GeomStyle {
    Color4   stroke;   ///< 杝边
    Color4   fill;     ///< 填充
    double   widthPx;  ///< 线宽
    LineStyle line;    ///< 线型
    DrawMode draw;     ///< 杝边或填充
    double   opacity;  ///< 丝逝明度，0 到 1
    bool     cosmetic; ///< 为真时线宽是屝幕僝素

    GeomStyle(); ///< 绿杝边〝淡绿填充〝实线〝坪杝边

    /** @brief 结果坠加的缺眝 */
    static GeomStyle stackStyle();
    /** @brief 坯拖 ROI 的缺眝 */
    static GeomStyle drawingStyle();
    /** @brief 选中高亮 */
    static GeomStyle highlightStyle();
    /** @brief 标注 */
    static GeomStyle annotationStyle();
};

/**
 * @class TextStyle
 * @brief 画文字。字色〝底色和字坷在这一份里，正文和锚点仝在 HsText
 */
struct HSCOMMDATA_EXPORT TextStyle {
    Color4 color;  ///< 字色
    Color4 back;   ///< 底色。未设置则丝画底
    double fontPx; ///< 屝幕字坷

    TextStyle(); ///< 绿字〝无底〝16 僝素

    /** @brief 结果文字的缺眝 */
    static TextStyle stackStyle();
};

/** @brief ????????? Style */
using Style = GeomStyle;

}

#endif
