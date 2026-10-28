/**
 * @file Color.h
 * @brief 颜色。RGBA，几何本身不带颜色
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_COLOR_H
#define HSCOMMDATA_COLOR_H

#include <HsCommData/HsCommDataExport.h>

namespace HsCommData {

/** @brief 常用纯色。与 Halcon set_color 的建议色一致。通过与不通过的颜色在显示配置里 */
enum ColorId {
    ColorBlack = 0,       ///< 00 00 00
    ColorWhite,           ///< FF FF FF
    ColorRed,             ///< FF 00 00
    ColorGreen,           ///< 00 FF 00
    ColorBlue,            ///< 00 00 FF
    ColorCyan,            ///< 00 FF FF
    ColorMagenta,         ///< FF 00 FF
    ColorYellow,          ///< FF FF 00
    ColorDimGray,         ///< 69 69 69
    ColorGray,            ///< BE BE BE
    ColorLightGray,       ///< D3 D3 D3
    ColorOrange,          ///< FF A5 00
    ColorOrangeRed,       ///< FF 45 00
    ColorCoral,           ///< FF 7F 50
    ColorPink,            ///< FF C0 CB
    ColorSpringGreen,     ///< 00 FF 7F
    ColorCadetBlue,       ///< 5F 9E A0
    ColorSlateBlue,       ///< 6A 5A CD
    ColorMediumSlateBlue, ///< 7B 68 EE
    ColorDarkOliveGreen   ///< 55 6B 2F
};

/**
 * @class Color4
 * @brief RGBA，每个通道 0 到 255。几何本身不带颜色
 */
struct HSCOMMDATA_EXPORT Color4 {
    unsigned char r; ///< 红
    unsigned char g; ///< 绿
    unsigned char b; ///< 蓝
    unsigned char a; ///< 透明度，255 为不透明

    Color4() : r(0), g(0), b(0), a(0) {} ///< 全 0 表示未设置
    Color4(unsigned char r_, unsigned char g_, unsigned char b_, unsigned char a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    /** @brief 全 0 时为真，表示未设置 */
    bool isUnset() const { return r == 0 && g == 0 && b == 0 && a == 0; }

    /** @brief 按常用色取值。a 是透明度 */
    static Color4 of(ColorId id, unsigned char a = 255)
    {
        switch (id) {
        case ColorBlack:           return Color4(0x00, 0x00, 0x00, a);
        case ColorWhite:           return Color4(0xFF, 0xFF, 0xFF, a);
        case ColorRed:             return Color4(0xFF, 0x00, 0x00, a);
        case ColorGreen:           return Color4(0x00, 0xFF, 0x00, a);
        case ColorBlue:            return Color4(0x00, 0x00, 0xFF, a);
        case ColorCyan:            return Color4(0x00, 0xFF, 0xFF, a);
        case ColorMagenta:         return Color4(0xFF, 0x00, 0xFF, a);
        case ColorYellow:          return Color4(0xFF, 0xFF, 0x00, a);
        case ColorDimGray:         return Color4(0x69, 0x69, 0x69, a);
        case ColorGray:            return Color4(0xBE, 0xBE, 0xBE, a);
        case ColorLightGray:       return Color4(0xD3, 0xD3, 0xD3, a);
        case ColorOrange:          return Color4(0xFF, 0xA5, 0x00, a);
        case ColorOrangeRed:       return Color4(0xFF, 0x45, 0x00, a);
        case ColorCoral:           return Color4(0xFF, 0x7F, 0x50, a);
        case ColorPink:            return Color4(0xFF, 0xC0, 0xCB, a);
        case ColorSpringGreen:     return Color4(0x00, 0xFF, 0x7F, a);
        case ColorCadetBlue:       return Color4(0x5F, 0x9E, 0xA0, a);
        case ColorSlateBlue:       return Color4(0x6A, 0x5A, 0xCD, a);
        case ColorMediumSlateBlue: return Color4(0x7B, 0x68, 0xEE, a);
        case ColorDarkOliveGreen:  return Color4(0x55, 0x6B, 0x2F, a);
        }
        return Color4();
    }
};

}

#endif
