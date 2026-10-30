/**
 * @file HsImageWindow.h
 * @brief HsImageWindow头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_HSIMAGEWINDOW_H
#define HSIMAGE_HSIMAGEWINDOW_H

#include <HsCtrl/HsImgCtrlExport.h>
#include <HsCtrl/HsImageWindowTypes.h>
#include <HsCommData/HsCommData.h>

#include <QByteArray>
#include <QColor>
#include <QGraphicsView>
#include <QImage>
#include <QMetaType>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <memory>
#include <vector>

class QContextMenuEvent;

namespace cv {
class Mat;
}

namespace HsCtrl {

/** @brief 把 HsCommData::Graphic 注册进 Qt 元类型，供信号排队 */
void HSCTRL_EXPORT registerGraphicMetaTypes();

/**
 * @class HsImageWindow
 * @brief 图像显示与交互窗口。场景坐标与图像像素 1:1，缩放只改视口变换。
 *        对外入口是 image_widget::ImageWidgetApi。本类是内部画布。
 *        底图、结果叠加与用户 ROI 分层刷新，不在上一帧画面上再采样。
 *        对外协议是 HsCommData::Graphic / HsCommData::GeomStyle / HsCommData::FrameOverlay，不暴露 Qt 图元。
 */
class HSCTRL_EXPORT HsImageWindow : public QGraphicsView {
    Q_OBJECT
public:
    /************************ 构造 / 析构 ************************/
    /** @brief 创建空窗口 */
    explicit HsImageWindow(QWidget* parent = 0);
    /** @brief 释放 Pimpl */
    virtual ~HsImageWindow();

    /************************ 模式与工具 ************************/
    /** @brief 设计态可画 ROI，运行态只浏览 */
    void setWindowMode(WindowMode mode);
    /** @brief 当前窗口模式 */
    WindowMode windowMode() const;
    /** @brief 切换绘制工具。浏览不落笔；ToolBrush 只在绘制会话里使用 */
    void setDrawTool(DrawTool tool);
    /** @brief 当前绘制工具 */
    DrawTool drawTool() const;
    /** @brief 设置像素比例。未写入的单位按1:1 */
    void setImageCalib(const HsCommData::ImageCalib& calib);
    /** @brief 当前像素比例 */
    HsCommData::ImageCalib imageCalib() const;
    /**
     * @brief 开始一次独占绘制。浏览工具会失败；成功后直到提交或取消才结束。
     * @note 圆、轴对齐矩形、旋转矩形、椭圆、直线、十字、折线、多边形、画笔可开会话。
     *       文本等工具返回 false，不会发出 drawSessionFinished。
     *       语义接近 Halcon draw_*：拖出形状后右键或 Enter 结束。
     *       直接 setDrawTool 只换工具，不开会话。
     */
    bool beginDrawSession(DrawTool tool);
    /** @brief 是否正在绘制会话中 */
    bool drawSessionActive() const;
    /** @brief 提交当前会话，并发出绘制结束信号 */
    void commitDrawSession();
    /** @brief 取消当前会话，不保留草稿 */
    void cancelDrawSession();
    /** @brief 取消未提交的橡皮筋 */
    void cancelActiveDraw();
    /** @brief 画笔半径（图像像素），一位小数，夹到 0.5..256 */
    void setBrushRadius(double radiusPx);
    /** @brief 当前画笔半径（图像像素） */
    double brushRadius() const;
    /** @brief 文本工具要写下的字符串 */
    void setDrawText(const QString& text);
    /** @brief 文本工具当前字符串 */
    QString drawText() const;
    /** @brief 文本锚点用图像坐标还是窗口坐标 */
    void setTextToolSpace(DrawInfoSpace space);
    /** @brief 文本工具当前坐标系 */
    DrawInfoSpace textToolSpace() const;
    /** @brief 文本工具字号（屏幕像素） */
    void setTextFontPx(int px);
    /** @brief 文本工具当前字号 */
    int textFontPx() const;
    /**
     * @brief 十字半宽
     * @param size 图像像素。小于 0 表示穿窗，长宽随窗口
     */
    void setCrossSize(double size);
    /** @brief 十字半宽。小于 0 表示穿窗 */
    double crossSize() const;
    /** @brief 实时标记。Demo 用来区分是否跟帧刷新 */
    void setLive(bool live);
    /** @brief 是否处于实时标记 */
    bool isLive() const;

    /************************ 底图 ************************/
    /**
     * @brief 用原始像素换底图。内部转成显示缓冲后再贴到场景。
     * @param encoding rgb8 / bgr8 / rgba8 / bgra8 / mono8 / 8uc1
     * @note 指针为空或尺寸、行跨度不大于 0 时清空底图；编码不支持时忽略本次调用。
     *       是否连带清掉叠加由 ImageRefreshPolicy 决定，默认保留 Stack / Drawing / drawInfo。
     */
    void setImage(const uint8_t* data, int width, int height, int strideBytes,
                  const QString& encoding);
    /** @brief 直接换上已经备好的图。不在这里再拷像素、不做 QPixmap */
    void setPreparedImage(const QImage& image);
    /** @brief 同 setImage，保留旧调用名 */
    void setImageRaw(const uint8_t* data, int width, int height, int strideBytes,
                     const QString& encoding);
    /**
     * @brief 用 cv::Mat 换底图。只接受连续或可按行读的 8 位图。
     *        支持 CV_8UC1(mono8)、CV_8UC3(bgr8)、CV_8UC4(bgra8)，必要时 clone。
     */
    void setImage(const cv::Mat& mat);
    /**
     * @brief 解码 JPEG/PNG 等压缩字节换底图。
     * @param formatHint 如 "jpeg"/"png"，空则交给 Qt 猜测
     */
    void setCompressedImage(const QByteArray& data, const QString& formatHint = QString());
    /** @brief 清掉底图，保留 Stack / Drawing */
    void clearImage();

    /**
     * @brief Ctrl 像素小窗用的 16 位深度探针（毫米）。
     *        普通 setImage 不自动清除，由壳层显式管理。
     */
    void setDepthProbeU16(const uint16_t* data, int width, int height, int strideBytes = 0);
    /** @brief Ctrl 像素小窗用的 32 位浮点深度探针（米，弹出时显示毫米） */
    void setDepthProbeF32(const float* data, int width, int height, int strideBytes = 0);
    /** @brief Ctrl 像素小窗用的 64 位浮点深度探针（米，弹出时显示毫米） */
    void setDepthProbeF64(const double* data, int width, int height, int strideBytes = 0);
    /** @brief 归一化灰度，供深度探针「灰度显示」一行 */
    void setDepthProbeGray(const QImage& gray);
    /** @brief 清掉深度探针，Ctrl 小窗回到灰度/RGB */
    void clearDepthProbe();
    /** @brief 是否已有底图 */
    bool hasImage() const;
    /** @brief 当前底图像素尺寸，无图时 0×0 */
    QSize currentImageSize() const;
    /** @brief 同 currentImageSize */
    QSize imageSize() const;
    /** @brief 当前底图帧号。没有 HsCommData::FrameOverlay 对齐时也可为 0 */
    qint64 currentFrameId() const;
    /**
     * @brief 换底图时是否连带清层。对之后的 setImage / setCompressedImage 生效。
     * @param policy refresh*=true 时清对应层，false 保留
     */
    void setImageRefreshPolicy(const ImageRefreshPolicy& policy);
    /** @brief 当前换底图策略 */
    ImageRefreshPolicy imageRefreshPolicy() const;

    /************************ 结果叠加（Stack） ************************/
    /**
     * @brief disp* 追加到 Stack 时使用的颜色
     */
    void setOverlayColor(const QColor& color);
    /** @brief disp* 追加到 Stack 时使用的线宽 */
    void setOverlayWidth(double widthPx);

    /** @brief 追加圆到 Stack，frameId 取当前底图 */
    void dispCircle(double x, double y, double r);
    /** @brief 追加圆到 Stack */
    void dispCircle(const HsCommData::CircleP& circle);
    /**
     * @brief 追加椭圆到 Stack
     * @param phi 旋转角。单位和正方向跟系统设置
     */
    void dispEllipse(double x, double y, double ra, double rb, double phi = 0.0);
    /** @brief 追加椭圆到 Stack */
    void dispEllipse(const HsCommData::EllipseP& ellipse);
    /** @brief 追加轴对齐矩形到 Stack */
    void dispRect1(double left, double top, double width, double height);
    /** @brief 追加轴对齐矩形到 Stack */
    void dispRect1(const HsCommData::Rect1P& rect);
    /** @brief 追加轴对齐矩形到 Stack（HsCommData::Box2） */
    void dispRect1(const HsCommData::Box2& box);
    /**
     * @brief 追加旋转矩形到 Stack
     * @param phi 旋转角。单位和正方向跟系统设置
     */
    void dispRect2(double x, double y, double l1, double l2, double phi = 0.0);
    /** @brief 追加旋转矩形到 Stack */
    void dispRect2(const HsCommData::Rect2P& rect);
    /** @brief 追加直线到 Stack */
    void dispLine(double x1, double y1, double x2, double y2);
    /** @brief 追加直线到 Stack */
    void dispLine(const HsCommData::LineP& line);
    /**
     * @brief 追加十字到 Stack
     * @param size 半宽，图像像素。小于 0 表示穿窗
     * @param angle 旋转角。单位和正方向跟系统设置。0 为轴对齐
     */
    void dispCross(double x, double y, double size = -1.0, double angle = 0.0);
    /**
     * @brief 追加十字到 Stack
     * @param center 中心。size 小于 0 表示穿窗
     * @param angle 旋转角。单位和正方向跟系统设置。0 为轴对齐
     */
    void dispCross(const HsCommData::Point2d& center, double size = -1.0, double angle = 0.0);
    /**
     * @brief 追加箭头到 Stack
     * @param x1,y1 尾；x2,y2 头
     */
    void dispArrow(double x1, double y1, double x2, double y2);
    /** @brief 追加箭头到 Stack */
    void dispArrow(const HsCommData::HsArrow& arrow);
    /** @brief 追加行程区域到 Stack */
    void dispRegion(const HsCommData::HsRegion& region);
    /** @brief 追加 XLD / 折线到 Stack */
    void dispContour(const HsCommData::HsContour& contour);

    /**
     * @brief 整页替换 Stack。frameId 须与底图一致才显示。
     * @param overlay 协议快照，不持有 Qt 对象
     */
    void setOverlay(const std::shared_ptr<const HsCommData::FrameOverlay>& overlay);
    /** @brief 由图形列表构造 HsCommData::FrameOverlay 后整页替换 */
    void setOverlay(const std::vector<HsCommData::Graphic>& items, int64_t frameId);
    /** @brief 替换 Stack，frameId 取当前底图 */
    void setOverlays(const std::vector<HsCommData::Graphic>& items);
    /** @brief 当前 Stack 快照 */
    std::shared_ptr<const HsCommData::FrameOverlay> overlay() const;
    /** @brief 清空 Stack */
    void clearWindow();
    /** @brief 同 clearWindow */
    void clearOverlays();
    /** @brief Stack 层显隐，不影响 Drawing ROI */
    void setOverlayVisible(bool visible);
    /** @brief Stack 是否可见。frameId 对不上时仍可能不画 */
    bool isOverlayVisible() const;
    /** @brief Stack 层显隐（同 setOverlayVisible） */
    void setStackVisible(bool visible);
    /** @brief Stack 层是否打开 */
    bool isStackVisible() const;
    /** @brief Overlay 刷新频率（Hz） */
    qreal fps() const;

    /************************ Drawing ROI（用户绘制，可改） ************************/
    /** @brief 替换 Drawing 层 ROI */
    void setDrawings(const std::vector<HsCommData::Graphic>& rois);
    /** @brief 当前 Drawing 层全部 ROI */
    std::vector<HsCommData::Graphic> drawings() const;
    /** @brief 按 id 更新或插入一条 Drawing。id 空则追加 */
    void setDrawing(const HsCommData::Graphic& roi);
    /** @brief 清空 Drawing */
    void clearDrawings();
    /** @brief 清空测量层 */
    void clearMeasures();
    /** @brief 删除一条测量。id 不是测量则不动 */
    void removeMeasure(const QString& id);
    /** @brief 清掉叠层、绘制、测量和文字，底图保留。绘制或测量中则先回到浏览 */
    void clearDisplay();
    /** @brief 当前选中 ROI。无选中时 id 为空 */
    HsCommData::Graphic selectedDrawing() const;
    /** @brief Drawing 层显隐 */
    void setDrawingVisible(bool visible);
    /** @brief Drawing 层是否打开 */
    bool isDrawingVisible() const;

    /************************ 画笔（新绘 ROI 与已选中同步） ************************/
    /** @brief 新绘 ROI 的画笔。已选中则同步改样式 */
    void setDrawingStyle(const HsCommData::GeomStyle& style);
    /** @brief 当前画笔 */
    HsCommData::GeomStyle drawingStyle() const;
    /** @brief 设置 ROI 画笔颜色 */
    void setPenColor(const QColor& color);
    /** @brief 设置 ROI 画笔线宽（屏幕像素，cosmetic） */
    void setPenWidth(double widthPx);

    /************************ 读选中 ROI（按 kind 拆开，失败返回 false） ************************/
    /**
     * @brief 读选中圆。kind 不符返回 false
     * @param x,y 圆心，r 半径
     */
    bool selectedAsCircle(double* x, double* y, double* r) const;
    /**
     * @brief 读选中椭圆
     * @param phi 旋转角。单位和正方向跟系统设置
     */
    bool selectedAsEllipse(double* x, double* y, double* ra, double* rb, double* phi) const;
    /** @brief 读选中轴对齐矩形，输出左上角与宽高 */
    bool selectedAsRect1(double* left, double* top, double* width, double* height) const;
    /**
     * @brief 读选中旋转矩形
     * @param phi 旋转角。单位和正方向跟系统设置
     */
    bool selectedAsRect2(double* x, double* y, double* l1, double* l2, double* phi) const;
    /** @brief 读选中直线两端 */
    bool selectedAsLine(double* x1, double* y1, double* x2, double* y2) const;
    /**
     * @brief 读选中十字
     * @param size 半宽。小于 0 表示穿窗
     * @param angle 旋转角。单位和正方向跟系统设置
     */
    bool selectedAsCross(double* x, double* y, double* size, double* angle = 0) const;
    /**
     * @brief 读选中箭头两端
     * @param x1,y1 尾；x2,y2 头
     */
    bool selectedAsArrow(double* x1, double* y1, double* x2, double* y2) const;

    /************************ 视口（只改变换，不重采样底图） ************************/
    /** @brief 当前可见图像矩形 */
    QRectF part() const;
    /** @brief 设置可见矩形，并同步宽高比 */
    void setPart(const QRectF& part);
    /** @brief 整幅图等比装入窗口 */
    void fitToWindow();
    /** @brief 1:1。图像中心对齐窗口中心 */
    void zoomActualSize();
    /** @brief 同 zoomActualSize */
    void resetView();
    /** @brief 以光标为锚放大 */
    void zoomIn();
    /** @brief 以光标为锚缩小 */
    void zoomOut();
    /** @brief 当前缩放（屏幕像素 / 图像像素） */
    qreal zoomScale() const;

    /************************ 坐标与取样 ************************/
    /** @brief 控件坐标 → 图像坐标 */
    QPointF widgetToImage(const QPointF& widgetPos) const;
    /** @brief 图像坐标 → 控件坐标 */
    QPointF imageToWidget(const QPointF& imagePt) const;
    /**
     * @brief 控件坐标换成图像整数像素
     * @param pixel 输出。无图或点在图外返回 false
     */
    bool mapWidgetToImagePixel(const QPointF& widgetPos, QPoint* pixel) const;
    /**
     * @brief 图像坐标取灰度。落在像素格上用 floor
     * @param gray 输出。彩色图先经 toLuma8
     */
    bool sampleGray(const QPointF& imagePt, double* gray) const;
    /**
     * @brief 取 RGB。单通道时三通道相同
     * @param r,g,b 输出，范围 0–255
     */
    bool sampleRgb(const QPointF& imagePt, int* r, int* g, int* b) const;

    /************************ 叠加文字（drawInfo，不进场景 Item） ************************/
    /**
     * @brief 按坐标系放一条叠加文字。先清空该坐标系下全部条目，message 为空则只清空。
     *
     * DrawInfoImage：锚点是图像坐标，随缩放平移；字号保持屏幕像素。
     * DrawInfoWindow：锚点是控件坐标，位置与字号都固定。
     * 文字画在前景，不进场景 Item，也不参与 ROI 命中。
     *
     * @param fontSizePx 屏幕像素字号
     * @param color 文字颜色。默认白，便于深色底图
     */
    void drawInfo(DrawInfoSpace space, const QString& message, const QPointF& position,
                  int fontSizePx = 14, const QColor& color = QColor(255, 255, 255));
    /** @brief 追加一条叠加文字。同位置不替换，message 为空则忽略 */
    void addDrawInfo(DrawInfoSpace space, const QString& message, const QPointF& position,
                     int fontSizePx = 14, const QColor& color = QColor(255, 255, 255));
    /** @brief 清除一种坐标系的叠加文字 */
    void clearDrawInfo(DrawInfoSpace space);
    /** @brief 清除全部叠加文字，两种坐标系都清 */
    void clearDrawInfo();

    /************************ HUD / 像素检验 / 信息条 ************************/
    /** @brief HUD（坐标/灰度）显隐 */
    void setHudVisible(bool visible);
    /** @brief HUD 是否打开 */
    bool isHudVisible() const;
    /** @brief 图像信息条文本，画在 HUD 下方或窗口底部 */
    void setImageInfoText(const QString& text);
    /** @brief 当前信息条文本 */
    QString imageInfoText() const;
    /** @brief 像素检验。高倍画格子和 RGB/灰度，与 HUD 开关独立 */
    void setPixelInspect(bool on);
    /** @brief 像素检验是否打开 */
    bool isPixelInspect() const;

    /************************ 底图显隐 ************************/
    /** @brief 底图层显隐 */
    void setImageVisible(bool visible);
    /** @brief 底图是否可见 */
    bool isImageVisible() const;

    /************************ 视口背景 ************************/
    /**
     * @brief 视口背景风格。棋盘格按屏幕像素画格；纯色模式忽略缩放。
     * @note 不读 ThemeMgr / 全局 QSS。ViewportCustom 见 setBackgroundColor。
     */
    void setViewportBackground(ViewportBackground mode);
    /** @brief 当前视口背景风格 */
    ViewportBackground viewportBackground() const;
    /**
     * @brief 为棋盘格或 Custom 指定颜色。无效色恢复该风格的默认配色。
     */
    void setViewportBackgroundCustomColor(const QColor& color);
    /** @brief 风格指定色。无效表示未指定 */
    QColor viewportBackgroundCustomColor() const;
    /** @brief 切到 Custom 并用该颜色铺视口 */
    void setBackgroundColor(const QColor& color);
    /** @brief 当前视口底色（含风格默认） */
    QColor backgroundColor() const;

    /************************ 浮动工具条 ************************/
    /** @brief 浮动工具条显隐；默认隐藏，需要时 setOverlayToolbarVisible(true) */
    void setOverlayToolbarVisible(bool visible);
    /** @brief 工具条是否可见 */
    bool isOverlayToolbarVisible() const;
    /** @brief 测量工具条显隐。未配置按钮时仍不显示 */
    void setMeasureToolbarVisible(bool visible);
    /** @brief 测量工具条是否可见 */
    bool isMeasureToolbarVisible() const;
    /** @brief 工具条停靠四角 */
    void setOverlayToolbarCorner(Qt::Corner corner);
    /** @brief 当前停靠角 */
    Qt::Corner overlayToolbarCorner() const;
    /** @brief 按钮垂直或水平排列 */
    void setOverlayToolbarOrientation(Qt::Orientation orientation);
    /** @brief 当前排列方向 */
    Qt::Orientation overlayToolbarOrientation() const;
    /** @brief 工具条皮肤 */
    void setOverlayToolbarSkin(OverlayToolbarSkin skin);
    /** @brief 当前工具条皮肤 */
    OverlayToolbarSkin overlayToolbarSkin() const;
    /** @brief 工具条控件，可继续往里加按钮 */
    QWidget* overlayToolbar() const;
    /** @brief 在工具条末尾加一个控件 */
    void addOverlayToolbarWidget(QWidget* widget);
    /** @brief 浮动工具条上要显示的测量按钮。只保留测量工具，空则不显示 */
    void setMeasureTools(const std::vector<DrawTool>& tools);
    /** @brief 当前显示的测量按钮 */
    std::vector<DrawTool> measureTools() const;
    /** @brief 已提交测量的读数。随当前标定变化 */
    QStringList measureResultLines() const;

    /************************ 存图 ************************/
    /**
     * @brief 保存当前底图原图，不含 Stack / Drawing / drawInfo。
     * @param error 失败时写入原因，可空
     */
    bool saveCurrentFrame(const QString& filePath, QString* error = 0) const;
    /**
     * @brief 保存窗口快照：底图 + 叠加 + drawInfo，不含浮动工具条。
     * @param error 失败时写入原因，可空
     */
    bool saveSnapshot(const QString& filePath, QString* error = 0) const;
    /** @brief 弹出保存快照对话框 */
    void promptSaveSnapshot();
    /** @brief 弹出导出原图对话框 */
    void promptSaveOriginal();

    /************************ 尺寸提示（Qt） ************************/
    /**
     * @brief 建议尺寸
     * @note 没有底图时给一个能放下工具条的缺省值，有图后按 sceneRect 与 sizeHint 不再把窗口撑爆。
     */
    QSize sizeHint() const override;
    /** @brief 最小建议尺寸。未设底图时也保留一块可交互区域 */
    QSize minimumSizeHint() const override;

// 对外信号
Q_SIGNALS:
    /************************ 信号 ************************/
    /** @brief ROI 已提交 */
    void roiCommitted(const HsCommData::Graphic& roi);
    /**
     * @brief 绘制会话结束。roi 在接受时与 roiCommitted 相同，取消时 id 为空。
     * @param accepted true 提交，false 取消
     */
    void drawSessionFinished(const HsCommData::Graphic& roi, bool accepted);
    /** @brief 点中一条 Drawing，id 可能为空 */
    void roiSelected(const HsCommData::Graphic& roi);
    /** @brief 点中 Stack 图元 */
    void stackItemClicked(const QString& graphicId);
    /** @brief 可见图像矩形变化 */
    void viewportChanged(const QRectF& part);
    /** @brief 光标在图像上移动。valid 为假时 gray 无意义 */
    void cursorMoved(const QPointF& imagePt, double gray, bool valid);
    /** @brief 当前绘制工具变化。参数为 DrawTool */
    void drawToolChanged(int tool);
    /** @brief 文本工具点下锚点。同时给出图像坐标和控件坐标 */
    void textAnchorPicked(const QPointF& imagePt, const QPointF& widgetPt);

protected:
    /************************ 绘制与输入（子类一般不要绕过） ************************/
    /** @brief 视口背景。棋盘格按屏幕像素画，不随缩放变格 */
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    /** @brief 画 HUD、drawInfo 与像素检验，不进场景 Item */
    void drawForeground(QPainter* painter, const QRectF& rect) override;
    /** @brief 窗口变了就重排 part */
    void resizeEvent(QResizeEvent* e) override;
    /** @brief 滚轮以光标为锚缩放 */
    void wheelEvent(QWheelEvent* e) override;
    /** @brief 按下：平移、落笔或拖手柄 */
    void mousePressEvent(QMouseEvent* e) override;
    /** @brief 移动：平移、拖手柄、橡皮筋、光标取样 */
    void mouseMoveEvent(QMouseEvent* e) override;
    /** @brief 松开：提交 ROI 或结束平移 */
    void mouseReleaseEvent(QMouseEvent* e) override;
    /** @brief 双击：结束折线，或在非模态下适应窗口 */
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    /** @brief 右键菜单：快照、原图、缩放、工具条停靠；折线闭合时不弹出 */
    void contextMenuEvent(QContextMenuEvent* e) override;
    /** @brief Esc 取消绘制，Enter 结束形状，Ctrl 打开像素小窗 */
    void keyPressEvent(QKeyEvent* e) override;
    /** @brief 松开 Ctrl 关掉像素小窗 */
    void keyReleaseEvent(QKeyEvent* e) override;
    /** @brief 光标离开时清 HUD 取样 */
    void leaveEvent(QEvent* e) override;
    /** @brief 第一次显示时 fit */
    void showEvent(QShowEvent* e) override;

private:
    class Private; ///< Pimpl 实现，定义在 HsImageWindow.cpp
    Private* d; ///< Pimpl
};

}

Q_DECLARE_METATYPE(HsCommData::Graphic)

#endif
