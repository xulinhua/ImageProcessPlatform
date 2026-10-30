/**
 * @file image_widget_api.hpp
 * @brief 对外图像面板 ImageWidgetApi：画布转发、信息栏、深度伪彩、录制按钮与跨线程换图
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef IMAGE_WIDGET__IMAGE_WIDGET_API_HPP_
#define IMAGE_WIDGET__IMAGE_WIDGET_API_HPP_

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <memory>
#include <vector>

#include <HsCtrl/HsImageWindow.h>
#include <HsCtrl/HsCloudWindow.h>
#include <HsCtrl/HsViewShell.h>

class QLabel;
class QToolButton;
class QTimer;

namespace cv {
class Mat;
}

namespace image_widget
{

struct LinuxCanvasState;  ///< 壳层旁路状态，定义在 cpp

/// \brief 工具栏背景样式。转成 HsCtrl::OverlayToolbarSkin。
enum class OverlayToolbarBackgroundStyle {
  Dark,         ///< 深色半透明
  Transparent,  ///< 全透明
  White,        ///< 浅色半透明
  DarkSolid,    ///< 深色不透明
  LightSolid    ///< 浅色不透明
};

/// \brief 图像控件唯一对外类：工具条、信息栏、深度伪彩、录像，以及画布全部接口。
///        嵌入布局用 widget()。信号从本类发出。HsCtrl::HsImageWindow 只作为内部画布。
///        视口/工具条/drawInfo 枚举用 HsCtrl::ViewportBackground / OverlayToolbarSkin / DrawInfoSpace。
class HSCTRL_EXPORT ImageWidgetApi : public QObject
{
  Q_OBJECT
public:
  /// \brief 构造函数
  /// \param parent 父窗口部件
  explicit ImageWidgetApi(QWidget * parent = nullptr);
  ~ImageWidgetApi();

  /// \brief 获取容器控件实例
  /// \return 返回可嵌入布局的 QWidget 指针
  QWidget * widget() const;

  /** @brief 对外唯一图像控件（ROI / 缩放 / Ctrl 像素小窗） */
  HsCtrl::HsImageWindow * imageWindow() const;
  /** @brief 图像/点云壳（含右键切 2D·3D） */
  HsCtrl::HsViewShell * viewShell() const;
  /** @brief 点云窗；尚未进过 3D 时可能为空 */
  HsCtrl::HsCloudWindow * cloudWindow() const;

  /// \brief 设置控件位置和尺寸
  /// \param x X 坐标
  /// \param y Y 坐标
  /// \param width 宽度
  /// \param height 高度
  void setDisplayRect(int x, int y, int width, int height);

  /// \brief 设置图像信息条文本
  /// \param text 显示文本
  void setImageInfoText(const QString & text);

  /// \brief 设置信息条显隐
  /// \param visible true 显示，false 隐藏
  void setImageInfoOverlayVisible(bool visible = true);

  /// \brief 设置信息条样式
  /// \param background 背景色
  /// \param foreground 前景色（文字颜色）
  void setImageInfoOverlayStyle(
    const QColor & background = QColor(235, 235, 235),
    const QColor & foreground = QColor(0, 0, 0));

  /// \brief 设置图像显示区背景色（letterbox / 无图像区域；自动切 Custom 模式）
  void setDisplayBackgroundColor(const QColor & color);

  /// \brief 为背景风格指定颜色：棋盘格→格基色；固定深/浅→底色；无效色恢复默认
  void setDisplayBackgroundCustomColor(const QColor & color);

  /// \brief 当前风格指定色（无效 = 未指定，走风格默认配色）
  QColor displayBackgroundCustomColor() const;

  /// \brief 从 QImage 换底图（壳层转缓冲后交给 HsImageWindow）
  void setImage(const QImage & image);
  /// 直接换上调用方已经备好的图。不在控件里再拷一遍像素
  void setPreparedImage(const QImage & image);

  /// \brief 仅更新显示图像，保留深度模式（外部伪彩渲染后送入）
  void setDisplayImage(const QImage & image);

  /**
   * @brief 从原始像素换底图。内部转显示缓存，不向外返回图像对象
   * @param encoding rgb8 / bgr8 / rgba8 / bgra8 / mono8 / 8uc1
   */
  void setImageRaw(
    const uint8_t * data, int width, int height, int stride_bytes,
    const QString & encoding);

  /// \brief 解码压缩图像（JPEG/PNG 等，兼容 sensor_msgs/CompressedImage）
  void setCompressedImage(const QByteArray & compressed_data, const QString & ros_format = {});

  /// \brief 设置 16 位无符号深度图
  /// \param data 深度数据指针
  /// \param width 图像宽度
  /// \param height 图像高度
  /// \param stride_bytes 行字节数（可选，默认根据宽度计算）
  void setDepthImageU16(const uint16_t * data, int width, int height, int stride_bytes = 0);

  /// \brief 设置 32 位浮点深度图
  void setDepthImageF32(const float * data, int width, int height, int stride_bytes = 0);

  /// \brief 设置 64 位浮点深度图
  void setDepthImageF64(const double * data, int width, int height, int stride_bytes = 0);

  /// \brief 设置深度图伪彩显示开关
  /// \param enabled true 启用伪彩，false 灰度显示
  void setDepthPseudoColorEnabled(bool enabled);

  /// \brief 最近一次定时提交里，底图和叠层各花了多少毫秒
  void lastCommitMs(double * image_ms, double * stack_ms) const;

  /// \brief 设置异步刷新模式（须在 Qt 主线程调用）
  /// \param enabled true：非界面线程换帧只写入 pending，由 GUI 定时器提交
  /// \param interval_ms 刷新间隔（毫秒），默认 33ms（约 30fps）
  void setAsyncRefreshEnabled(bool enabled, int interval_ms = 33);

  /// \brief 设置工具栏显隐
  /// \param enabled true 显示工具栏
  void setOverlayToolbarEnabled(bool enabled);
  /// \brief 设置工具栏背景。White 对应浅色半透明。
  void setOverlayToolbarBackgroundStyle(OverlayToolbarBackgroundStyle style);
  /// \brief 浮动工具条上要显示的测量按钮。空则不显示测量项
  void setMeasureTools(const std::vector<HsCtrl::DrawTool> & tools);
  /// \brief 工具栏停靠四角
  void setOverlayToolbarCorner(Qt::Corner corner);
  /// \brief 工具按钮垂直或水平排列
  void setOverlayToolbarOrientation(Qt::Orientation orientation);

  /// \brief 设置录包按钮显隐
  void setRecordButtonVisible(bool visible);

  /// \brief 设置录包按钮状态
  /// \param recording true 录制中（显示方形图标），false 未录制（圆形图标）
  void setRecordButtonState(bool recording);

  /// \brief 设置录包按钮回调函数
  /// \param callback 点击按钮时执行的回调
  void setRecordButtonCallback(const std::function<void()> & callback);

  /// \brief 设置录制边框闪烁动画
  /// \param active true 启动红色边框闪烁，false 停止
  void setRecordingBorderActive(bool active);

  /// \brief 清除当前图像
  void clearImage();

  /// \brief 当前显示图像的像素尺寸（无图时返回 0×0）
  QSize currentImageSize() const;

  /// \brief 按图像宽高 1:1 调整显示区与容器尺寸（可限制最大显示宽高）
  /// \return 调整后的容器 widget 尺寸
  QSize resizeContainerToImage(int max_display_width = 0, int max_display_height = 0);

  /// \brief 将显示区设为指定像素尺寸（无图时也可用于占位，与有图面板对齐）
  /// \return 调整后的容器 widget 尺寸
  QSize resizeContainerToDisplaySize(int display_width, int display_height);

  /// \brief 当前图像显示区 widget 尺寸
  QSize displayAreaSize() const;

  /// \brief 将控件坐标转换为图像像素坐标（考虑缩放/平移）
  /// \param widget_pos 控件坐标系中的点
  /// \param pixel 输出的图像像素坐标
  /// \return 转换成功返回 true，无图像或坐标无效返回 false
  bool mapWidgetToImagePixel(const QPointF &widget_pos, QPoint *pixel) const;

  /// \brief 启用后容器随父布局伸缩，显示区由布局分配（不重置缩放/平移）
  void setLayoutFillEnabled(bool enabled);

  /// \brief 将显示区同步为当前布局可用空间（layout fill 模式下由 Resize 自动调用）
  void syncDisplayAreaFromLayout();

  /// \brief 用户未手动缩放/平移时，将图像适应当前显示区
  void fitDisplayIfNotUserAdjusted();

  // ---------- 结果叠加（协议 HsCommData::Graphic；深度模式暂不画）----------
  /** @brief 替换 Stack（frameId 取当前底图；并缓存供深度模式恢复） */
  void setOverlays(const std::vector<HsCommData::Graphic> & items);
  /// \brief 清除所有叠加图形
  void clearOverlays();
  /// \brief 叠加图形总显隐
  void setOverlayVisible(bool visible);

  // ---------- 画布转发（HsImageWindow）。Linux 用本类即可，不必再拿 imageWindow 调大部分接口 ----------
  /** @brief 是否有底图 */
  bool hasImage() const;
  /** @brief 同 currentImageSize */
  QSize imageSize() const;
  /** @brief 当前底图帧号 */
  qint64 currentFrameId() const;

  /** @brief 设置结果叠加（协议 HsCommData::FrameOverlay） */
  void setOverlay(const std::shared_ptr<const HsCommData::FrameOverlay> & overlay);
  /** @brief 由 HsCommData::Graphic 列表构造 HsCommData::FrameOverlay */
  void setOverlay(const std::vector<HsCommData::Graphic> & items, int64_t frameId);
  /** @brief 当前 Stack 快照 */
  std::shared_ptr<const HsCommData::FrameOverlay> overlay() const;
  /** @brief 清空 Stack */
  void clearWindow();

  /** @brief 替换 Drawing 层 ROI */
  void setDrawings(const std::vector<HsCommData::Graphic> & rois);
  /** @brief 当前 Drawing */
  std::vector<HsCommData::Graphic> drawings() const;
  /** @brief 清空 Drawing */
  void clearDrawings();
  /** @brief 当前选中 ROI */
  HsCommData::Graphic selectedDrawing() const;
  /** @brief 按 id 更新或插入 Drawing */
  void setDrawing(const HsCommData::Graphic & roi);
  /** @brief 新绘 ROI 画笔 */
  void setDrawingStyle(const HsCommData::GeomStyle & style);
  /** @brief 当前画笔 */
  HsCommData::GeomStyle drawingStyle() const;

  /** @brief 底图层显隐 */
  void setImageVisible(bool visible);
  bool isImageVisible() const;
  QRectF part() const;
  void setPart(const QRectF & part);
  void fitToWindow();
  void zoomActualSize();
  void resetView();
  void zoomIn();
  void zoomOut();
  qreal zoomScale() const;

  void setWindowMode(HsCtrl::WindowMode mode);
  HsCtrl::WindowMode windowMode() const;
  void setDrawTool(HsCtrl::DrawTool tool);
  HsCtrl::DrawTool drawTool() const;
  /** @brief 开始一次独占绘制，语义同 HsImageWindow::beginDrawSession */
  bool beginDrawSession(HsCtrl::DrawTool tool);
  /** @brief 是否在绘制会话中 */
  bool drawSessionActive() const;
  /** @brief 提交当前绘制会话 */
  void commitDrawSession();
  /** @brief 取消当前绘制会话 */
  void cancelDrawSession();
  /** @brief 画笔半径（图像像素），一位小数，夹到 0.5..256 */
  void setBrushRadius(double radiusPx);
  /** @brief 当前画笔半径（图像像素） */
  double brushRadius() const;
  void setDrawText(const QString & text);
  QString drawText() const;
  void setTextToolSpace(HsCtrl::DrawInfoSpace space);
  HsCtrl::DrawInfoSpace textToolSpace() const;
  void setTextFontPx(int px);
  int textFontPx() const;

  void setHudVisible(bool visible);
  bool isHudVisible() const;
  void setPixelInspect(bool on);
  bool isPixelInspect() const;
  void setLive(bool live);
  bool isLive() const;
  void setStackVisible(bool visible);
  bool isStackVisible() const;
  void setDrawingVisible(bool visible);
  bool isDrawingVisible() const;

  bool sampleGray(const QPointF & imagePt, double * gray) const;
  bool sampleRgb(const QPointF & imagePt, int * r, int * g, int * b) const;
  QPointF widgetToImage(const QPointF & widgetPos) const;
  QPointF imageToWidget(const QPointF & imagePt) const;

  /** @brief 叠加文字（图像/窗口坐标） */
  void drawInfo(HsCtrl::DrawInfoSpace space, const QString & message, const QPointF & position,
                int fontSizePx = 14, const QColor & color = QColor(255, 255, 255));
  void addDrawInfo(HsCtrl::DrawInfoSpace space, const QString & message, const QPointF & position,
                   int fontSizePx = 14, const QColor & color = QColor(255, 255, 255));
  void clearDrawInfo(HsCtrl::DrawInfoSpace space);
  void clearDrawInfo();

  /** @brief 视口背景（同 HsImageWindow） */
  void setViewportBackground(HsCtrl::ViewportBackground mode);
  HsCtrl::ViewportBackground viewportBackground() const;
  void setBackgroundColor(const QColor & color);
  QColor backgroundColor() const;
  void setViewportBackgroundCustomColor(const QColor & color);
  QColor viewportBackgroundCustomColor() const;
  /** @brief 浮动工具条皮肤（同 HsImageWindow） */
  void setOverlayToolbarSkin(HsCtrl::OverlayToolbarSkin skin);
  HsCtrl::OverlayToolbarSkin overlayToolbarSkin() const;
  /** @brief 同 setOverlayToolbarEnabled */
  void setOverlayToolbarVisible(bool visible);
  bool isOverlayToolbarVisible() const;
  Qt::Corner overlayToolbarCorner() const;
  Qt::Orientation overlayToolbarOrientation() const;
  QWidget * overlayToolbar() const;
  void addOverlayToolbarWidget(QWidget * widget);

  bool saveCurrentFrame(const QString & filePath, QString * error = 0) const;
  bool saveSnapshot(const QString & filePath, QString * error = 0) const;
  void setImageRefreshPolicy(const HsCtrl::ImageRefreshPolicy & policy);
  HsCtrl::ImageRefreshPolicy imageRefreshPolicy() const;
  qreal fps() const;

  void setImageCalib(const HsCommData::ImageCalib & calib);
  HsCommData::ImageCalib imageCalib() const;
  void cancelActiveDraw();
  void setCrossSize(double size);
  double crossSize() const;

  void setImage(const uint8_t * data, int width, int height, int strideBytes, const QString & encoding);
  void setImage(const cv::Mat & mat);

  void setDepthProbeU16(const uint16_t * data, int width, int height, int strideBytes = 0);
  void setDepthProbeF32(const float * data, int width, int height, int strideBytes = 0);
  void setDepthProbeF64(const double * data, int width, int height, int strideBytes = 0);
  void setDepthProbeGray(const QImage & gray);
  void clearDepthProbe();

  void setOverlayColor(const QColor & color);
  void setOverlayWidth(double widthPx);
  void dispCircle(double x, double y, double r);
  void dispCircle(const HsCommData::CircleP & circle);
  void dispEllipse(double x, double y, double ra, double rb, double phi = 0.0);
  void dispEllipse(const HsCommData::EllipseP & ellipse);
  void dispRect1(double left, double top, double width, double height);
  void dispRect1(const HsCommData::Rect1P & rect);
  void dispRect1(const HsCommData::Box2 & box);
  void dispRect2(double x, double y, double l1, double l2, double phi = 0.0);
  void dispRect2(const HsCommData::Rect2P & rect);
  void dispLine(double x1, double y1, double x2, double y2);
  void dispLine(const HsCommData::LineP & line);
  void dispCross(double x, double y, double size = -1.0, double angle = 0.0);
  void dispCross(const HsCommData::Point2d & center, double size = -1.0, double angle = 0.0);
  void dispArrow(double x1, double y1, double x2, double y2);
  void dispArrow(const HsCommData::HsArrow & arrow);
  void dispRegion(const HsCommData::HsRegion & region);
  void dispContour(const HsCommData::HsContour & contour);

  bool isOverlayVisible() const;
  void clearMeasures();
  void removeMeasure(const QString & id);
  void clearDisplay();
  void setPenColor(const QColor & color);
  void setPenWidth(double widthPx);
  bool selectedAsCircle(double * x, double * y, double * r) const;
  bool selectedAsEllipse(double * x, double * y, double * ra, double * rb, double * phi) const;
  bool selectedAsRect1(double * left, double * top, double * width, double * height) const;
  bool selectedAsRect2(double * x, double * y, double * l1, double * l2, double * phi) const;
  bool selectedAsLine(double * x1, double * y1, double * x2, double * y2) const;
  bool selectedAsCross(double * x, double * y, double * size, double * angle = nullptr) const;
  bool selectedAsArrow(double * x1, double * y1, double * x2, double * y2) const;

  QString imageInfoText() const;
  void setMeasureToolbarVisible(bool visible);
  bool isMeasureToolbarVisible() const;
  std::vector<HsCtrl::DrawTool> measureTools() const;
  QStringList measureResultLines() const;
  void promptSaveSnapshot();
  void promptSaveOriginal();

Q_SIGNALS:
  void roiCommitted(const HsCommData::Graphic & roi);
  void drawSessionFinished(const HsCommData::Graphic & roi, bool accepted);
  void roiSelected(const HsCommData::Graphic & roi);
  void stackItemClicked(const QString & graphicId);
  void viewportChanged(const QRectF & part);
  void cursorMoved(const QPointF & imagePt, double gray, bool valid);
  void drawToolChanged(int tool);
  void textAnchorPicked(const QPointF & imagePt, const QPointF & widgetPt);

private:
  HsCtrl::HsViewShell * view_shell_;      ///< 图像/点云壳
  HsCtrl::HsImageWindow * image_window_; ///< 壳内图像画布
  LinuxCanvasState * linux_;              ///< 深度伪彩 / 叠加缓存 / 视口标记
  QWidget * container_widget_;
  QLabel * image_info_label_; ///< 底部信息条
  QWidget * image_panel_widget_;
  QToolButton * record_button_;
  std::function<void()> record_button_callback_;
  QColor image_info_overlay_bg_;
  QColor image_info_overlay_fg_;
  QColor display_bg_color_;
  class RecordingBorderOverlay * recording_border_overlay_;
  QTimer * recording_border_timer_;
  QObject * recording_border_geometry_filter_;
  QObject * container_geometry_filter_;
  QObject * toolbar_tooltip_filter_;

  void applyImageInfoOverlayStyle();
  void syncRecordingBorderGeometry();
  /// 跨线程 pending 里的原始深度，供 Ctrl 小窗显示距离
  struct PendingDepthProbe
  {
    enum Kind { None = 0, U16, F32, F64 };
    Kind kind;
    int w;
    int h;
    std::vector<uint16_t> u16;
    std::vector<float> f32;
    std::vector<double> f64;
    PendingDepthProbe()
    : kind(None),
      w(0),
      h(0)
    {
    }
  };

  /// 将显示图推上画布。只允许在控件所在线程调用。
  /// probe.kind 非空时写入 HsImageWindow 深度探针；彩色换底则清除探针。
  void applyDisplayImage(
    const QImage & image, bool depth_mode, const QImage & depth_gray,
    PendingDepthProbe probe = PendingDepthProbe());
  /// 已在控件线程则立刻上屏。跨线程只深拷贝进 pending，禁止直接碰画布。
  /// 控件线程里再排队会和布局/换窗重入叠在一起，把堆写坏（e9324e5）。
  void queueOrApplyDisplay(
    QImage image, bool depth_mode, QImage depth_gray = QImage(),
    PendingDepthProbe probe = PendingDepthProbe());
  /// 把 pending 提交到画布。须在控件线程调用。
  void commitPendingFrame();
  /// 跨线程合并投递一次 GUI 提交。async 关或定时器未开时使用。
  void scheduleCommitOnGuiThread();

  bool layout_fill_enabled_ {false};

  QTimer * async_timer_ {nullptr};
  std::mutex pending_mutex_;
  QImage pending_image_;
  QImage pending_depth_gray_;
  PendingDepthProbe pending_depth_probe_;
  bool pending_depth_mode_ {false};
  std::vector<HsCommData::Graphic> pending_overlays_;
  std::atomic<bool> pending_overlay_dirty_ {false};
  std::atomic<double> commit_image_ms_ {0};
  std::atomic<double> commit_stack_ms_ {0};
  std::atomic<bool> pending_dirty_ {false};
  std::atomic<bool> async_refresh_enabled_ {false};
  std::atomic<bool> commit_posted_ {false}; ///< 已有 QueuedConnection 等待 commit
  std::atomic<bool> depth_mode_ {false};    ///< 供非界面线程读取，避免直接碰 linux_
  std::atomic<bool> depth_pseudo_ {false};
  /// 析构后异步回调一律短路，避免关窗重建时回调打到已释放对象（b06d366）
  std::shared_ptr<std::atomic<bool>> lifetime_ok_;
  int async_interval_ms_ {33};
};

}  // namespace image_widget

#endif  // IMAGE_WIDGET__IMAGE_WIDGET_API_HPP_
