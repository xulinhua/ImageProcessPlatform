#ifndef HSIMAGE_HSIMAGEWINDOWTYPES_H
#define HSIMAGE_HSIMAGEWINDOWTYPES_H

/**
 * @file HsImageWindowTypes.h
 * @brief 画布窗口侧公共枚举与换底图策略（不依赖 QGraphicsView / HsCommData::Graphic）。
 */

#include <HsCtrl/HsImgCtrlExport.h>

namespace HsCtrl {

enum WindowMode {
    WindowDesign = 0, ///< 设计态，可画 ROI
    WindowRuntime     ///< 运行态，只浏览
};

/** @brief 图像窗口图层。从下到上，判断归层和显隐时用这个 */
enum WindowLayer {
    LayerImage = 0,   ///< 底图
    LayerStack,       ///< 结果叠层
    LayerDrawing,     ///< 设计态 ROI
    LayerMeasure,     ///< 临时测量
    LayerForeground   ///< 前景：读数、drawInfo 与 HUD
};
inline int windowLayerZ(WindowLayer layer)
{
    switch (layer) {
    case LayerImage: return 0;
    case LayerStack: return 10;
    case LayerDrawing: return 20;
    case LayerMeasure: return 30;
    case LayerForeground: return 40;
    }
    return 0;
}
enum DrawTool {
    ToolBrowse = 0, ///< 浏览，不落笔
    ToolCircle,     ///< 圆
    ToolRect1,      ///< 轴对齐矩形
    ToolRect2,      ///< 旋转矩形
    ToolEllipse,    ///< 椭圆
    ToolLine,       ///< 直线
    ToolCross,      ///< 十字
    ToolContour,    ///< XLD 折线
    ToolPolygon,    ///< 多边形 → HsCommData::HsRegion
    ToolText,       ///< 文本（单击放置）
    ToolBrush,       ///< 仅绘制会话。圆画笔涂成 HsCommData::HsRegion
    ToolMeasureDist,    ///< 两点距离
    ToolMeasureAngle,   ///< 三点夹角
    ToolMeasureRadius,  ///< 圆半径和直径
    ToolMeasureRect,    ///< 矩形宽高
    ToolMeasureEllipse,  ///< 椭圆半轴
    ToolMax ///< 个数，不是合法值
};

enum DrawInfoSpace {
    DrawInfoImage = 0, ///< 图像像素锚点，随缩放平移；字号保持屏幕像素
    DrawInfoWindow     ///< 控件像素锚点，位置与字号均固定
};

/**
 * @brief 视口背景（无图 / letterbox）。不读外部主题，不受全局 QSS 颜色影响。
 *        棋盘格按屏幕像素绘制，不随缩放变格。
 */
enum ViewportBackground {
    ViewportDark = 0,         ///< 工业深色
    ViewportMid,              ///< 中性灰
    ViewportLight,            ///< 浅灰工作区
    ViewportCheckerboard,     ///< 浅色棋盘格（透明显示习惯）
    ViewportCheckerboardDark, ///< 深色棋盘格
    ViewportCustom            ///< 纯色，见 setBackgroundColor
};

/**
 * @brief 浮动工具条皮肤。不读外部主题。
 */
enum OverlayToolbarSkin {
    ToolbarSkinDark = 0,      ///< 深色半透明
    ToolbarSkinLight,         ///< 浅色半透明
    ToolbarSkinTransparent,   ///< 全透明
    ToolbarSkinDarkSolid,     ///< 深色不透明
    ToolbarSkinLightSolid     ///< 浅色不透明
};

/**
 * @class ImageRefreshPolicy
 * @brief 换底图时各层是否一并清掉。默认全保留，与原先 setImage 行为一致。
 */
struct HSCTRL_EXPORT ImageRefreshPolicy {
    bool refreshStack;   ///< true：setImage 时清空 Stack
    bool refreshDrawing; ///< true：清空几何 ROI（不含 HsCommData::KindText）
    bool refreshText;    ///< true：清空 drawInfo 与 Drawing 层文字

    ImageRefreshPolicy()
        : refreshStack(false)
        , refreshDrawing(false)
        , refreshText(false)
    {
    }
};

}

#endif
