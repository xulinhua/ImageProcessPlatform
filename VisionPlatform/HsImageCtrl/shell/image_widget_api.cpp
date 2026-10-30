/**
 * @file image_widget_api.cpp
 * @brief image_widget_api 实现：把 HsImageWindow 包成带信息栏、录制按钮与深度显示的图像面板
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "image_widget/image_widget_api.hpp"
#include <HsCtrl/HsCloudWindow.h>

#include <cstring>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <mutex>
#include <vector>

#include <QBrush>
#include <QByteArray>
#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMetaObject>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtGlobal>
#include <QWidget>

namespace image_widget
{

/// 画布附加状态：深度显示与叠加缓存
struct LinuxCanvasState {
  QImage depthGray;                 ///< 深度归一化后的灰度图，伪彩切换时复用
  std::vector<HsCommData::Graphic> overlays; ///< 最近一次设置的叠加，深度模式下不显示
  bool depthPseudo = false;         ///< 深度图是否用伪彩显示
  bool depthMode = false;           ///< 当前底图是否为深度图（此时隐藏叠加）
  bool userAdjusted = false;        ///< 用户滚轮或拖动调过视图后，换图不再自动 fit
};

namespace
{
/// 把 QImage 转成 mono8 / rgb8 原始像素推给 HsImageWindow；空图则清底图
void pushDisplayImage(HsCtrl::HsImageWindow * window, const QImage & src)
{
  if (window == nullptr) {
    return;
  }
  if (src.isNull()) {
    window->clearImage();
    return;
  }
  QImage img = src;
  QString enc;
  if (img.format() == QImage::Format_Grayscale8) {
    enc = QStringLiteral("mono8");
  } else {
    if (img.format() != QImage::Format_RGB888) {
      img = img.convertToFormat(QImage::Format_RGB888);
    }
    enc = QStringLiteral("rgb8");
  }
  window->setImage(img.constBits(), img.width(), img.height(), img.bytesPerLine(), enc);
}

/// 灰度转伪彩：亮处偏红、暗处偏蓝（HSV 色相 0..240）
QImage applyPseudoColor(const QImage & gray)
{
  if (gray.isNull()) {
    return QImage();
  }
  QImage src = gray.convertToFormat(QImage::Format_Grayscale8);
  QImage dst(src.width(), src.height(), QImage::Format_RGB888);
  for (int y = 0; y < src.height(); ++y) {
    const uchar * src_row = src.constScanLine(y);
    uchar * dst_row = dst.scanLine(y);
    for (int x = 0; x < src.width(); ++x) {
      const int v = src_row[x];
      const int hue = static_cast<int>((255 - v) * 240 / 255);
      const QColor c = QColor::fromHsv(hue, 255, 255);
      dst_row[3 * x + 0] = static_cast<uchar>(c.red());
      dst_row[3 * x + 1] = static_cast<uchar>(c.green());
      dst_row[3 * x + 2] = static_cast<uchar>(c.blue());
    }
  }
  return dst;
}

/// 把带行跨度的深度数据逐行拷成紧排数组
template<typename T>
void copyDepthRows(const T * data, int width, int height, int stride_bytes, std::vector<T> * out)
{
  out->clear();
  if (data == nullptr || width <= 0 || height <= 0 || out == nullptr) {
    return;
  }
  const int effective = stride_bytes > 0
    ? stride_bytes
    : static_cast<int>(sizeof(T) * static_cast<size_t>(width));
  out->resize(static_cast<size_t>(width) * static_cast<size_t>(height));
  const unsigned char * base = reinterpret_cast<const unsigned char *>(data);
  for (int y = 0; y < height; ++y) {
    const T * src = reinterpret_cast<const T *>(base + y * effective);
    std::copy(src, src + width, out->begin() + static_cast<size_t>(y) * static_cast<size_t>(width));
  }
}

/// 用有效像素的最小最大值把深度线性拉到 0–255，无效像素置 0；全无效返回空图
template<typename T, typename ValidFn>
QImage buildNormalizedDepthImage(
  const T * data, int width, int height, int stride_bytes, ValidFn valid_fn)
{
  if (data == nullptr || width <= 0 || height <= 0) {
    return QImage();
  }

  const int effective_stride = stride_bytes > 0 ? stride_bytes : static_cast<int>(sizeof(T) * width);
  const unsigned char * base = reinterpret_cast<const unsigned char *>(data);

  double min_v = std::numeric_limits<double>::max();
  double max_v = std::numeric_limits<double>::lowest();

  for (int y = 0; y < height; ++y) {
    const T * row = reinterpret_cast<const T *>(base + y * effective_stride);
    for (int x = 0; x < width; ++x) {
      const T value = row[x];
      if (!valid_fn(value)) {
        continue;
      }
      const double v = static_cast<double>(value);
      min_v = std::min(min_v, v);
      max_v = std::max(max_v, v);
    }
  }

  if (min_v > max_v) {
    return QImage();
  }
  if (max_v <= min_v) {
    max_v = min_v + 1.0;
  }

  QImage gray(width, height, QImage::Format_Grayscale8);
  for (int y = 0; y < height; ++y) {
    const T * src_row = reinterpret_cast<const T *>(base + y * effective_stride);
    uchar * dst_row = gray.scanLine(y);
    for (int x = 0; x < width; ++x) {
      const T value = src_row[x];
      if (!valid_fn(value)) {
        dst_row[x] = 0;
        continue;
      }
      const double normalized = (static_cast<double>(value) - min_v) / (max_v - min_v);
      dst_row[x] = static_cast<uchar>(qBound(0.0, normalized * 255.0, 255.0));
    }
  }
  return gray;
}

/// 按编码把原始像素深拷成 QImage：mono 为灰度，rgba/bgra 为四通道，其余按三通道；bgr 交换红蓝
QImage copyRawToImage(
  const uint8_t * data, int width, int height, int stride_bytes, const QString & encoding)
{
  if (data == nullptr || width <= 0 || height <= 0) {
    return QImage();
  }
  const QString enc = encoding.toLower();
  QImage::Format format = QImage::Format_RGB888;
  int bpp = 3;
  bool swap_rb = false;
  if (enc.contains(QLatin1String("mono")) || enc == QLatin1String("8uc1")) {
    format = QImage::Format_Grayscale8;
    bpp = 1;
  } else if (enc.contains(QLatin1String("rgba")) || enc.contains(QLatin1String("bgra"))) {
    format = QImage::Format_RGBA8888;
    bpp = 4;
    swap_rb = enc.contains(QLatin1String("bgr"));
  } else {
    swap_rb = enc.contains(QLatin1String("bgr"));
  }
  const int stride = stride_bytes > 0 ? stride_bytes : bpp * width;
  QImage img(width, height, format);
  for (int y = 0; y < height; ++y) {
    memcpy(img.scanLine(y), data + static_cast<size_t>(y) * static_cast<size_t>(stride),
           static_cast<size_t>(bpp) * static_cast<size_t>(width));
  }
  if (swap_rb) {
    img = img.rgbSwapped();
  }
  return img;
}

/// 把缓存的叠加重新设给窗口；深度模式下改为清空叠加
void applyStoredOverlays(HsCtrl::HsImageWindow * window, LinuxCanvasState * st)
{
  if (window == nullptr || st == nullptr) {
    return;
  }
  if (st->depthMode) {
    window->clearOverlays();
    return;
  }
  window->setOverlays(st->overlays);
}

/// 进入深度模式，按伪彩开关显示深度灰度图
void pushDepthDisplay(HsCtrl::HsImageWindow * window, LinuxCanvasState * st)
{
  if (window == nullptr || st == nullptr) {
    return;
  }
  st->depthMode = true;
  const QImage display = st->depthPseudo ? applyPseudoColor(st->depthGray) : st->depthGray;
  pushDisplayImage(window, display);
  applyStoredOverlays(window, st);
}

/// 监视视口事件，记录用户是否手动调过视图
class ViewAdjustFilter final : public QObject
{
public:
  explicit ViewAdjustFilter(LinuxCanvasState * state, QObject * parent)
  : QObject(parent),
    state_(state)
  {
  }

  // 滚轮，或按住左键 / 中键拖动，视为用户调过视图；事件照常放行
  bool eventFilter(QObject *, QEvent * event) override
  {
    if (state_ == nullptr || event == nullptr) {
      return false;
    }
    if (event->type() == QEvent::Wheel) {
      state_->userAdjusted = true;
    } else if (event->type() == QEvent::MouseMove) {
      auto * me = static_cast<QMouseEvent *>(event);
      if (me->buttons() & (Qt::LeftButton | Qt::MiddleButton)) {
        state_->userAdjusted = true;
      }
    }
    return false;
  }

private:
  LinuxCanvasState * state_;
};
}  // namespace

/// \brief 录制时的红色虚线边框覆盖层
/// 不接收鼠标、不画背景，可切换显隐做闪烁
class RecordingBorderOverlay final : public QWidget
{
public:
  explicit RecordingBorderOverlay(QWidget * parent = nullptr)
  : QWidget(parent),
    border_visible_(true)
  {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
  }

  void setBorderVisible(bool visible)
  {
    border_visible_ = visible;
    update();
  }

  void toggleBorderVisible()
  {
    border_visible_ = !border_visible_;
    update();
  }

protected:
  void paintEvent(QPaintEvent * event) override
  {
    QWidget::paintEvent(event);
    if (!border_visible_) {
      return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor(220, 38, 38));
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(2.0);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const QRectF r = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    painter.drawRect(r);
  }

private:
  bool border_visible_;
};

namespace
{
/// \brief 取图标的资源路径
/// 图标放在 Qt 资源里
QString getIconPath(const QString & icon_name)
{
  // Qt 资源路径格式：:/icons/icon/xxx.png
  return ":/icons/icon/" + icon_name;
}

/// \brief 生成录制按钮图标
/// \param recording true 为停止录制图标，false 为开始录制图标
QIcon makeRecordIcon(bool recording)
{
  const QString icon_name = recording ? "stop-record.png" : "start-record.png";
  const QString icon_path = getIconPath(icon_name);
  
  if (QFile::exists(icon_path)) {
    return QIcon(icon_path);
  }
  
  // 资源里没有图标时返回空图标
  return QIcon();
}

/// \brief 面板几何事件过滤器
/// 监视 Resize/Show，触发回调同步覆盖层等子控件的位置
class PanelGeometryEventFilter final : public QObject
{
public:
  using SyncFn = std::function<void()>;

  explicit PanelGeometryEventFilter(SyncFn sync_fn, QObject * parent = nullptr)
  : QObject(parent),
    sync_fn_(std::move(sync_fn))
  {
  }

protected:
  bool eventFilter(QObject * watched, QEvent * event) override
  {
    if (event != nullptr && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
      if (sync_fn_) {
        sync_fn_();
      }
    }
    return QObject::eventFilter(watched, event);
  }

private:
  SyncFn sync_fn_;
};

class ToolButtonTooltipFilter final : public QObject
{
public:
  explicit ToolButtonTooltipFilter(QObject * parent = nullptr)
  : QObject(parent),
    popup_(new QLabel(nullptr))
  {
    popup_->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    popup_->setAttribute(Qt::WA_ShowWithoutActivating);
    popup_->setStyleSheet(
      QStringLiteral(
        "QLabel {"
        "  color: #191919;"
        "  background-color: #ffffff;"
        "  border: 1px solid #aaaaaa;"
        "  border-radius: 2px;"
        "  padding: 4px 6px;"
        "}"));
    popup_->hide();
  }

  ~ToolButtonTooltipFilter() override
  {
    if (popup_) {
      delete popup_;
      popup_ = nullptr;
    }
  }

  void setTip(QToolButton * button, const QString & tip)
  {
    if (button == nullptr) {
      return;
    }
    tips_[button] = tip;
  }

protected:
  bool eventFilter(QObject * watched, QEvent * event) override
  {
    if (event == nullptr || !tips_.contains(watched)) {
      return QObject::eventFilter(watched, event);
    }
    auto * button = qobject_cast<QWidget *>(watched);
    if (button == nullptr) {
      return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
      case QEvent::Enter:
      case QEvent::ToolTip:
        showTipFor(button);
        return event->type() == QEvent::ToolTip;
      case QEvent::Leave:
      case QEvent::Hide:
      case QEvent::MouseButtonPress:
        if (popup_ != nullptr) {
          popup_->hide();
        }
        break;
      default:
        break;
    }
    return QObject::eventFilter(watched, event);
  }

private:
  void showTipFor(QWidget * button)
  {
    if (button == nullptr || popup_ == nullptr) {
      return;
    }
    const QString text = tips_.value(button);
    if (text.isEmpty()) {
      popup_->hide();
      return;
    }
    popup_->setText(text);
    popup_->adjustSize();
    const QPoint pos = button->mapToGlobal(QPoint(
      button->width() + 8,
      (button->height() - popup_->height()) / 2));
    popup_->move(pos);
    popup_->show();
    popup_->raise();
  }

  QHash<QObject *, QString> tips_;
  QLabel * popup_;
};

/// \brief 底部信息条：长文本中间省略，且不向布局贡献水平 minimumSizeHint
class ElidedInfoLabel final : public QLabel
{
public:
  explicit ElidedInfoLabel(QWidget * parent = nullptr)
  : QLabel(parent)
  {
    setWordWrap(false);
    setTextFormat(Qt::PlainText);
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setMinimumWidth(0);
  }

  void setFullText(const QString & text)
  {
    full_text_ = text;
    refreshElide();
  }

  void refreshElideDisplay()
  {
    refreshElide();
  }

  QSize minimumSizeHint() const override
  {
    const QSize base = QLabel::minimumSizeHint();
    return QSize(0, base.height());
  }

protected:
  void resizeEvent(QResizeEvent * event) override
  {
    QLabel::resizeEvent(event);
    refreshElide();
  }

private:
  void refreshElide()
  {
    if (full_text_.isEmpty()) {
      setText(QString());
      return;
    }
    const int w = width();
    if (w <= 1) {
      setText(full_text_);
      return;
    }
    setText(fontMetrics().elidedText(full_text_, Qt::ElideMiddle, w));
  }

  QString full_text_;
};

/// \brief 面板容器：layout fill 模式下不向父布局贡献 minimumSizeHint
class ImageWidgetContainer final : public QWidget
{
public:
  explicit ImageWidgetContainer(QWidget * parent = nullptr)
  : QWidget(parent),
    layout_fill_neutral_minimum_(false)
  {
  }

  void setLayoutFillNeutralMinimum(bool enabled)
  {
    if (layout_fill_neutral_minimum_ == enabled) {
      return;
    }
    layout_fill_neutral_minimum_ = enabled;
    updateGeometry();
  }

  QSize minimumSizeHint() const override
  {
    if (layout_fill_neutral_minimum_) {
      return QSize(0, 0);
    }
    return QWidget::minimumSizeHint();
  }

protected:
  // 作为顶层窗口被关闭时只最小化并忽略关闭，不 hide，保持 isVisible()==true；
  // 避免外部 VisualizerMgrBase 按可见性判断而误删面板（b06d366）
  void closeEvent(QCloseEvent * event) override
  {
    if (isWindow()) {
      showMinimized();
      event->ignore();
      return;
    }
    QWidget::closeEvent(event);
  }

private:
  bool layout_fill_neutral_minimum_;
};
}  // namespace

// 组装面板：HsImageWindow 画布 + 浮动工具条录制按钮 + 底部信息条，并转发画布信号
ImageWidgetApi::ImageWidgetApi(QWidget * parent)
: QObject(),
  view_shell_(nullptr),
  image_window_(nullptr),
  linux_(new LinuxCanvasState()),
  container_widget_(nullptr),
  image_info_label_(nullptr),
  image_panel_widget_(nullptr),
  record_button_(nullptr),
  record_button_callback_(),
  image_info_overlay_bg_(235, 235, 235),
  image_info_overlay_fg_(0, 0, 0),
  display_bg_color_(0xe8, 0xec, 0xf1),
  recording_border_overlay_(nullptr),
  recording_border_timer_(nullptr),
  recording_border_geometry_filter_(nullptr),
  container_geometry_filter_(nullptr),
  toolbar_tooltip_filter_(nullptr),
  async_timer_(nullptr),
  lifetime_ok_(std::make_shared<std::atomic<bool>>(true))
{
  auto * container = new ImageWidgetContainer(parent);
  container_widget_ = container;

  QVBoxLayout * layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  image_panel_widget_ = new QWidget(container);
  image_panel_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QHBoxLayout * root_layout = new QHBoxLayout(image_panel_widget_);
  root_layout->setContentsMargins(6, 6, 6, 6);
  root_layout->setSpacing(6);

  QWidget * left_column = new QWidget(image_panel_widget_);
  left_column->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  QVBoxLayout * left_layout = new QVBoxLayout(left_column);
  left_layout->setContentsMargins(0, 0, 0, 0);
  left_layout->setSpacing(4);
  view_shell_ = new HsCtrl::HsViewShell(left_column);
  image_window_ = view_shell_->imageWindow();
  image_window_->setWindowMode(HsCtrl::WindowRuntime);
  image_window_->setHudVisible(false);
  view_shell_->setMinimumSize(320, 240);
  view_shell_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  image_window_->setStyleSheet(
    QStringLiteral("QGraphicsView { background: transparent; border: 1px solid #4b5563; border-radius: 4px; }"));
  auto * view_filter = new ViewAdjustFilter(linux_, image_window_);
  image_window_->installEventFilter(view_filter);
  if (image_window_->viewport() != nullptr) {
    image_window_->viewport()->installEventFilter(view_filter);
  }
  left_layout->addWidget(view_shell_, 1);

  auto * info_label = new ElidedInfoLabel(left_column);
  info_label->setFullText(QString::fromUtf8("分辨率: N/A"));
  image_info_label_ = info_label;
  applyImageInfoOverlayStyle();
  left_layout->addWidget(image_info_label_, 0);

  image_window_->setOverlayToolbarVisible(false);
  record_button_ = new QToolButton(image_window_->overlayToolbar());
  record_button_->setAutoRaise(true);
  record_button_->setIconSize(QSize(18, 18));
  record_button_->setFixedSize(32, 24);
  record_button_->setIcon(makeRecordIcon(false));
  record_button_->setVisible(false);
  image_window_->addOverlayToolbarWidget(record_button_);

  toolbar_tooltip_filter_ = new ToolButtonTooltipFilter(container);
  auto * toolbar_tip = static_cast<ToolButtonTooltipFilter *>(toolbar_tooltip_filter_);
  toolbar_tip->setTip(record_button_, "开始rosbag 录制");
  record_button_->installEventFilter(toolbar_tooltip_filter_);

  root_layout->addWidget(left_column, 1);
  layout->addWidget(image_panel_widget_, 1);

  recording_border_overlay_ = new RecordingBorderOverlay(container);
  recording_border_overlay_->hide();
  recording_border_timer_ = new QTimer(container);
  recording_border_timer_->setInterval(300);
  // 异步刷新定时器：在 GUI 线程提交 pending 帧；lifetime_ok_ 失效后回调直接返回
  async_timer_ = new QTimer(container);
  async_timer_->setSingleShot(false);
  {
    auto ok = lifetime_ok_;
    QObject::connect(async_timer_, &QTimer::timeout, container, [this, ok]() {
      if (!ok || !ok->load(std::memory_order_acquire)) {
        return;
      }
      commitPendingFrame();
    });
  }
  QObject::connect(recording_border_timer_, &QTimer::timeout, recording_border_overlay_, [this]() {
    if (recording_border_overlay_ == nullptr) {
      return;
    }
    recording_border_overlay_->toggleBorderVisible();
  });

  recording_border_geometry_filter_ = new PanelGeometryEventFilter(
    [this]() { syncRecordingBorderGeometry(); }, container);
  image_panel_widget_->installEventFilter(recording_border_geometry_filter_);

  container_geometry_filter_ = new PanelGeometryEventFilter(
    [this]() {
      if (layout_fill_enabled_) {
        syncDisplayAreaFromLayout();
      }
      if (auto * info = dynamic_cast<ElidedInfoLabel *>(image_info_label_)) {
        info->refreshElideDisplay();
      }
    }, container);
  container->installEventFilter(container_geometry_filter_);

  QObject::connect(record_button_, &QToolButton::clicked, container, [this]() {
    if (record_button_callback_) {
      record_button_callback_();
    }
  });

  QObject::connect(image_window_, &HsCtrl::HsImageWindow::roiCommitted,
    this, &ImageWidgetApi::roiCommitted);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::drawSessionFinished,
    this, &ImageWidgetApi::drawSessionFinished);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::roiSelected,
    this, &ImageWidgetApi::roiSelected);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::stackItemClicked,
    this, &ImageWidgetApi::stackItemClicked);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::viewportChanged,
    this, &ImageWidgetApi::viewportChanged);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::cursorMoved,
    this, &ImageWidgetApi::cursorMoved);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::drawToolChanged,
    this, &ImageWidgetApi::drawToolChanged);
  QObject::connect(image_window_, &HsCtrl::HsImageWindow::textAnchorPicked,
    this, &ImageWidgetApi::textAnchorPicked);

  syncRecordingBorderGeometry();
}

// 先停定时器、清 pending、卸事件过滤器，再同步删除容器（画布随之释放），最后释放壳层状态；
// 不用 deleteLater，并移除已投递的 DeferredDelete，避免外部 cam_mgr 析构后再被延迟删除（b06d366）
ImageWidgetApi::~ImageWidgetApi()
{
  if (lifetime_ok_) {
    lifetime_ok_->store(false, std::memory_order_release);
  }
  async_refresh_enabled_.store(false, std::memory_order_release);
  commit_posted_.store(false, std::memory_order_release);
  if (async_timer_ != nullptr) {
    async_timer_->stop();
    async_timer_->disconnect();
    QCoreApplication::removePostedEvents(async_timer_);
  }
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_image_ = QImage();
    pending_depth_gray_ = QImage();
    pending_overlays_.clear();
    pending_overlay_dirty_.store(false, std::memory_order_release);
    pending_depth_mode_ = false;
    pending_dirty_.store(false, std::memory_order_release);
  }
  if (recording_border_timer_ != nullptr) {
    recording_border_timer_->stop();
    recording_border_timer_->disconnect();
  }
  if (image_panel_widget_ != nullptr && recording_border_geometry_filter_ != nullptr) {
    image_panel_widget_->removeEventFilter(recording_border_geometry_filter_);
  }
  if (container_widget_ != nullptr && container_geometry_filter_ != nullptr) {
    container_widget_->removeEventFilter(container_geometry_filter_);
  }
  if (container_widget_ != nullptr) {
    QCoreApplication::removePostedEvents(container_widget_);
    QCoreApplication::removePostedEvents(container_widget_, QEvent::DeferredDelete);
    container_widget_->setParent(nullptr);
    delete container_widget_;
  }
  container_widget_ = nullptr;
  image_info_label_ = nullptr;
  image_panel_widget_ = nullptr;
  record_button_ = nullptr;
  async_timer_ = nullptr;
  if (linux_) {
    delete linux_;
    linux_ = nullptr;
  }
  view_shell_ = nullptr;
  image_window_ = nullptr;
  recording_border_overlay_ = nullptr;
  recording_border_timer_ = nullptr;
  recording_border_geometry_filter_ = nullptr;
  container_geometry_filter_ = nullptr;
  toolbar_tooltip_filter_ = nullptr;
}

// 对外唯一的面板控件
QWidget * ImageWidgetApi::widget() const
{
  return container_widget_;
}

// 内部画布，需要 QGraphicsView 信号时使用
HsCtrl::HsImageWindow * ImageWidgetApi::imageWindow() const
{
  return image_window_;
}

HsCtrl::HsViewShell * ImageWidgetApi::viewShell() const
{
  return view_shell_;
}

HsCtrl::HsCloudWindow * ImageWidgetApi::cloudWindow() const
{
  return view_shell_ ? view_shell_->cloudWindow() : nullptr;
}

// 非布局模式下直接设置面板几何
void ImageWidgetApi::setDisplayRect(int x, int y, int width, int height)
{
  if (image_window_ == nullptr) {
    return;
  }
  container_widget_->setGeometry(x, y, width, height);
  syncRecordingBorderGeometry();
}

// 设置底部信息条文本，过长时中间省略
void ImageWidgetApi::setImageInfoText(const QString & text)
{
  if (image_info_label_ == nullptr) {
    return;
  }
  if (auto * info = dynamic_cast<ElidedInfoLabel *>(image_info_label_)) {
    info->setFullText(text);
    return;
  }
  image_info_label_->setText(text);
}

// 底部信息条显隐
void ImageWidgetApi::setImageInfoOverlayVisible(bool visible)
{
  if (image_info_label_ == nullptr) {
    return;
  }
  image_info_label_->setVisible(visible);
}

// 信息条背景色与文字色
void ImageWidgetApi::setImageInfoOverlayStyle(const QColor & background, const QColor & foreground)
{
  image_info_overlay_bg_ = background;
  image_info_overlay_fg_ = foreground;
  applyImageInfoOverlayStyle();
}

// 显示区背景色：画布切到 Custom 并用此色填充
void ImageWidgetApi::setDisplayBackgroundColor(const QColor & color)
{
  display_bg_color_ = color;
  if (image_window_ != nullptr) {
    image_window_->setBackgroundColor(color);
  }
}

// 视口指定色，不改背景风格
void ImageWidgetApi::setDisplayBackgroundCustomColor(const QColor & color)
{
  if (image_window_ == nullptr) {
    return;
  }
  image_window_->setViewportBackgroundCustomColor(color);
}

// 当前视口指定色
QColor ImageWidgetApi::displayBackgroundCustomColor() const
{
  return image_window_ == nullptr ? QColor() : image_window_->viewportBackgroundCustomColor();
}

// 普通图换底：退出深度模式并恢复叠加；任意线程可调，非 GUI 线程时排队提交
void ImageWidgetApi::setImage(const QImage & image)
{
  queueOrApplyDisplay(image, false);
}

// 直接贴已备好的显示图，非 GUI 线程时转到 GUI 线程；不更新取样用的帧
void ImageWidgetApi::setPreparedImage(const QImage & image)
{
  if (image_window_ == nullptr || image.isNull() || container_widget_ == nullptr) {
    return;
  }
  if (QThread::currentThread() != container_widget_->thread()) {
    QImage held = image;
    QTimer::singleShot(0, container_widget_, [this, held]() {
      if (image_window_ != nullptr) {
        image_window_->setPreparedImage(held);
      }
    });
    return;
  }
  image_window_->setPreparedImage(image);
}

// 只换显示图，保持当前深度模式（外部自行伪彩时用）
void ImageWidgetApi::setDisplayImage(const QImage & image)
{
  queueOrApplyDisplay(image, depth_mode_.load(std::memory_order_acquire));
}

// 原始像素先深拷成 QImage，再按普通图换底
void ImageWidgetApi::setImageRaw(
  const uint8_t * data, int width, int height, int stride_bytes, const QString & encoding)
{
  queueOrApplyDisplay(copyRawToImage(data, width, height, stride_bytes, encoding), false);
}

// 按 ROS format 提示（取分号前部分）选 PNG / JPEG 解码，否则自动识别；解码失败或空数据会清底图
void ImageWidgetApi::setCompressedImage(const QByteArray & compressed_data, const QString & ros_format)
{
  QImage decoded;
  if (!compressed_data.isEmpty()) {
    QString hint = ros_format.toLower();
    if (hint.contains(QLatin1Char(';'))) {
      hint = hint.section(QLatin1Char(';'), 0, 0).trimmed();
    }
    const char * fmt = nullptr;
    if (hint.contains(QLatin1String("png"))) {
      fmt = "PNG";
    } else if (hint.contains(QLatin1String("jpeg")) || hint.contains(QLatin1String("jpg"))) {
      fmt = "JPEG";
    }
    if (fmt) {
      decoded.loadFromData(compressed_data, fmt);
    } else {
      decoded.loadFromData(compressed_data);
    }
  }
  queueOrApplyDisplay(decoded, false);
}

// 16 位深度（0 为无效）归一化后按伪彩开关上屏，同时拷一份原值给 Ctrl 小窗探针
void ImageWidgetApi::setDepthImageU16(
  const uint16_t * data, int width, int height, int stride_bytes)
{
  const QImage gray = buildNormalizedDepthImage<uint16_t>(
    data, width, height, stride_bytes,
    [](uint16_t value) { return value > 0; });
  const bool pseudo = depth_pseudo_.load(std::memory_order_acquire);
  const QImage display = pseudo ? applyPseudoColor(gray) : gray;
  PendingDepthProbe probe;
  copyDepthRows(data, width, height, stride_bytes, &probe.u16);
  if (!probe.u16.empty()) {
    probe.kind = PendingDepthProbe::U16;
    probe.w = width;
    probe.h = height;
  }
  queueOrApplyDisplay(display, true, gray, std::move(probe));
}

// 32 位浮点深度归一化后上屏，非有限值与非正值视为无效
void ImageWidgetApi::setDepthImageF32(
  const float * data, int width, int height, int stride_bytes)
{
  const QImage gray = buildNormalizedDepthImage<float>(
    data, width, height, stride_bytes,
    [](float value) { return std::isfinite(value) && value > 0.0f; });
  const bool pseudo = depth_pseudo_.load(std::memory_order_acquire);
  const QImage display = pseudo ? applyPseudoColor(gray) : gray;
  PendingDepthProbe probe;
  copyDepthRows(data, width, height, stride_bytes, &probe.f32);
  if (!probe.f32.empty()) {
    probe.kind = PendingDepthProbe::F32;
    probe.w = width;
    probe.h = height;
  }
  queueOrApplyDisplay(display, true, gray, std::move(probe));
}

// 64 位浮点深度归一化后上屏，非有限值与非正值视为无效
void ImageWidgetApi::setDepthImageF64(
  const double * data, int width, int height, int stride_bytes)
{
  const QImage gray = buildNormalizedDepthImage<double>(
    data, width, height, stride_bytes,
    [](double value) { return std::isfinite(value) && value > 0.0; });
  const bool pseudo = depth_pseudo_.load(std::memory_order_acquire);
  const QImage display = pseudo ? applyPseudoColor(gray) : gray;
  PendingDepthProbe probe;
  copyDepthRows(data, width, height, stride_bytes, &probe.f64);
  if (!probe.f64.empty()) {
    probe.kind = PendingDepthProbe::F64;
    probe.w = width;
    probe.h = height;
  }
  queueOrApplyDisplay(display, true, gray, std::move(probe));
}

// 伪彩开关；深度模式下用缓存的深度灰度重推一次，linux_ 只在 GUI 线程改
void ImageWidgetApi::setDepthPseudoColorEnabled(bool enabled)
{
  depth_pseudo_.store(enabled, std::memory_order_release);
  if (container_widget_ == nullptr ||
      !lifetime_ok_ || !lifetime_ok_->load(std::memory_order_acquire))
  {
    return;
  }
  if (QThread::currentThread() == container_widget_->thread()) {
    if (linux_ == nullptr) {
      return;
    }
    linux_->depthPseudo = enabled;
    if (linux_->depthMode) {
      pushDepthDisplay(image_window_, linux_);
    }
    return;
  }
  auto ok = lifetime_ok_;
  QTimer::singleShot(0, container_widget_, [this, ok, enabled]() {
    if (!ok || !ok->load(std::memory_order_acquire) || linux_ == nullptr) {
      return;
    }
    linux_->depthPseudo = enabled;
    if (linux_->depthMode) {
      pushDepthDisplay(image_window_, linux_);
    }
  });
}

// 异步刷新：开启后按间隔定时提交 pending 帧；关闭时立即提交一次剩余帧
void ImageWidgetApi::setAsyncRefreshEnabled(bool enabled, int interval_ms)
{
  async_refresh_enabled_.store(enabled, std::memory_order_release);
  if (interval_ms > 0) {
    async_interval_ms_ = interval_ms;
  }
  if (async_timer_ == nullptr || container_widget_ == nullptr) {
    return;
  }
  // QTimer 只能在所属线程启停，跨线程时转到 GUI 线程再调一次
  if (QThread::currentThread() != container_widget_->thread()) {
    auto ok = lifetime_ok_;
    const int interval = async_interval_ms_;
    QTimer::singleShot(0, container_widget_, [this, ok, enabled, interval]() {
      if (!ok || !ok->load(std::memory_order_acquire)) {
        return;
      }
      setAsyncRefreshEnabled(enabled, interval);
    });
    return;
  }
  if (enabled) {
    async_timer_->start(async_interval_ms_);
  } else {
    async_timer_->stop();
    commitPendingFrame();
  }
}

// GUI 线程上屏：深度图清叠加并设深度探针；普通图清探针并恢复缓存叠加
void ImageWidgetApi::applyDisplayImage(
  const QImage & image, bool depth_mode, const QImage & depth_gray,
  PendingDepthProbe probe)
{
  if (image_window_ == nullptr || linux_ == nullptr || container_widget_ == nullptr) {
    return;
  }
  linux_->depthMode = depth_mode;
  linux_->depthPseudo = depth_pseudo_.load(std::memory_order_acquire);
  depth_mode_.store(depth_mode, std::memory_order_release);
  if (depth_mode) {
    linux_->depthGray = depth_gray.isNull() ? image : depth_gray;
    pushDisplayImage(image_window_, image);
    image_window_->clearOverlays();
    if (probe.kind == PendingDepthProbe::U16) {
      image_window_->setDepthProbeU16(
        probe.u16.empty() ? nullptr : probe.u16.data(), probe.w, probe.h);
      image_window_->setDepthProbeGray(linux_->depthGray);
    } else if (probe.kind == PendingDepthProbe::F32) {
      image_window_->setDepthProbeF32(
        probe.f32.empty() ? nullptr : probe.f32.data(), probe.w, probe.h);
      image_window_->setDepthProbeGray(linux_->depthGray);
    } else if (probe.kind == PendingDepthProbe::F64) {
      image_window_->setDepthProbeF64(
        probe.f64.empty() ? nullptr : probe.f64.data(), probe.w, probe.h);
      image_window_->setDepthProbeGray(linux_->depthGray);
    }
  } else {
    linux_->depthGray = QImage();
    pushDisplayImage(image_window_, image);
    image_window_->clearDepthProbe();
    applyStoredOverlays(image_window_, linux_);
  }
}

// 向 GUI 线程投递一次提交，已投递未执行时不重复投递
void ImageWidgetApi::scheduleCommitOnGuiThread()
{
  if (container_widget_ == nullptr ||
      !lifetime_ok_ || !lifetime_ok_->load(std::memory_order_acquire))
  {
    return;
  }
  bool expected = false;
  if (!commit_posted_.compare_exchange_strong(
        expected, true, std::memory_order_acq_rel))
  {
    return;
  }
  auto ok = lifetime_ok_;
  QTimer::singleShot(0, container_widget_, [this, ok]() {
    commit_posted_.store(false, std::memory_order_release);
    if (!ok || !ok->load(std::memory_order_acquire)) {
      return;
    }
    commitPendingFrame();
  });
}

// GUI 线程直接上屏；其他线程只保留最新一帧到 pending，由定时器或单次投递提交
void ImageWidgetApi::queueOrApplyDisplay(
  QImage image, bool depth_mode, QImage depth_gray,
  PendingDepthProbe probe)
{
  if (container_widget_ == nullptr || image_window_ == nullptr ||
      !lifetime_ok_ || !lifetime_ok_->load(std::memory_order_acquire))
  {
    return;
  }

  // GUI 线程调用时同步上屏；再排队会和布局 / 换窗重入叠在一起，把堆写坏（e9324e5）
  if (QThread::currentThread() == container_widget_->thread()) {
    applyDisplayImage(image, depth_mode, depth_gray, std::move(probe));
    return;
  }

  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_image_ = std::move(image);
    pending_depth_gray_ = std::move(depth_gray);
    pending_depth_probe_ = std::move(probe);
    pending_depth_mode_ = depth_mode;
    pending_dirty_.store(true, std::memory_order_release);
  }

  if (async_refresh_enabled_.load(std::memory_order_acquire) &&
      async_timer_ != nullptr && async_timer_->isActive())
  {
    return;
  }
  scheduleCommitOnGuiThread();
}

// 取出 pending 的图像与叠加上屏，并记录两部分耗时
void ImageWidgetApi::commitPendingFrame()
{
  if (image_window_ == nullptr || container_widget_ == nullptr || linux_ == nullptr ||
      !lifetime_ok_ || !lifetime_ok_->load(std::memory_order_acquire))
  {
    return;
  }
  const bool imageDirty = pending_dirty_.load(std::memory_order_acquire);
  const bool overlayDirty = pending_overlay_dirty_.load(std::memory_order_acquire);
  if (!imageDirty && !overlayDirty) {
    return;
  }
  QImage image;
  QImage depth_gray;
  PendingDepthProbe probe;
  bool depth_mode = false;
  bool haveImage = false;
  std::vector<HsCommData::Graphic> overlays;
  bool haveOverlays = false;
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    if (pending_dirty_.exchange(false, std::memory_order_acq_rel)) {
      image = std::move(pending_image_);
      depth_gray = std::move(pending_depth_gray_);
      probe = std::move(pending_depth_probe_);
      depth_mode = pending_depth_mode_;
      pending_depth_mode_ = false;
      pending_depth_gray_ = QImage();
      pending_depth_probe_ = PendingDepthProbe();
      haveImage = true;
    }
    if (pending_overlay_dirty_.exchange(false, std::memory_order_acq_rel)) {
      overlays = std::move(pending_overlays_);
      pending_overlays_.clear();
      haveOverlays = true;
    }
  }
  if (haveOverlays) {
    linux_->overlays = std::move(overlays);
  }
  if (haveImage) {
    const auto started = std::chrono::steady_clock::now();
    applyDisplayImage(image, depth_mode, depth_gray, std::move(probe));
    commit_image_ms_.store(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - started).count(), std::memory_order_relaxed);
  }
  if (haveOverlays && !haveImage) {
    const auto started = std::chrono::steady_clock::now();
    applyStoredOverlays(image_window_, linux_);
    commit_stack_ms_.store(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - started).count(), std::memory_order_relaxed);
  } else if (haveOverlays) {
    commit_stack_ms_.store(0, std::memory_order_relaxed);
  }
}

// 最近一次提交的换图与叠加耗时（毫秒）
void ImageWidgetApi::lastCommitMs(double * image_ms, double * stack_ms) const
{
  if (image_ms != nullptr) {
    *image_ms = commit_image_ms_.load(std::memory_order_relaxed);
  }
  if (stack_ms != nullptr) {
    *stack_ms = commit_stack_ms_.load(std::memory_order_relaxed);
  }
}

// 浮动工具条显隐
void ImageWidgetApi::setOverlayToolbarEnabled(bool enabled)
{
  if (image_window_ == nullptr) {
    return;
  }
  image_window_->setOverlayToolbarVisible(enabled);
}

// 壳层背景风格映射为画布工具条皮肤，未知值按 Dark
void ImageWidgetApi::setOverlayToolbarBackgroundStyle(OverlayToolbarBackgroundStyle style)
{
  HsCtrl::OverlayToolbarSkin skin = HsCtrl::ToolbarSkinDark;
  switch (style) {
    case OverlayToolbarBackgroundStyle::Transparent:
      skin = HsCtrl::ToolbarSkinTransparent;
      break;
    case OverlayToolbarBackgroundStyle::White:
      skin = HsCtrl::ToolbarSkinLight;
      break;
    case OverlayToolbarBackgroundStyle::DarkSolid:
      skin = HsCtrl::ToolbarSkinDarkSolid;
      break;
    case OverlayToolbarBackgroundStyle::LightSolid:
      skin = HsCtrl::ToolbarSkinLightSolid;
      break;
    case OverlayToolbarBackgroundStyle::Dark:
    default:
      skin = HsCtrl::ToolbarSkinDark;
      break;
  }
  setOverlayToolbarSkin(skin);
}

// 设置测量工具条显示哪些按钮
void ImageWidgetApi::setMeasureTools(const std::vector<HsCtrl::DrawTool>& tools)
{
  if (image_window_ == nullptr) {
    return;
  }
  image_window_->setMeasureTools(tools);
}

// 工具条停靠角
void ImageWidgetApi::setOverlayToolbarCorner(Qt::Corner corner)
{
  if (image_window_ == nullptr) {
    return;
  }
  image_window_->setOverlayToolbarCorner(corner);
}

// 工具按钮垂直或水平排列
void ImageWidgetApi::setOverlayToolbarOrientation(Qt::Orientation orientation)
{
  if (image_window_ == nullptr) {
    return;
  }
  image_window_->setOverlayToolbarOrientation(orientation);
}

// 丢掉 pending 帧后清底图并退出深度模式，避免稍后又被旧 pending 帧覆盖
void ImageWidgetApi::clearImage()
{
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_image_ = QImage();
    pending_depth_gray_ = QImage();
    pending_depth_mode_ = false;
    pending_dirty_.store(false, std::memory_order_release);
  }
  queueOrApplyDisplay(QImage(), false);
}

// 当前底图像素尺寸，无图为空
QSize ImageWidgetApi::currentImageSize() const
{
  if (image_window_ == nullptr) {
    return QSize();
  }
  return image_window_->currentImageSize();
}

// 按图像尺寸设画布大小，超出上限时等比缩小；返回容器尺寸
QSize ImageWidgetApi::resizeContainerToImage(int max_display_width, int max_display_height)
{
  if (image_window_ == nullptr || container_widget_ == nullptr) {
    return QSize();
  }

  const QSize img = image_window_->currentImageSize();
  if (img.isEmpty()) {
    return container_widget_->size();
  }

  int display_w = img.width();
  int display_h = img.height();

  if (max_display_width > 0 && display_w > max_display_width) {
    const double scale = static_cast<double>(max_display_width) / static_cast<double>(display_w);
    display_w = max_display_width;
    display_h = std::max(1, static_cast<int>(std::lround(img.height() * scale)));
  }
  if (max_display_height > 0 && display_h > max_display_height) {
    const double scale = static_cast<double>(max_display_height) / static_cast<double>(display_h);
    display_h = max_display_height;
    display_w = std::max(1, static_cast<int>(std::lround(img.width() * scale)));
  }

  image_window_->setMinimumSize(display_w, display_h);
  image_window_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  if (image_window_->size().width() != display_w || image_window_->size().height() != display_h) {
    image_window_->resize(display_w, display_h);
  }

  container_widget_->setMinimumSize(0, 0);
  container_widget_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  container_widget_->adjustSize();
  container_widget_->updateGeometry();

  return container_widget_->size();
}

// 把画布设为指定显示尺寸；layout fill 模式下改为跟随布局
QSize ImageWidgetApi::resizeContainerToDisplaySize(int display_width, int display_height)
{
  if (image_window_ == nullptr || container_widget_ == nullptr) {
    return QSize();
  }
  if (layout_fill_enabled_) {
    syncDisplayAreaFromLayout();
    return container_widget_->size();
  }
  if (display_width <= 0 || display_height <= 0) {
    return container_widget_->size();
  }

  image_window_->setMinimumSize(display_width, display_height);
  image_window_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  if (image_window_->size().width() != display_width ||
    image_window_->size().height() != display_height)
  {
    image_window_->resize(display_width, display_height);
  }

  container_widget_->setMinimumSize(0, 0);
  container_widget_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  container_widget_->adjustSize();
  container_widget_->updateGeometry();

  return container_widget_->size();
}

// layout fill：面板由父布局决定大小，不再贡献最小尺寸
void ImageWidgetApi::setLayoutFillEnabled(bool enabled)
{
  layout_fill_enabled_ = enabled;
  if (auto * container = dynamic_cast<ImageWidgetContainer *>(container_widget_)) {
    container->setLayoutFillNeutralMinimum(enabled);
  }
  if (enabled) {
    syncDisplayAreaFromLayout();
  }
  if (auto * info = dynamic_cast<ElidedInfoLabel *>(image_info_label_)) {
    info->refreshElideDisplay();
  }
}

// 放开容器、面板与画布的尺寸约束，让布局重新分配
void ImageWidgetApi::syncDisplayAreaFromLayout()
{
  if (!layout_fill_enabled_ || image_window_ == nullptr || container_widget_ == nullptr) {
    return;
  }

  container_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  container_widget_->setMinimumSize(0, 0);
  container_widget_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

  if (image_panel_widget_ != nullptr) {
    image_panel_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    image_panel_widget_->setMinimumSize(0, 0);
    image_panel_widget_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
  }

  image_window_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  image_window_->setMinimumSize(0, 0);
  image_window_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

  if (QWidget * left_column = image_window_->parentWidget()) {
    if (QLayout * layout = left_column->layout()) {
      layout->activate();
    }
    left_column->updateGeometry();
  }
  container_widget_->updateGeometry();
}

// 用户没手动调过视口时适应窗口
void ImageWidgetApi::fitDisplayIfNotUserAdjusted()
{
  if (image_window_ == nullptr || linux_ == nullptr) {
    return;
  }
  if (linux_->userAdjusted) {
    return;
  }
  image_window_->fitToWindow();
}

// 画布控件当前尺寸
QSize ImageWidgetApi::displayAreaSize() const
{
  if (image_window_ == nullptr) {
    return QSize();
  }
  return image_window_->size();
}

// 画布控件坐标落到图像整数像素
bool ImageWidgetApi::mapWidgetToImagePixel(const QPointF &widget_pos, QPoint *pixel) const
{
  if (image_window_ == nullptr || pixel == nullptr) {
    return false;
  }
  return image_window_->mapWidgetToImagePixel(widget_pos, pixel);
}

// ============================ 叠加图形 ============================
// 设置叠加并缓存；非 GUI 线程时放进 pending 等提交，深度模式下不显示
void ImageWidgetApi::setOverlays(const std::vector<HsCommData::Graphic> & items)
{
  if (container_widget_ != nullptr &&
      QThread::currentThread() != container_widget_->thread()) {
    {
      std::lock_guard<std::mutex> lock(pending_mutex_);
      pending_overlays_ = items;
      pending_overlay_dirty_.store(true, std::memory_order_release);
    }
    if (async_refresh_enabled_.load(std::memory_order_acquire) &&
        async_timer_ != nullptr && async_timer_->isActive()) {
      return;
    }
    scheduleCommitOnGuiThread();
    return;
  }
  if (linux_ != nullptr) {
    linux_->overlays = items;
  }
  applyStoredOverlays(image_window_, linux_);
}

// 清空壳层缓存与画布叠加
void ImageWidgetApi::clearOverlays()
{
  if (linux_ != nullptr) {
    linux_->overlays.clear();
  }
  if (image_window_ != nullptr) {
    image_window_->clearOverlays();
  }
}

// 叠加层显隐
void ImageWidgetApi::setOverlayVisible(bool visible)
{
  if (image_window_ != nullptr) {
    image_window_->setOverlayVisible(visible);
  }
}

// 录制按钮显隐
void ImageWidgetApi::setRecordButtonVisible(bool visible)
{
  if (record_button_ == nullptr) {
    return;
  }
  record_button_->setVisible(visible);
}

// 按录制状态切换按钮图标与提示
void ImageWidgetApi::setRecordButtonState(bool recording)
{
  if (record_button_ == nullptr) {
    return;
  }
  record_button_->setIcon(makeRecordIcon(recording));
  if (toolbar_tooltip_filter_ != nullptr) {
    auto * toolbar_tip = static_cast<ToolButtonTooltipFilter *>(toolbar_tooltip_filter_);
    toolbar_tip->setTip(record_button_, recording ? "录制中，点击停止" : "开始rosbag 录制");
  }
}

// 录制按钮点击回调
void ImageWidgetApi::setRecordButtonCallback(const std::function<void()> & callback)
{
  record_button_callback_ = callback;
}

// 录制边框：开启时每 300ms 闪烁一次，关闭时隐藏
void ImageWidgetApi::setRecordingBorderActive(bool active)
{
  if (recording_border_overlay_ == nullptr || recording_border_timer_ == nullptr) {
    return;
  }

  if (!active) {
    recording_border_timer_->stop();
    recording_border_overlay_->hide();
    recording_border_overlay_->setBorderVisible(true);
    return;
  }

  syncRecordingBorderGeometry();
  recording_border_overlay_->setBorderVisible(true);
  recording_border_overlay_->show();
  recording_border_overlay_->raise();
  if (!recording_border_timer_->isActive()) {
    recording_border_timer_->start();
  }
}

// 按当前前景 / 背景色刷新信息条样式表
void ImageWidgetApi::applyImageInfoOverlayStyle()
{
  if (image_info_label_ == nullptr) {
    return;
  }
  image_info_label_->setStyleSheet(QString(
      "QLabel {"
      "  color: %1;"
      "  background-color: %2;"
      "  border: none;"
      "  border-top: 1px solid %3;"
      "  padding: 4px 8px;"
      "}")
    .arg(image_info_overlay_fg_.name(QColor::HexArgb))
    .arg(image_info_overlay_bg_.name(QColor::HexArgb))
    .arg(QColor("#e2e8f0").name()));
}

// 录制边框覆盖整个图像面板
void ImageWidgetApi::syncRecordingBorderGeometry()
{
  if (recording_border_overlay_ == nullptr || image_panel_widget_ == nullptr || container_widget_ == nullptr) {
    return;
  }

  const QPoint top_left = image_panel_widget_->mapTo(container_widget_, QPoint(0, 0));
  recording_border_overlay_->setGeometry(QRect(top_left, image_panel_widget_->size()));
  if (recording_border_timer_ != nullptr && recording_border_timer_->isActive()) {
    recording_border_overlay_->raise();
  }
}

// 是否有底图
bool ImageWidgetApi::hasImage() const
{
  return image_window_ != nullptr && image_window_->hasImage();
}

// 当前底图像素尺寸
QSize ImageWidgetApi::imageSize() const
{
  return currentImageSize();
}

// 当前底图帧号
qint64 ImageWidgetApi::currentFrameId() const
{
  return image_window_ != nullptr ? image_window_->currentFrameId() : 0;
}

// 设置协议 FrameOverlay 为 Stack，并缓存其图元
void ImageWidgetApi::setOverlay(const std::shared_ptr<const HsCommData::FrameOverlay> & overlay)
{
  if (linux_ != nullptr) {
    if (overlay) {
      linux_->overlays = overlay->items;
    } else {
      linux_->overlays.clear();
    }
  }
  if (image_window_ != nullptr) {
    image_window_->setOverlay(overlay);
  }
}

// 用 Graphic 列表设置 Stack，并缓存
void ImageWidgetApi::setOverlay(const std::vector<HsCommData::Graphic> & items, int64_t frameId)
{
  if (linux_ != nullptr) {
    linux_->overlays = items;
  }
  if (image_window_ != nullptr) {
    image_window_->setOverlay(items, frameId);
  }
}

// 当前 Stack 快照
std::shared_ptr<const HsCommData::FrameOverlay> ImageWidgetApi::overlay() const
{
  return image_window_ != nullptr ? image_window_->overlay()
                                  : std::shared_ptr<const HsCommData::FrameOverlay>();
}

// 清空 Stack 与壳层叠加缓存
void ImageWidgetApi::clearWindow()
{
  if (linux_ != nullptr) {
    linux_->overlays.clear();
  }
  if (image_window_ != nullptr) {
    image_window_->clearWindow();
  }
}

// 替换 Drawing 层
void ImageWidgetApi::setDrawings(const std::vector<HsCommData::Graphic> & rois)
{
  if (image_window_ != nullptr) {
    image_window_->setDrawings(rois);
  }
}

// 当前 Drawing
std::vector<HsCommData::Graphic> ImageWidgetApi::drawings() const
{
  return image_window_ != nullptr ? image_window_->drawings() : std::vector<HsCommData::Graphic>();
}

// 清空 Drawing
void ImageWidgetApi::clearDrawings()
{
  if (image_window_ != nullptr) {
    image_window_->clearDrawings();
  }
}

// 当前选中 ROI
HsCommData::Graphic ImageWidgetApi::selectedDrawing() const
{
  return image_window_ != nullptr ? image_window_->selectedDrawing() : HsCommData::Graphic();
}

// 按 id 更新或插入 Drawing
void ImageWidgetApi::setDrawing(const HsCommData::Graphic & roi)
{
  if (image_window_ != nullptr) {
    image_window_->setDrawing(roi);
  }
}

// 新绘 ROI 的画笔样式
void ImageWidgetApi::setDrawingStyle(const HsCommData::GeomStyle & style)
{
  if (image_window_ != nullptr) {
    image_window_->setDrawingStyle(style);
  }
}

// 当前画笔样式
HsCommData::GeomStyle ImageWidgetApi::drawingStyle() const
{
  return image_window_ != nullptr ? image_window_->drawingStyle() : HsCommData::GeomStyle();
}

// 底图层显隐
void ImageWidgetApi::setImageVisible(bool visible)
{
  if (image_window_ != nullptr) {
    image_window_->setImageVisible(visible);
  }
}

// 底图是否可见
bool ImageWidgetApi::isImageVisible() const
{
  return image_window_ != nullptr && image_window_->isImageVisible();
}

// 图像坐标下的可见矩形
QRectF ImageWidgetApi::part() const
{
  return image_window_ != nullptr ? image_window_->part() : QRectF();
}

// 设置可见矩形
void ImageWidgetApi::setPart(const QRectF & part)
{
  if (image_window_ != nullptr) {
    image_window_->setPart(part);
  }
}

// 整幅图适应窗口
void ImageWidgetApi::fitToWindow()
{
  if (image_window_ != nullptr) {
    image_window_->fitToWindow();
  }
}

// 1:1 显示
void ImageWidgetApi::zoomActualSize()
{
  if (image_window_ != nullptr) {
    image_window_->zoomActualSize();
  }
}

// 同 zoomActualSize
void ImageWidgetApi::resetView()
{
  if (image_window_ != nullptr) {
    image_window_->resetView();
  }
}

// 以视口中心放大
void ImageWidgetApi::zoomIn()
{
  if (image_window_ != nullptr) {
    image_window_->zoomIn();
  }
}

// 以视口中心缩小
void ImageWidgetApi::zoomOut()
{
  if (image_window_ != nullptr) {
    image_window_->zoomOut();
  }
}

// 当前缩放倍率
qreal ImageWidgetApi::zoomScale() const
{
  return image_window_ != nullptr ? image_window_->zoomScale() : 1.0;
}

// 设计态 / 运行态
void ImageWidgetApi::setWindowMode(HsCtrl::WindowMode mode)
{
  if (image_window_ != nullptr) {
    image_window_->setWindowMode(mode);
  }
}

// 当前窗口模式
HsCtrl::WindowMode ImageWidgetApi::windowMode() const
{
  return image_window_ != nullptr ? image_window_->windowMode() : HsCtrl::WindowRuntime;
}

// 设置当前绘制工具
void ImageWidgetApi::setDrawTool(HsCtrl::DrawTool tool)
{
  if (image_window_ != nullptr) {
    image_window_->setDrawTool(tool);
  }
}

// 当前绘制工具
HsCtrl::DrawTool ImageWidgetApi::drawTool() const
{
  return image_window_ != nullptr ? image_window_->drawTool() : HsCtrl::ToolBrowse;
}

// 开始一次绘制会话
bool ImageWidgetApi::beginDrawSession(HsCtrl::DrawTool tool)
{
  return image_window_ != nullptr && image_window_->beginDrawSession(tool);
}

// 是否正在绘制会话中
bool ImageWidgetApi::drawSessionActive() const
{
  return image_window_ != nullptr && image_window_->drawSessionActive();
}

// 提交当前会话
void ImageWidgetApi::commitDrawSession()
{
  if (image_window_ != nullptr) {
    image_window_->commitDrawSession();
  }
}

// 取消当前会话
void ImageWidgetApi::cancelDrawSession()
{
  if (image_window_ != nullptr) {
    image_window_->cancelDrawSession();
  }
}

// 画笔半径（图像像素）
void ImageWidgetApi::setBrushRadius(double radiusPx)
{
  if (image_window_ != nullptr) {
    image_window_->setBrushRadius(radiusPx);
  }
}

// 当前画笔半径，无画布时返回 8
double ImageWidgetApi::brushRadius() const
{
  return image_window_ != nullptr ? image_window_->brushRadius() : 8.0;
}

// 文本工具要写下的字符串
void ImageWidgetApi::setDrawText(const QString & text)
{
  if (image_window_ != nullptr) {
    image_window_->setDrawText(text);
  }
}

// 文本工具当前字符串
QString ImageWidgetApi::drawText() const
{
  return image_window_ != nullptr ? image_window_->drawText() : QString();
}

// 文本工具锚点坐标系
void ImageWidgetApi::setTextToolSpace(HsCtrl::DrawInfoSpace space)
{
  if (image_window_ != nullptr) {
    image_window_->setTextToolSpace(space);
  }
}

// 文本工具当前坐标系
HsCtrl::DrawInfoSpace ImageWidgetApi::textToolSpace() const
{
  return image_window_ != nullptr ? image_window_->textToolSpace() : HsCtrl::DrawInfoImage;
}

// 文本工具字号
void ImageWidgetApi::setTextFontPx(int px)
{
  if (image_window_ != nullptr) {
    image_window_->setTextFontPx(px);
  }
}

// 文本工具当前字号
int ImageWidgetApi::textFontPx() const
{
  return image_window_ != nullptr ? image_window_->textFontPx() : 14;
}

// HUD 显隐
void ImageWidgetApi::setHudVisible(bool visible)
{
  if (image_window_ != nullptr) {
    image_window_->setHudVisible(visible);
  }
}

// HUD 是否打开
bool ImageWidgetApi::isHudVisible() const
{
  return image_window_ != nullptr && image_window_->isHudVisible();
}

// 像素检验开关
void ImageWidgetApi::setPixelInspect(bool on)
{
  if (image_window_ != nullptr) {
    image_window_->setPixelInspect(on);
  }
}

// 像素检验是否打开
bool ImageWidgetApi::isPixelInspect() const
{
  return image_window_ != nullptr && image_window_->isPixelInspect();
}

// 实时标记
void ImageWidgetApi::setLive(bool live)
{
  if (image_window_ != nullptr) {
    image_window_->setLive(live);
  }
}

// 是否处于实时标记
bool ImageWidgetApi::isLive() const
{
  return image_window_ != nullptr && image_window_->isLive();
}

// Stack 层显隐
void ImageWidgetApi::setStackVisible(bool visible)
{
  if (image_window_ != nullptr) {
    image_window_->setStackVisible(visible);
  }
}

// Stack 层开关是否打开
bool ImageWidgetApi::isStackVisible() const
{
  return image_window_ != nullptr && image_window_->isStackVisible();
}

// Drawing 层显隐
void ImageWidgetApi::setDrawingVisible(bool visible)
{
  if (image_window_ != nullptr) {
    image_window_->setDrawingVisible(visible);
  }
}

// Drawing 层开关是否打开
bool ImageWidgetApi::isDrawingVisible() const
{
  return image_window_ != nullptr && image_window_->isDrawingVisible();
}

// 图像坐标取灰度
bool ImageWidgetApi::sampleGray(const QPointF & imagePt, double * gray) const
{
  return image_window_ != nullptr && image_window_->sampleGray(imagePt, gray);
}

// 图像坐标取 RGB
bool ImageWidgetApi::sampleRgb(const QPointF & imagePt, int * r, int * g, int * b) const
{
  return image_window_ != nullptr && image_window_->sampleRgb(imagePt, r, g, b);
}

// 控件坐标 → 图像坐标
QPointF ImageWidgetApi::widgetToImage(const QPointF & widgetPos) const
{
  return image_window_ != nullptr ? image_window_->widgetToImage(widgetPos) : QPointF();
}

// 图像坐标 → 控件坐标
QPointF ImageWidgetApi::imageToWidget(const QPointF & imagePt) const
{
  return image_window_ != nullptr ? image_window_->imageToWidget(imagePt) : QPointF();
}

// 叠加文字：先清空该坐标系下全部条目再放这一条
void ImageWidgetApi::drawInfo(HsCtrl::DrawInfoSpace space, const QString & message, const QPointF & position,
                              int fontSizePx, const QColor & color)
{
  if (image_window_ != nullptr) {
    image_window_->drawInfo(space, message, position, fontSizePx, color);
  }
}

// 追加一条叠加文字
void ImageWidgetApi::addDrawInfo(HsCtrl::DrawInfoSpace space, const QString & message, const QPointF & position,
                                 int fontSizePx, const QColor & color)
{
  if (image_window_ != nullptr) {
    image_window_->addDrawInfo(space, message, position, fontSizePx, color);
  }
}

// 清除一种坐标系的叠加文字
void ImageWidgetApi::clearDrawInfo(HsCtrl::DrawInfoSpace space)
{
  if (image_window_ != nullptr) {
    image_window_->clearDrawInfo(space);
  }
}

// 清除全部叠加文字
void ImageWidgetApi::clearDrawInfo()
{
  if (image_window_ != nullptr) {
    image_window_->clearDrawInfo();
  }
}

// 视口背景风格
void ImageWidgetApi::setViewportBackground(HsCtrl::ViewportBackground mode)
{
  if (image_window_ != nullptr) {
    image_window_->setViewportBackground(mode);
  }
}

// 当前视口背景风格
HsCtrl::ViewportBackground ImageWidgetApi::viewportBackground() const
{
  return image_window_ != nullptr ? image_window_->viewportBackground() : HsCtrl::ViewportDark;
}

// 切到 Custom 并设填充色
void ImageWidgetApi::setBackgroundColor(const QColor & color)
{
  if (image_window_ != nullptr) {
    image_window_->setBackgroundColor(color);
  }
}

// 当前填充参考色
QColor ImageWidgetApi::backgroundColor() const
{
  return image_window_ != nullptr ? image_window_->backgroundColor() : QColor();
}

// 视口指定色
void ImageWidgetApi::setViewportBackgroundCustomColor(const QColor & color)
{
  if (image_window_ != nullptr) {
    image_window_->setViewportBackgroundCustomColor(color);
  }
}

// 当前视口指定色
QColor ImageWidgetApi::viewportBackgroundCustomColor() const
{
  return image_window_ != nullptr ? image_window_->viewportBackgroundCustomColor() : QColor();
}

// 浮动工具条皮肤
void ImageWidgetApi::setOverlayToolbarSkin(HsCtrl::OverlayToolbarSkin skin)
{
  if (image_window_ != nullptr) {
    image_window_->setOverlayToolbarSkin(skin);
  }
}

// 当前工具条皮肤
HsCtrl::OverlayToolbarSkin ImageWidgetApi::overlayToolbarSkin() const
{
  return image_window_ != nullptr ? image_window_->overlayToolbarSkin() : HsCtrl::ToolbarSkinDark;
}

// 同 setOverlayToolbarEnabled
void ImageWidgetApi::setOverlayToolbarVisible(bool visible)
{
  setOverlayToolbarEnabled(visible);
}

// 工具条是否可见
bool ImageWidgetApi::isOverlayToolbarVisible() const
{
  return image_window_ != nullptr && image_window_->isOverlayToolbarVisible();
}

// 工具条停靠角
Qt::Corner ImageWidgetApi::overlayToolbarCorner() const
{
  return image_window_ != nullptr ? image_window_->overlayToolbarCorner() : Qt::TopRightCorner;
}

// 工具条排列方向
Qt::Orientation ImageWidgetApi::overlayToolbarOrientation() const
{
  return image_window_ != nullptr ? image_window_->overlayToolbarOrientation() : Qt::Vertical;
}

// 浮动工具条控件
QWidget * ImageWidgetApi::overlayToolbar() const
{
  return image_window_ != nullptr ? image_window_->overlayToolbar() : nullptr;
}

// 在工具条末尾插入控件
void ImageWidgetApi::addOverlayToolbarWidget(QWidget * widget)
{
  if (image_window_ != nullptr) {
    image_window_->addOverlayToolbarWidget(widget);
  }
}

// 保存当前原图像素
bool ImageWidgetApi::saveCurrentFrame(const QString & filePath, QString * error) const
{
  if (image_window_ == nullptr) {
    if (error != nullptr) {
      *error = QStringLiteral("no image window");
    }
    return false;
  }
  return image_window_->saveCurrentFrame(filePath, error);
}

// 截取当前视口显示
bool ImageWidgetApi::saveSnapshot(const QString & filePath, QString * error) const
{
  if (image_window_ == nullptr) {
    if (error != nullptr) {
      *error = QStringLiteral("no image window");
    }
    return false;
  }
  return image_window_->saveSnapshot(filePath, error);
}

// 换底图刷新策略
void ImageWidgetApi::setImageRefreshPolicy(const HsCtrl::ImageRefreshPolicy & policy)
{
  if (image_window_ != nullptr) {
    image_window_->setImageRefreshPolicy(policy);
  }
}

// 当前换底图刷新策略
HsCtrl::ImageRefreshPolicy ImageWidgetApi::imageRefreshPolicy() const
{
  return image_window_ != nullptr ? image_window_->imageRefreshPolicy() : HsCtrl::ImageRefreshPolicy();
}

// Overlay 刷新频率
qreal ImageWidgetApi::fps() const
{
  return image_window_ != nullptr ? image_window_->fps() : 0.0;
}

// 图像标定，用于测量与坐标换算
void ImageWidgetApi::setImageCalib(const HsCommData::ImageCalib & calib)
{
  if (image_window_ != nullptr) {
    image_window_->setImageCalib(calib);
  }
}

HsCommData::ImageCalib ImageWidgetApi::imageCalib() const
{
  return image_window_ != nullptr ? image_window_->imageCalib() : HsCommData::ImageCalib();
}

void ImageWidgetApi::cancelActiveDraw()
{
  if (image_window_ != nullptr) {
    image_window_->cancelActiveDraw();
  }
}

void ImageWidgetApi::setCrossSize(double size)
{
  if (image_window_ != nullptr) {
    image_window_->setCrossSize(size);
  }
}

double ImageWidgetApi::crossSize() const
{
  return image_window_ != nullptr ? image_window_->crossSize() : -1.0;
}

void ImageWidgetApi::setImage(
  const uint8_t * data, int width, int height, int strideBytes, const QString & encoding)
{
  setImageRaw(data, width, height, strideBytes, encoding);
}

void ImageWidgetApi::setImage(const cv::Mat & mat)
{
#ifdef HSIMAGE_HAS_OPENCV
  if (image_window_ != nullptr) {
    image_window_->setImage(mat);
  }
#else
  Q_UNUSED(mat);
#endif
}

void ImageWidgetApi::setDepthProbeU16(const uint16_t * data, int width, int height, int strideBytes)
{
  if (image_window_ != nullptr) {
    image_window_->setDepthProbeU16(data, width, height, strideBytes);
  }
}

void ImageWidgetApi::setDepthProbeF32(const float * data, int width, int height, int strideBytes)
{
  if (image_window_ != nullptr) {
    image_window_->setDepthProbeF32(data, width, height, strideBytes);
  }
}

void ImageWidgetApi::setDepthProbeF64(const double * data, int width, int height, int strideBytes)
{
  if (image_window_ != nullptr) {
    image_window_->setDepthProbeF64(data, width, height, strideBytes);
  }
}

void ImageWidgetApi::setDepthProbeGray(const QImage & gray)
{
  if (image_window_ != nullptr) {
    image_window_->setDepthProbeGray(gray);
  }
}

void ImageWidgetApi::clearDepthProbe()
{
  if (image_window_ != nullptr) {
    image_window_->clearDepthProbe();
  }
}

void ImageWidgetApi::setOverlayColor(const QColor & color)
{
  if (image_window_ != nullptr) {
    image_window_->setOverlayColor(color);
  }
}

void ImageWidgetApi::setOverlayWidth(double widthPx)
{
  if (image_window_ != nullptr) {
    image_window_->setOverlayWidth(widthPx);
  }
}

void ImageWidgetApi::dispCircle(double x, double y, double r)
{
  if (image_window_ != nullptr) {
    image_window_->dispCircle(x, y, r);
  }
}

void ImageWidgetApi::dispCircle(const HsCommData::CircleP & circle)
{
  if (image_window_ != nullptr) {
    image_window_->dispCircle(circle);
  }
}

void ImageWidgetApi::dispEllipse(double x, double y, double ra, double rb, double phi)
{
  if (image_window_ != nullptr) {
    image_window_->dispEllipse(x, y, ra, rb, phi);
  }
}

void ImageWidgetApi::dispEllipse(const HsCommData::EllipseP & ellipse)
{
  if (image_window_ != nullptr) {
    image_window_->dispEllipse(ellipse);
  }
}

void ImageWidgetApi::dispRect1(double left, double top, double width, double height)
{
  if (image_window_ != nullptr) {
    image_window_->dispRect1(left, top, width, height);
  }
}

void ImageWidgetApi::dispRect1(const HsCommData::Rect1P & rect)
{
  if (image_window_ != nullptr) {
    image_window_->dispRect1(rect);
  }
}

void ImageWidgetApi::dispRect1(const HsCommData::Box2 & box)
{
  if (image_window_ != nullptr) {
    image_window_->dispRect1(box);
  }
}

void ImageWidgetApi::dispRect2(double x, double y, double l1, double l2, double phi)
{
  if (image_window_ != nullptr) {
    image_window_->dispRect2(x, y, l1, l2, phi);
  }
}

void ImageWidgetApi::dispRect2(const HsCommData::Rect2P & rect)
{
  if (image_window_ != nullptr) {
    image_window_->dispRect2(rect);
  }
}

void ImageWidgetApi::dispLine(double x1, double y1, double x2, double y2)
{
  if (image_window_ != nullptr) {
    image_window_->dispLine(x1, y1, x2, y2);
  }
}

void ImageWidgetApi::dispLine(const HsCommData::LineP & line)
{
  if (image_window_ != nullptr) {
    image_window_->dispLine(line);
  }
}

void ImageWidgetApi::dispCross(double x, double y, double size, double angle)
{
  if (image_window_ != nullptr) {
    image_window_->dispCross(x, y, size, angle);
  }
}

void ImageWidgetApi::dispCross(const HsCommData::Point2d & center, double size, double angle)
{
  if (image_window_ != nullptr) {
    image_window_->dispCross(center, size, angle);
  }
}

void ImageWidgetApi::dispArrow(double x1, double y1, double x2, double y2)
{
  if (image_window_ != nullptr) {
    image_window_->dispArrow(x1, y1, x2, y2);
  }
}

void ImageWidgetApi::dispArrow(const HsCommData::HsArrow & arrow)
{
  if (image_window_ != nullptr) {
    image_window_->dispArrow(arrow);
  }
}

void ImageWidgetApi::dispRegion(const HsCommData::HsRegion & region)
{
  if (image_window_ != nullptr) {
    image_window_->dispRegion(region);
  }
}

void ImageWidgetApi::dispContour(const HsCommData::HsContour & contour)
{
  if (image_window_ != nullptr) {
    image_window_->dispContour(contour);
  }
}

bool ImageWidgetApi::isOverlayVisible() const
{
  return image_window_ != nullptr && image_window_->isOverlayVisible();
}

void ImageWidgetApi::clearMeasures()
{
  if (image_window_ != nullptr) {
    image_window_->clearMeasures();
  }
}

void ImageWidgetApi::removeMeasure(const QString & id)
{
  if (image_window_ != nullptr) {
    image_window_->removeMeasure(id);
  }
}

void ImageWidgetApi::clearDisplay()
{
  if (image_window_ != nullptr) {
    image_window_->clearDisplay();
  }
}

void ImageWidgetApi::setPenColor(const QColor & color)
{
  if (image_window_ != nullptr) {
    image_window_->setPenColor(color);
  }
}

void ImageWidgetApi::setPenWidth(double widthPx)
{
  if (image_window_ != nullptr) {
    image_window_->setPenWidth(widthPx);
  }
}

bool ImageWidgetApi::selectedAsCircle(double * x, double * y, double * r) const
{
  return image_window_ != nullptr && image_window_->selectedAsCircle(x, y, r);
}

bool ImageWidgetApi::selectedAsEllipse(
  double * x, double * y, double * ra, double * rb, double * phi) const
{
  return image_window_ != nullptr && image_window_->selectedAsEllipse(x, y, ra, rb, phi);
}

bool ImageWidgetApi::selectedAsRect1(
  double * left, double * top, double * width, double * height) const
{
  return image_window_ != nullptr && image_window_->selectedAsRect1(left, top, width, height);
}

bool ImageWidgetApi::selectedAsRect2(
  double * x, double * y, double * l1, double * l2, double * phi) const
{
  return image_window_ != nullptr && image_window_->selectedAsRect2(x, y, l1, l2, phi);
}

bool ImageWidgetApi::selectedAsLine(double * x1, double * y1, double * x2, double * y2) const
{
  return image_window_ != nullptr && image_window_->selectedAsLine(x1, y1, x2, y2);
}

bool ImageWidgetApi::selectedAsCross(double * x, double * y, double * size, double * angle) const
{
  return image_window_ != nullptr && image_window_->selectedAsCross(x, y, size, angle);
}

bool ImageWidgetApi::selectedAsArrow(double * x1, double * y1, double * x2, double * y2) const
{
  return image_window_ != nullptr && image_window_->selectedAsArrow(x1, y1, x2, y2);
}

QString ImageWidgetApi::imageInfoText() const
{
  return image_window_ != nullptr ? image_window_->imageInfoText() : QString();
}

void ImageWidgetApi::setMeasureToolbarVisible(bool visible)
{
  if (image_window_ != nullptr) {
    image_window_->setMeasureToolbarVisible(visible);
  }
}

bool ImageWidgetApi::isMeasureToolbarVisible() const
{
  return image_window_ != nullptr && image_window_->isMeasureToolbarVisible();
}

std::vector<HsCtrl::DrawTool> ImageWidgetApi::measureTools() const
{
  return image_window_ != nullptr ? image_window_->measureTools() : std::vector<HsCtrl::DrawTool>();
}

QStringList ImageWidgetApi::measureResultLines() const
{
  return image_window_ != nullptr ? image_window_->measureResultLines() : QStringList();
}

void ImageWidgetApi::promptSaveSnapshot()
{
  if (image_window_ != nullptr) {
    image_window_->promptSaveSnapshot();
  }
}

void ImageWidgetApi::promptSaveOriginal()
{
  if (image_window_ != nullptr) {
    image_window_->promptSaveOriginal();
  }
}

}  // namespace image_widget

#ifdef HSIMAGE_MOC_FROM_INTDIR
#include "moc_image_widget_api.cpp"
#endif

