/**
 * @file MainWindow.cpp
 * @brief MainWindow实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MainWindow.h"
#include <HsCommData/HsSystem.h>
#include "HalconAlgo.h"

#include <QAbstractButton>
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>
#include <memory>

using namespace HsCtrl;

// 源码为 UTF-8：窄字面量用 fromUtf8 转 QString
static QString u8(const char* s)
{
    return QString::fromUtf8(s);
}

// 目录下按文件名排序的图像路径
static QStringList listImageFiles(const QString& dirPath)
{
    QDir dir(dirPath);
    const QStringList names = dir.entryList(
        QStringList()
            << QStringLiteral("*.png")
            << QStringLiteral("*.jpg")
            << QStringLiteral("*.jpeg")
            << QStringLiteral("*.bmp")
            << QStringLiteral("*.tif")
            << QStringLiteral("*.tiff"),
        QDir::Files, QDir::Name);
    QStringList out;
    for (int i = 0; i < names.size(); ++i) {
        out.push_back(dir.absoluteFilePath(names.at(i)));
    }
    return out;
}

// pixmap 包装为图标
static QIcon iconFromPixmap(const QPixmap& pm)
{
    return QIcon(pm);
}

// 32x32 透明画布
static QPixmap toolCanvas()
{
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    return pm;
}

// 浏览工具图标
static QIcon iconBrowse()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.6));
    p.setBrush(QColor(0xFA, 0xFA, 0xFA));
    QPainterPath path;
    path.moveTo(8, 7);
    path.lineTo(8, 24);
    path.lineTo(13, 20);
    path.lineTo(16, 27);
    path.lineTo(19, 25);
    path.lineTo(16, 18);
    path.lineTo(22, 18);
    path.closeSubpath();
    p.drawPath(path);
    p.end();
    return iconFromPixmap(pm);
}

// 圆工具图标
static QIcon iconCircle()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(6, 6, 20, 20));
    p.end();
    return iconFromPixmap(pm);
}

// 轴对齐矩形图标
static QIcon iconRect1()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(7, 9, 18, 14));
    p.end();
    return iconFromPixmap(pm);
}

// 旋转矩形图标
static QIcon iconRect2()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.setBrush(Qt::NoBrush);
    QPolygon poly;
    poly << QPoint(10, 8) << QPoint(24, 12) << QPoint(20, 24) << QPoint(6, 20);
    p.drawPolygon(poly);
    p.end();
    return iconFromPixmap(pm);
}

// 椭圆工具图标
static QIcon iconEllipse()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(5, 9, 22, 14));
    p.end();
    return iconFromPixmap(pm);
}

// 直线工具图标
static QIcon iconLine()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.drawLine(QPoint(7, 24), QPoint(25, 8));
    p.setBrush(QColor(0x33, 0x33, 0x33));
    p.drawEllipse(QPoint(7, 24), 2, 2);
    p.drawEllipse(QPoint(25, 8), 2, 2);
    p.end();
    return iconFromPixmap(pm);
}

// 十字工具图标
static QIcon iconCross()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.drawLine(QPoint(16, 6), QPoint(16, 26));
    p.drawLine(QPoint(6, 16), QPoint(26, 16));
    p.end();
    return iconFromPixmap(pm);
}

// 多边形 / HsRegion 图标
static QIcon iconRegion()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.6));
    p.setBrush(QColor(0xFA, 0xAD, 0x14, 80));
    QPolygon poly;
    poly << QPoint(7, 10) << QPoint(16, 6) << QPoint(25, 12) << QPoint(22, 24) << QPoint(9, 23);
    p.drawPolygon(poly);
    p.end();
    return iconFromPixmap(pm);
}

// XLD 折线图标
static QIcon iconXld()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.8));
    p.drawPolyline(QPolygon() << QPoint(6, 22) << QPoint(12, 10) << QPoint(18, 18) << QPoint(26, 8));
    p.setBrush(QColor(0x33, 0x33, 0x33));
    p.drawEllipse(QPoint(6, 22), 2, 2);
    p.drawEllipse(QPoint(12, 10), 2, 2);
    p.drawEllipse(QPoint(18, 18), 2, 2);
    p.drawEllipse(QPoint(26, 8), 2, 2);
    p.end();
    return iconFromPixmap(pm);
}

// 文本工具图标
static QIcon iconText()
        {
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    QFont f = p.font();
    f.setPixelSize(18);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(0x33, 0x33, 0x33));
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("T"));
    p.end();
    return iconFromPixmap(pm);
}
static QIcon iconFit()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(8, 8, 16, 16));
    p.drawLine(8, 8, 12, 12);
    p.drawLine(24, 8, 20, 12);
    p.drawLine(8, 24, 12, 20);
    p.drawLine(24, 24, 20, 20);
    p.end();
    return iconFromPixmap(pm);
}

// 1:1 图标
static QIcon iconOneToOne()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    QFont f = p.font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(0x33, 0x33, 0x33));
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("1:1"));
    p.end();
    return iconFromPixmap(pm);
}

// 清空图标
static QIcon iconClear()
{
    QPixmap pm = toolCanvas();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x33, 0x33, 0x33), 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(6, 6, 20, 20));
    p.drawLine(11, 11, 21, 21);
    p.end();
    return iconFromPixmap(pm);
}

// 生成棋盘+几何的演示灰度图
static QImage makeDemoImage()
{
    QImage img(1280, 960, QImage::Format_Grayscale8);
    for (int y = 0; y < img.height(); ++y) {
        uchar* line = img.scanLine(y);
        for (int x = 0; x < img.width(); ++x) {
            const int v = 40 + ((x / 40 + y / 40) % 2) * 30 + (x * 80 / img.width());
            line[x] = static_cast<uchar>(qBound(0, v, 255));
        }
    }
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(210, 210, 210));
    p.drawEllipse(QPoint(420, 380), 90, 90);
    p.setBrush(QColor(30, 30, 30));
    p.drawRect(QRect(760, 240, 220, 160));
    p.setPen(QPen(QColor(180, 180, 180), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRect(160, 620, 320, 180));
    p.end();
    return img;
}

// kind 显示名
static QString kindName(const HsCommData::Graphic& g)
{
    if (g.kind == HsCommData::KindObject && g.geom.isInitialized()) {
        switch (g.geom.kind()) {
        case HsCommData::ObjCircle:  return QStringLiteral("Circle");
        case HsCommData::ObjEllipse: return QStringLiteral("Ellipse");
        case HsCommData::ObjRect1:   return QStringLiteral("Rect1");
        case HsCommData::ObjRect2:   return QStringLiteral("Rect2");
        case HsCommData::ObjLine:    return QStringLiteral("Line");
        case HsCommData::ObjContour: return QStringLiteral("XLD");
        case HsCommData::ObjRegion:  return QStringLiteral("Region");
        case HsCommData::ObjMixed:   return QStringLiteral("Object");
        default: break;
        }
    }
    switch (g.kind) {
    case HsCommData::KindObject:    return QStringLiteral("Object");
    case HsCommData::KindCross:     return QStringLiteral("Cross");
    case HsCommData::KindPoint:     return QStringLiteral("Point");
    case HsCommData::KindText:      return QStringLiteral("Text");
    case HsCommData::KindKeypoints: return QStringLiteral("Keypoints");
    case HsCommData::KindSkeleton:  return QStringLiteral("Skeleton");
    case HsCommData::KindTextBadge: return QStringLiteral("Badge");
    case HsCommData::KindMask:      return QStringLiteral("Mask");
    case HsCommData::KindArrow:     return QStringLiteral("Arrow");
    default:                        return QStringLiteral("--");
    }
}

// 组装界面并加载演示图
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_api(nullptr)
    , m_view(nullptr)
    , m_toolCaption(nullptr)
    , m_toolGroup(nullptr)
    , m_colorGroup(nullptr)
    , m_widthSpin(nullptr)
    , m_lineCombo(nullptr)
    , m_strokeRadio(nullptr)
    , m_fillRadio(nullptr)
    , m_typeEdit(nullptr)
    , m_roleEdit(nullptr)
    , m_roiGeom(nullptr)
    , m_textEdit(nullptr)
    , m_textXSpin(nullptr)
    , m_textYSpin(nullptr)
    , m_textFontSpin(nullptr)
    , m_textImageRadio(nullptr)
    , m_textWindowRadio(nullptr)
    , m_textUiBusy(false)
    , m_layerImage(nullptr)
    , m_layerStack(nullptr)
    , m_layerDrawing(nullptr)
    , m_layerHud(nullptr)
    , m_pixelInspect(nullptr)
    , m_toolbarVisible(nullptr)
    , m_measDist(nullptr)
    , m_measAngle(nullptr)
    , m_measRadius(nullptr)
    , m_measRect(nullptr)
    , m_measEllipse(nullptr)
    , m_calibPreset(nullptr)
    , m_calibUnit(nullptr)
    , m_calibX(nullptr)
    , m_calibY(nullptr)
    , m_showPx(nullptr)
    , m_showMm(nullptr)
    , m_showUm(nullptr)
    , m_showPulse(nullptr)
    , m_measureLog(nullptr)
    , m_calibBusy(false)
    , m_toolbarCorner(nullptr)
    , m_toolbarVertical(nullptr)
    , m_toolbarHorizontal(nullptr)
    , m_toolbarSkin(nullptr)
    , m_viewportBg(nullptr)
    , m_viewportColorBtn(nullptr)
    , m_refreshStack(nullptr)
    , m_refreshDrawing(nullptr)
    , m_refreshText(nullptr)
    , m_imagePathLabel(nullptr)
    , m_prevImageBtn(nullptr)
    , m_nextImageBtn(nullptr)
    , m_thSlider(nullptr)
    , m_thSpin(nullptr)
    , m_algoStats(nullptr)
    , m_algoLive(nullptr)
    , m_algoStress(nullptr)
    , m_algoColorGroup(nullptr)
    , m_algoPickBtn(nullptr)
    , m_algoOpacitySlider(nullptr)
    , m_algoOpacitySpin(nullptr)
    , m_liveTimer(new QTimer(this))
    , m_algoTimer(new QTimer(this))
    , m_algoPushCount(0)
    , m_algoPushHz(0)
    , m_algoBusy(false)
    , m_frameSeq(1)
    , m_penColor(QColor(0xFA, 0xAD, 0x14))
    , m_customColor(Qt::white)
    , m_algoColor(QColor(0x52, 0xC4, 0x1A))
    , m_folderIndex(-1)
{
    for (int i = 0; i < 4; ++i) {
        m_colorBtns[i] = nullptr;
        m_algoColorBtns[i] = nullptr;
    }
    setWindowTitle(u8("图像交互  HsImageWindow"));
    resize(1480, 860);
    applyShellStyle();
    buildUi();
    loadGeneratedImage();
    applyPenToView();
    m_algoHzClock.start();
    m_algoTimer->setTimerType(Qt::PreciseTimer);
    connect(m_algoTimer, &QTimer::timeout, this, &MainWindow::runAlgoRefresh);
    useCurrentImage();

    connect(m_liveTimer, &QTimer::timeout, this, [this]() {
        if (m_sourceImage.isNull()) {
            return;
        }
        QImage img = m_sourceImage.copy();
        QPainter p(&img);
        p.setPen(QPen(QColor(255, 255, 255), 2));
        const int t = static_cast<int>(m_frameSeq % 360);
        p.drawEllipse(QPoint(640, 480), 40 + (t % 40), 40 + (t % 40));
        p.end();
        m_sourceImage = img;
        ++m_frameSeq;
        pushSourceToView();
    });
}

// 先释放对外控件，避免与 Qt 子控件析构顺序冲突
MainWindow::~MainWindow()
{
    if (m_api) {
        delete m_api;
        m_api = nullptr;
    }
    m_view = nullptr;
}

// 浅色壳样式
void MainWindow::applyShellStyle()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #F5F7FA; color: #333333; }"
        "QWidget { font-family: 'Microsoft YaHei'; font-size: 12px; color: #333333; }"
        "QMenuBar { background: #FFFFFF; border-bottom: 1px solid #D9D9D9; }"
        "QGroupBox { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; margin-top: 12px; padding: 10px 8px 8px 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #333333; font-weight: bold; }"
        "QToolButton { border: 1px solid transparent; border-radius: 4px; padding: 4px 2px; background: transparent; color: #333333; }"
        "QToolButton:hover { background: #E6F7FF; }"
        "QToolButton:checked { background: #E6F7FF; border: 1px solid #1890FF; color: #1890FF; }"
        "QLineEdit { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 2px; padding: 3px 6px; }"
        "QLineEdit:read-only { background: #F5F7FA; color: #888888; }"
        "QPlainTextEdit { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 2px; padding: 4px; color: #333333; }"
        "QPlainTextEdit:read-only { background: #F5F7FA; color: #555555; }"
        "QSpinBox, QDoubleSpinBox, QComboBox { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 2px; padding: 2px; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 6px 16px; }"
        "QPushButton:hover { border-color: #1890FF; color: #1890FF; }"
        "QPushButton#primaryBtn { background: #1890FF; color: #FFFFFF; border: none; }"
        "QPushButton#primaryBtn:hover { background: #40A9FF; }"
        "QCheckBox, QRadioButton { spacing: 6px; }"
        "QLabel#caption { color: #888888; }"
        "QSlider::groove:horizontal { height: 6px; background: #E8E8E8; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #1890FF; border-radius: 7px; }"
        "QSlider::sub-page:horizontal { background: #91D5FF; border-radius: 3px; }"));
}

// 左侧工具条按钮
QToolButton* MainWindow::addToolButton(QWidget* parent, const QIcon& icon, const QString& text, bool checkable)
{
    QToolButton* b = new QToolButton(parent);
    b->setIcon(icon);
    b->setIconSize(QSize(28, 28));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setCheckable(checkable);
    b->setAutoRaise(true);
    b->setFixedSize(68, 56);
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return b;
}

// 组装菜单与三栏布局
void MainWindow::buildUi()
{
    QMenu* fileMenu = menuBar()->addMenu(u8("文件"));
    QAction* actOpen = fileMenu->addAction(u8("打开图像..."));
    QAction* actFolder = fileMenu->addAction(u8("打开文件夹..."));
    fileMenu->addSeparator();
    QAction* actSnapshot = fileMenu->addAction(u8("保存快照..."));
    QAction* actExport = fileMenu->addAction(u8("导出原图..."));
    fileMenu->addSeparator();
    QAction* actOverlay = fileMenu->addAction(u8("模拟叠加"));
    QAction* actLive = fileMenu->addAction(QStringLiteral("Live"));
    actLive->setCheckable(true);
    connect(actOpen, &QAction::triggered, this, &MainWindow::openImage);
    connect(actFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(actSnapshot, &QAction::triggered, this, &MainWindow::saveSnapshot);
    connect(actExport, &QAction::triggered, this, &MainWindow::exportOriginal);
    connect(actOverlay, &QAction::triggered, this, &MainWindow::mockOverlay);
    connect(actLive, &QAction::toggled, this, &MainWindow::toggleLive);

    QWidget* root = new QWidget;
    QVBoxLayout* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    QWidget* header = new QWidget;
    header->setFixedHeight(32);
    header->setStyleSheet(QStringLiteral("background:#FFFFFF; border-bottom:1px solid #D9D9D9;"));
    QHBoxLayout* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 12, 0);
    m_toolCaption = new QLabel(u8("当前工具: 浏览"));
    m_toolCaption->setObjectName(QStringLiteral("caption"));
    hl->addWidget(m_toolCaption);
    hl->addStretch();

    QWidget* body = new QWidget;
    QHBoxLayout* bodyLay = new QHBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);

    QWidget* left = new QWidget;
    left->setFixedWidth(156);
    left->setStyleSheet(QStringLiteral("background:#FFFFFF; border-right:1px solid #D9D9D9;"));
    QVBoxLayout* leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(6, 8, 6, 8);
    leftLay->setSpacing(6);

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);

    struct ToolDef {
        DrawTool tool;
        QIcon icon;
        const char* text;
    };
    const ToolDef tools[] = {
        { ToolBrowse,  iconBrowse(),  "浏览" },
        { ToolCircle,  iconCircle(),  "圆" },
        { ToolRect1,   iconRect1(),   "Rect1" },
        { ToolRect2,   iconRect2(),   "Rect2" },
        { ToolEllipse, iconEllipse(), "椭圆" },
        { ToolLine,    iconLine(),    "直线" },
        { ToolCross,   iconCross(),   "十字" },
        { ToolPolygon, iconRegion(),  "Region" },
        { ToolContour, iconXld(),     "XLD" },
        { ToolText,    iconText(),    "文本" },
        { ToolMeasureDist, iconLine(),    "距离" },
        { ToolMeasureAngle, iconLine(),   "夹角" },
        { ToolMeasureRadius, iconCircle(), "半径" },
        { ToolMeasureRect, iconRect1(),   "矩形" },
        { ToolMeasureEllipse, iconEllipse(), "椭圆" },
    };
    const int nTools = static_cast<int>(sizeof(tools) / sizeof(tools[0]));
    QGridLayout* toolGrid = new QGridLayout;
    toolGrid->setContentsMargins(0, 0, 0, 0);
    toolGrid->setHorizontalSpacing(4);
    toolGrid->setVerticalSpacing(4);
    for (int i = 0; i < nTools; ++i) {
        QToolButton* b = addToolButton(left, tools[i].icon, u8(tools[i].text), true);
        m_toolGroup->addButton(b, static_cast<int>(tools[i].tool));
        toolGrid->addWidget(b, i / 2, i % 2, Qt::AlignCenter);
    }
    leftLay->addLayout(toolGrid);
    m_toolGroup->button(static_cast<int>(ToolBrowse))->setChecked(true);
    connect(m_toolGroup, SIGNAL(buttonClicked(int)), this, SLOT(onToolId(int)));

    QFrame* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#D9D9D9;"));
    leftLay->addWidget(sep);

    QToolButton* fitBtn = addToolButton(left, iconFit(), u8("适应窗口"), false);
    QToolButton* oneBtn = addToolButton(left, iconOneToOne(), QStringLiteral("1:1"), false);
    connect(fitBtn, &QToolButton::clicked, this, &MainWindow::fitView);
    connect(oneBtn, &QToolButton::clicked, this, &MainWindow::zoom1to1);
    QHBoxLayout* zoomRow = new QHBoxLayout;
    zoomRow->setContentsMargins(0, 0, 0, 0);
    zoomRow->setSpacing(4);
    zoomRow->addWidget(fitBtn);
    zoomRow->addWidget(oneBtn);
    leftLay->addLayout(zoomRow);
    leftLay->addStretch();

    QToolButton* clearLeft = addToolButton(left, iconClear(), u8("清空叠加"), false);
    connect(clearLeft, &QToolButton::clicked, this, &MainWindow::clearOverlay);
    leftLay->addWidget(clearLeft, 0, Qt::AlignHCenter);

    m_api = new image_widget::ImageWidgetApi;
    m_api->setWindowMode(WindowDesign);
    m_api->setHudVisible(true);
    m_api->setOverlayToolbarEnabled(true);
    m_api->setImageInfoOverlayVisible(false);
    m_api->setLayoutFillEnabled(true);
    m_view = m_api->imageWindow();
    connect(m_view, &HsCtrl::HsImageWindow::drawToolChanged, this, [this](int tool) {
        if (!m_toolGroup) {
            return;
        }
        m_toolGroup->setExclusive(false);
        if (QAbstractButton* cur = m_toolGroup->checkedButton()) {
            cur->setChecked(false);
        }
        m_toolGroup->setExclusive(true);
        if (QAbstractButton* b = m_toolGroup->button(tool)) {
            b->setChecked(true);
        }
        updateToolCaption();
    });
    m_api->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget* right = new QWidget;
    right->setFixedWidth(560);
    right->setStyleSheet(QStringLiteral("background:#F5F7FA; border-left:1px solid #D9D9D9;"));
    QVBoxLayout* rl = new QVBoxLayout(right);
    rl->setContentsMargins(8, 8, 8, 8);
    rl->setSpacing(8);

    QWidget* cols = new QWidget;
    QHBoxLayout* colLay = new QHBoxLayout(cols);
    colLay->setContentsMargins(0, 0, 0, 0);
    colLay->setSpacing(8);
    QWidget* colL = new QWidget;
    QWidget* colR = new QWidget;
    QVBoxLayout* ll = new QVBoxLayout(colL);
    QVBoxLayout* rr = new QVBoxLayout(colR);
    ll->setContentsMargins(0, 0, 0, 0);
    rr->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);
    rr->setSpacing(8);
    colLay->addWidget(colL, 1);
    colLay->addWidget(colR, 1);

    QGroupBox* imageBox = new QGroupBox(u8("加载图像"));
    QVBoxLayout* imageLay = new QVBoxLayout(imageBox);
    QHBoxLayout* imageBtnRow = new QHBoxLayout;
    QPushButton* loadFileBtn = new QPushButton(u8("加载图像"));
    QPushButton* loadFolderBtn = new QPushButton(u8("加载文件夹"));
    imageBtnRow->addWidget(loadFileBtn);
    imageBtnRow->addWidget(loadFolderBtn);
    imageLay->addLayout(imageBtnRow);
    QHBoxLayout* navRow = new QHBoxLayout;
    m_prevImageBtn = new QPushButton(u8("上一张"));
    m_nextImageBtn = new QPushButton(u8("下一张"));
    navRow->addWidget(m_prevImageBtn);
    navRow->addWidget(m_nextImageBtn);
    imageLay->addLayout(navRow);
    m_imagePathLabel = new QLabel(u8("内置演示图"));
    m_imagePathLabel->setWordWrap(true);
    m_imagePathLabel->setStyleSheet(QStringLiteral("color:#888888;"));
    imageLay->addWidget(m_imagePathLabel);
    connect(loadFileBtn, &QPushButton::clicked, this, &MainWindow::openImage);
    connect(loadFolderBtn, &QPushButton::clicked, this, &MainWindow::openFolder);
    connect(m_prevImageBtn, &QPushButton::clicked, this, &MainWindow::showPrevImage);
    connect(m_nextImageBtn, &QPushButton::clicked, this, &MainWindow::showNextImage);

    QGroupBox* refreshBox = new QGroupBox(u8("刷新策略"));
    QVBoxLayout* refreshLay = new QVBoxLayout(refreshBox);
    QLabel* refreshHint = new QLabel(u8("勾选后换底图时清掉对应层。T 工具 / 放置的文字只受「绘制文字」控制，不受「交互 ROI」影响。"));
    refreshHint->setWordWrap(true);
    refreshHint->setStyleSheet(QStringLiteral("color:#888888;"));
    refreshLay->addWidget(refreshHint);
    m_refreshStack = new QCheckBox(u8("刷新结果 Stack"));
    m_refreshDrawing = new QCheckBox(u8("刷新交互 ROI"));
    m_refreshText = new QCheckBox(u8("刷新绘制文字"));
    m_refreshDrawing->setToolTip(u8("圆/矩形/椭圆/Region/XLD。不含文字"));
    m_refreshText->setToolTip(u8("含 T 工具、放置按钮和 drawInfo"));
    refreshLay->addWidget(m_refreshStack);
    refreshLay->addWidget(m_refreshDrawing);
    refreshLay->addWidget(m_refreshText);
    connect(m_refreshStack, &QCheckBox::toggled, this, &MainWindow::applyRefreshPolicyFromUi);
    connect(m_refreshDrawing, &QCheckBox::toggled, this, &MainWindow::applyRefreshPolicyFromUi);
    connect(m_refreshText, &QCheckBox::toggled, this, &MainWindow::applyRefreshPolicyFromUi);

    QGroupBox* barBox = new QGroupBox(u8("工具栏"));
    QVBoxLayout* barLay = new QVBoxLayout(barBox);
    m_toolbarVisible = new QCheckBox(u8("显示浮动工具栏"));
    m_toolbarVisible->setChecked(true);
    barLay->addWidget(m_toolbarVisible);
    QHBoxLayout* measRow = new QHBoxLayout;
    m_measDist = new QCheckBox(QString::fromUtf8("距离"));
    m_measAngle = new QCheckBox(QString::fromUtf8("夹角"));
    m_measRadius = new QCheckBox(QString::fromUtf8("圆"));
    m_measRect = new QCheckBox(QString::fromUtf8("矩形"));
    m_measEllipse = new QCheckBox(QString::fromUtf8("椭圆"));
    m_measDist->setChecked(true);
    m_measAngle->setChecked(true);
    m_measRadius->setChecked(true);
    m_measRect->setChecked(true);
    m_measEllipse->setChecked(true);
    measRow->addWidget(m_measDist);
    measRow->addWidget(m_measAngle);
    measRow->addWidget(m_measRadius);
    measRow->addWidget(m_measRect);
    measRow->addWidget(m_measEllipse);
    barLay->addLayout(measRow);
    m_calibPreset = new QComboBox;
    m_calibPreset->addItem(QString::fromUtf8("1:1 px"));
    m_calibPreset->addItem(QString::fromUtf8("0.012 mm"));
    m_calibPreset->addItem(QString::fromUtf8("mm + μm"));
    m_calibPreset->addItem(QString::fromUtf8("pulse"));
    barLay->addWidget(m_calibPreset);
    connect(m_measDist, &QCheckBox::toggled, this, [this](bool) { applyShownMeasureTools(); });
    connect(m_measAngle, &QCheckBox::toggled, this, [this](bool) { applyShownMeasureTools(); });
    connect(m_measRadius, &QCheckBox::toggled, this, [this](bool) { applyShownMeasureTools(); });
    connect(m_measRect, &QCheckBox::toggled, this, [this](bool) { applyShownMeasureTools(); });
    connect(m_measEllipse, &QCheckBox::toggled, this, [this](bool) { applyShownMeasureTools(); });
    connect(m_calibPreset, SIGNAL(currentIndexChanged(int)), this, SLOT(onCalibPreset(int)));
    QHBoxLayout* scaleRow = new QHBoxLayout;
    m_calibUnit = new QComboBox;
    m_calibUnit->addItem(QString::fromUtf8("px"));
    m_calibUnit->addItem(QString::fromUtf8("mm"));
    m_calibUnit->addItem(QString::fromUtf8("μm"));
    m_calibUnit->addItem(QString::fromUtf8("pulse"));
    m_calibX = new QDoubleSpinBox;
    m_calibY = new QDoubleSpinBox;
    m_calibX->setRange(0.000001, 1000000.0);
    m_calibY->setRange(0.000001, 1000000.0);
    m_calibX->setDecimals(6);
    m_calibY->setDecimals(6);
    m_calibX->setValue(1.0);
    m_calibY->setValue(1.0);
    m_calibX->setToolTip(QString::fromUtf8("X / px"));
    m_calibY->setToolTip(QString::fromUtf8("Y / px"));
    scaleRow->addWidget(m_calibUnit);
    scaleRow->addWidget(m_calibX, 1);
    scaleRow->addWidget(m_calibY, 1);
    barLay->addLayout(scaleRow);
    QHBoxLayout* showRow = new QHBoxLayout;
    m_showPx = new QCheckBox(QString::fromUtf8("px"));
    m_showMm = new QCheckBox(QString::fromUtf8("mm"));
    m_showUm = new QCheckBox(QString::fromUtf8("μm"));
    m_showPulse = new QCheckBox(QString::fromUtf8("pulse"));
    m_showPx->setChecked(true);
    showRow->addWidget(m_showPx);
    showRow->addWidget(m_showMm);
    showRow->addWidget(m_showUm);
    showRow->addWidget(m_showPulse);
    barLay->addLayout(showRow);
    QPushButton* calibBtn = new QPushButton(QString::fromUtf8("写入标定"));
    barLay->addWidget(calibBtn);
    m_measureLog = new QPlainTextEdit;
    m_measureLog->setReadOnly(true);
    m_measureLog->setFixedHeight(96);
    m_measureLog->setPlaceholderText(QString::fromUtf8("测量结果"));
    barLay->addWidget(m_measureLog);
    connect(calibBtn, &QPushButton::clicked, this, [this]() { applyCalibFromUi(); });
    connect(m_calibUnit, SIGNAL(currentIndexChanged(int)), this, SLOT(applyCalibFromUi()));
    connect(m_calibX, SIGNAL(valueChanged(double)), this, SLOT(applyCalibFromUi()));
    connect(m_calibY, SIGNAL(valueChanged(double)), this, SLOT(applyCalibFromUi()));
    connect(m_showPx, &QCheckBox::toggled, this, [this](bool) { applyCalibFromUi(); });
    connect(m_showMm, &QCheckBox::toggled, this, [this](bool) { applyCalibFromUi(); });
    connect(m_showUm, &QCheckBox::toggled, this, [this](bool) { applyCalibFromUi(); });
    connect(m_showPulse, &QCheckBox::toggled, this, [this](bool) { applyCalibFromUi(); });
    QHBoxLayout* cornerRow = new QHBoxLayout;
    cornerRow->addWidget(new QLabel(u8("位置")));
    m_toolbarCorner = new QComboBox;
    m_toolbarCorner->addItem(u8("左上"), static_cast<int>(Qt::TopLeftCorner));
    m_toolbarCorner->addItem(u8("右上"), static_cast<int>(Qt::TopRightCorner));
    m_toolbarCorner->addItem(u8("左下"), static_cast<int>(Qt::BottomLeftCorner));
    m_toolbarCorner->addItem(u8("右下"), static_cast<int>(Qt::BottomRightCorner));
    m_toolbarCorner->setCurrentIndex(1);
    cornerRow->addWidget(m_toolbarCorner, 1);
    barLay->addLayout(cornerRow);
    QHBoxLayout* orientRow = new QHBoxLayout;
    orientRow->addWidget(new QLabel(u8("排列")));
    m_toolbarVertical = new QRadioButton(u8("垂直"));
    m_toolbarHorizontal = new QRadioButton(u8("水平"));
    m_toolbarVertical->setChecked(true);
    QButtonGroup* orientGroup = new QButtonGroup(this);
    orientGroup->addButton(m_toolbarVertical);
    orientGroup->addButton(m_toolbarHorizontal);
    orientRow->addWidget(m_toolbarVertical);
    orientRow->addWidget(m_toolbarHorizontal);
    orientRow->addStretch();
    barLay->addLayout(orientRow);
    connect(m_toolbarVisible, &QCheckBox::toggled, this, &MainWindow::onToolbarVisible);
    connect(m_toolbarCorner, SIGNAL(currentIndexChanged(int)), this, SLOT(onToolbarCorner(int)));
    connect(m_toolbarVertical, &QRadioButton::toggled, this, &MainWindow::onToolbarOrientation);

    QHBoxLayout* skinRow = new QHBoxLayout;
    skinRow->addWidget(new QLabel(u8("皮肤")));
    m_toolbarSkin = new QComboBox;
    m_toolbarSkin->addItem(u8("深色半透明"), static_cast<int>(ToolbarSkinDark));
    m_toolbarSkin->addItem(u8("浅色半透明"), static_cast<int>(ToolbarSkinLight));
    m_toolbarSkin->addItem(u8("透明"), static_cast<int>(ToolbarSkinTransparent));
    m_toolbarSkin->addItem(u8("深色不透明"), static_cast<int>(ToolbarSkinDarkSolid));
    m_toolbarSkin->addItem(u8("浅色不透明"), static_cast<int>(ToolbarSkinLightSolid));
    m_toolbarSkin->setCurrentIndex(0);
    skinRow->addWidget(m_toolbarSkin, 1);
    barLay->addLayout(skinRow);
    connect(m_toolbarSkin, SIGNAL(currentIndexChanged(int)), this, SLOT(onToolbarSkin(int)));

    QGroupBox* vpBox = new QGroupBox(u8("视口背景"));
    QVBoxLayout* vpLay = new QVBoxLayout(vpBox);
    QLabel* vpHint = new QLabel(u8("letterbox / 无图区域。棋盘格按屏幕像素画，不随缩放变格。"));
    vpHint->setWordWrap(true);
    vpHint->setStyleSheet(QStringLiteral("color:#888888;"));
    vpLay->addWidget(vpHint);
    m_viewportBg = new QComboBox;
    m_viewportBg->addItem(u8("深色"), static_cast<int>(ViewportDark));
    m_viewportBg->addItem(u8("中性灰"), static_cast<int>(ViewportMid));
    m_viewportBg->addItem(u8("浅色"), static_cast<int>(ViewportLight));
    m_viewportBg->addItem(u8("浅色棋盘格"), static_cast<int>(ViewportCheckerboard));
    m_viewportBg->addItem(u8("深色棋盘格"), static_cast<int>(ViewportCheckerboardDark));
    m_viewportBg->addItem(u8("自定义纯色"), static_cast<int>(ViewportCustom));
    m_viewportBg->setCurrentIndex(0);
    vpLay->addWidget(m_viewportBg);
    QHBoxLayout* vpColorRow = new QHBoxLayout;
    vpColorRow->addWidget(new QLabel(u8("自定义色")));
    m_viewportColorBtn = new QToolButton;
    m_viewportColorBtn->setText(QStringLiteral("..."));
    m_viewportColorBtn->setFixedSize(28, 22);
    m_viewportColorBtn->setToolTip(u8("纯色填充，或作为棋盘格浅格颜色"));
    vpColorRow->addWidget(m_viewportColorBtn);
    vpColorRow->addStretch();
    vpLay->addLayout(vpColorRow);
    connect(m_viewportBg, SIGNAL(currentIndexChanged(int)), this, SLOT(onViewportBackground(int)));
    connect(m_viewportColorBtn, &QToolButton::clicked, this, &MainWindow::onViewportColor);

    QGroupBox* brush = new QGroupBox(u8("画笔"));
    QVBoxLayout* bl = new QVBoxLayout(brush);
    QHBoxLayout* widthRow = new QHBoxLayout;
    widthRow->addWidget(new QLabel(u8("线宽")));
    m_widthSpin = new QSpinBox;
    m_widthSpin->setRange(1, 20);
    m_widthSpin->setValue(2);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    widthRow->addWidget(m_widthSpin, 1);
    bl->addLayout(widthRow);
    QHBoxLayout* lineRow = new QHBoxLayout;
    lineRow->addWidget(new QLabel(u8("线型")));
    m_lineCombo = new QComboBox;
    m_lineCombo->addItem(u8("实线"), static_cast<int>(HsCommData::LineSolid));
    m_lineCombo->addItem(u8("虚线"), static_cast<int>(HsCommData::LineDash));
    m_lineCombo->addItem(u8("点线"), static_cast<int>(HsCommData::LineDot));
    m_lineCombo->addItem(u8("点划线"), static_cast<int>(HsCommData::LineDashDot));
    lineRow->addWidget(m_lineCombo, 1);
    bl->addLayout(lineRow);
    connect(m_lineCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onWidthChanged(int)));

    QHBoxLayout* colorRow = new QHBoxLayout;
    colorRow->addWidget(new QLabel(u8("颜色")));
    m_colorGroup = new QButtonGroup(this);
    m_colorGroup->setExclusive(true);
    const QColor presets[4] = {
        QColor(0xFA, 0xAD, 0x14),
        QColor(0x52, 0xC4, 0x1A),
        QColor(0xFF, 0x4D, 0x4F),
        QColor(0xFF, 0xFF, 0xFF)
    };
    for (int i = 0; i < 4; ++i) {
        QToolButton* sw = new QToolButton;
        sw->setCheckable(true);
        sw->setFixedSize(22, 22);
        sw->setStyleSheet(QStringLiteral("QToolButton { background:%1; border:1px solid #D9D9D9; border-radius:2px; }"
                                         "QToolButton:checked { border:2px solid #1890FF; }")
                              .arg(presets[i].name()));
        m_colorBtns[i] = sw;
        m_colorGroup->addButton(sw, i);
        colorRow->addWidget(sw);
    }
    m_colorBtns[0]->setChecked(true);
    QToolButton* pick = new QToolButton;
    pick->setText(QStringLiteral("..."));
    pick->setFixedSize(22, 22);
    pick->setToolTip(u8("自定义颜色"));
    colorRow->addWidget(pick);
    colorRow->addStretch();
    bl->addLayout(colorRow);
    connect(m_colorGroup, SIGNAL(buttonClicked(int)), this, SLOT(onColorId(int)));
    connect(pick, &QToolButton::clicked, this, &MainWindow::onPickColor);

    QHBoxLayout* modeRow = new QHBoxLayout;
    modeRow->addWidget(new QLabel(u8("绘制模式")));
    m_strokeRadio = new QRadioButton(u8("描边"));
    m_fillRadio = new QRadioButton(u8("填充"));
    m_strokeRadio->setChecked(true);
    modeRow->addWidget(m_strokeRadio);
    modeRow->addWidget(m_fillRadio);
    modeRow->addStretch();
    bl->addLayout(modeRow);
    connect(m_widthSpin, SIGNAL(valueChanged(int)), this, SLOT(onWidthChanged(int)));
    connect(m_strokeRadio, &QRadioButton::toggled, this, &MainWindow::onDrawModeChanged);

    QGroupBox* roiBox = new QGroupBox(u8("当前ROI"));
    QVBoxLayout* roiLay = new QVBoxLayout(roiBox);
    QHBoxLayout* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(u8("类型")));
    m_typeEdit = new QLineEdit(QStringLiteral("--"));
    m_typeEdit->setReadOnly(true);
    typeRow->addWidget(m_typeEdit, 1);
    QHBoxLayout* roleRow = new QHBoxLayout;
    roleRow->addWidget(new QLabel(u8("Role")));
    m_roleEdit = new QLineEdit(QStringLiteral("--"));
    m_roleEdit->setReadOnly(true);
    roleRow->addWidget(m_roleEdit, 1);
    roiLay->addLayout(typeRow);
    roiLay->addLayout(roleRow);
    m_roiGeom = new QPlainTextEdit;
    m_roiGeom->setReadOnly(true);
    m_roiGeom->setMaximumHeight(118);
    m_roiGeom->setMinimumHeight(88);
    m_roiGeom->setPlaceholderText(u8("选中或绘制 ROI 后显示几何参数"));
    roiLay->addWidget(m_roiGeom);

    QHBoxLayout* textRow = new QHBoxLayout;
    textRow->addWidget(new QLabel(u8("文字")));
    m_textEdit = new QLineEdit(u8("Text"));
    m_textEdit->setPlaceholderText(u8("要显示的文字"));
    textRow->addWidget(m_textEdit, 1);
    roiLay->addLayout(textRow);

    QHBoxLayout* textSpaceRow = new QHBoxLayout;
    textSpaceRow->addWidget(new QLabel(u8("坐标")));
    m_textImageRadio = new QRadioButton(u8("图像"));
    m_textWindowRadio = new QRadioButton(u8("窗口"));
    m_textImageRadio->setChecked(true);
    QButtonGroup* textSpaceGroup = new QButtonGroup(this);
    textSpaceGroup->addButton(m_textImageRadio);
    textSpaceGroup->addButton(m_textWindowRadio);
    textSpaceRow->addWidget(m_textImageRadio);
    textSpaceRow->addWidget(m_textWindowRadio);
    textSpaceRow->addStretch();
    roiLay->addLayout(textSpaceRow);

    QHBoxLayout* textPosRow = new QHBoxLayout;
    textPosRow->addWidget(new QLabel(QStringLiteral("X")));
    m_textXSpin = new QDoubleSpinBox;
    m_textXSpin->setRange(-100000.0, 100000.0);
    m_textXSpin->setDecimals(2);
    m_textXSpin->setKeyboardTracking(false);
    textPosRow->addWidget(m_textXSpin, 1);
    textPosRow->addWidget(new QLabel(QStringLiteral("Y")));
    m_textYSpin = new QDoubleSpinBox;
    m_textYSpin->setRange(-100000.0, 100000.0);
    m_textYSpin->setDecimals(2);
    m_textYSpin->setKeyboardTracking(false);
    textPosRow->addWidget(m_textYSpin, 1);
    roiLay->addLayout(textPosRow);

    QHBoxLayout* textFontRow = new QHBoxLayout;
    textFontRow->addWidget(new QLabel(u8("字号")));
    m_textFontSpin = new QSpinBox;
    m_textFontSpin->setRange(8, 72);
    m_textFontSpin->setValue(14);
    m_textFontSpin->setSuffix(QStringLiteral(" px"));
    textFontRow->addWidget(m_textFontSpin);
    QPushButton* placeTextBtn = new QPushButton(u8("放置"));
    textFontRow->addWidget(placeTextBtn, 1);
    roiLay->addLayout(textFontRow);

    connect(m_textEdit, &QLineEdit::textChanged, this, &MainWindow::syncTextToolToView);
    connect(m_textImageRadio, &QRadioButton::toggled, this, &MainWindow::syncTextToolToView);
    connect(m_textFontSpin, SIGNAL(valueChanged(int)), this, SLOT(syncTextToolToView()));
    connect(placeTextBtn, &QPushButton::clicked, this, &MainWindow::onPlaceText);

    QGroupBox* layerBox = new QGroupBox(u8("图层"));
    QVBoxLayout* layerLay = new QVBoxLayout(layerBox);
    m_layerImage = new QCheckBox(u8("图像"));
    m_layerStack = new QCheckBox(u8("结果 Stack"));
    m_layerDrawing = new QCheckBox(u8("ROI Drawing"));
    m_layerHud = new QCheckBox(u8("HUD"));
    m_pixelInspect = new QCheckBox(u8("像素检验"));
    m_layerImage->setChecked(true);
    m_layerStack->setChecked(true);
    m_layerDrawing->setChecked(true);
    m_layerHud->setChecked(true);
    m_pixelInspect->setChecked(true);
    layerLay->addWidget(m_layerImage);
    layerLay->addWidget(m_layerStack);
    layerLay->addWidget(m_layerDrawing);
    layerLay->addWidget(m_layerHud);
    layerLay->addWidget(m_pixelInspect);
    connect(m_layerImage, &QCheckBox::toggled, this, &MainWindow::onLayerImage);
    connect(m_layerStack, &QCheckBox::toggled, this, &MainWindow::onLayerStack);
    connect(m_layerDrawing, &QCheckBox::toggled, this, &MainWindow::onLayerDrawing);
    connect(m_layerHud, &QCheckBox::toggled, this, &MainWindow::onLayerHud);
    connect(m_pixelInspect, &QCheckBox::toggled, this, &MainWindow::onPixelInspect);
    m_view->setPixelInspect(true);

    QGroupBox* algoBox = new QGroupBox(u8("算法测试"));
    QVBoxLayout* algoLay = new QVBoxLayout(algoBox);
    algoLay->setSpacing(6);

    QHBoxLayout* algoBtnRow = new QHBoxLayout;
    QPushButton* algoLoad = new QPushButton(u8("加载图像"));
    QPushButton* algoUse = new QPushButton(u8("当前图像"));
    algoBtnRow->addWidget(algoLoad);
    algoBtnRow->addWidget(algoUse);
    algoLay->addLayout(algoBtnRow);

    QHBoxLayout* thRow = new QHBoxLayout;
    thRow->addWidget(new QLabel(u8("阈值")));
    m_thSlider = new QSlider(Qt::Horizontal);
    m_thSlider->setRange(0, 255);
    m_thSlider->setValue(128);
    m_thSpin = new QSpinBox;
    m_thSpin->setRange(0, 255);
    m_thSpin->setValue(128);
    m_thSpin->setFixedWidth(56);
    thRow->addWidget(m_thSlider, 1);
    thRow->addWidget(m_thSpin);
    algoLay->addLayout(thRow);

    QHBoxLayout* algoColorRow = new QHBoxLayout;
    algoColorRow->addWidget(new QLabel(u8("颜色")));
    m_algoColorGroup = new QButtonGroup(this);
    m_algoColorGroup->setExclusive(true);
    const QColor algoPresets[4] = {
        QColor(0x52, 0xC4, 0x1A),
        QColor(0xFA, 0xAD, 0x14),
        QColor(0xFF, 0x4D, 0x4F),
        QColor(0x18, 0x90, 0xFF)
    };
    for (int i = 0; i < 4; ++i) {
        QToolButton* sw = new QToolButton;
        sw->setCheckable(true);
        sw->setFixedSize(22, 22);
        styleAlgoColorButton(sw, algoPresets[i], true);
        m_algoColorBtns[i] = sw;
        m_algoColorGroup->addButton(sw, i);
        algoColorRow->addWidget(sw);
    }
    m_algoColorBtns[0]->setChecked(true);
    m_algoPickBtn = new QToolButton;
    m_algoPickBtn->setText(QStringLiteral("..."));
    m_algoPickBtn->setFixedSize(22, 22);
    m_algoPickBtn->setToolTip(u8("自定义颜色"));
    algoColorRow->addWidget(m_algoPickBtn);
    algoColorRow->addStretch();
    algoLay->addLayout(algoColorRow);

    QHBoxLayout* algoOpRow = new QHBoxLayout;
    algoOpRow->addWidget(new QLabel(u8("透明度")));
    m_algoOpacitySlider = new QSlider(Qt::Horizontal);
    m_algoOpacitySlider->setRange(0, 100);
    m_algoOpacitySlider->setValue(35);
    m_algoOpacitySpin = new QSpinBox;
    m_algoOpacitySpin->setRange(0, 100);
    m_algoOpacitySpin->setValue(35);
    m_algoOpacitySpin->setSuffix(QStringLiteral(" %"));
    m_algoOpacitySpin->setFixedWidth(62);
    algoOpRow->addWidget(m_algoOpacitySlider, 1);
    algoOpRow->addWidget(m_algoOpacitySpin);
    algoLay->addLayout(algoOpRow);

    m_algoLive = new QCheckBox(u8("滑杆实时提取"));
    m_algoLive->setChecked(true);
    m_algoStress = new QCheckBox(u8("连刷测刷新频率"));
    algoLay->addWidget(m_algoLive);
    algoLay->addWidget(m_algoStress);

    m_algoStats = new QLabel;
    m_algoStats->setWordWrap(true);
    m_algoStats->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_algoStats->setMinimumHeight(96);
    m_algoStats->setStyleSheet(QStringLiteral("color:#555555; font-size:11px;"));
    m_algoStats->setText(u8("引擎: ") + halconEngineName() + u8("\n加载图像后拖动阈值滑杆。"));
    algoLay->addWidget(m_algoStats, 1);

    connect(algoLoad, &QPushButton::clicked, this, &MainWindow::loadAlgoImage);
    connect(algoUse, &QPushButton::clicked, this, &MainWindow::useCurrentImage);
    connect(m_thSlider, SIGNAL(valueChanged(int)), this, SLOT(onThresholdChanged(int)));
    connect(m_thSpin, SIGNAL(valueChanged(int)), this, SLOT(onThresholdChanged(int)));
    connect(m_algoColorGroup, SIGNAL(buttonClicked(int)), this, SLOT(onAlgoColorId(int)));
    connect(m_algoPickBtn, &QToolButton::clicked, this, &MainWindow::onAlgoPickColor);
    connect(m_algoOpacitySlider, SIGNAL(valueChanged(int)), this, SLOT(onAlgoOpacityChanged(int)));
    connect(m_algoOpacitySpin, SIGNAL(valueChanged(int)), this, SLOT(onAlgoOpacityChanged(int)));
    connect(m_algoStress, &QCheckBox::toggled, this, &MainWindow::onAlgoStress);

    QHBoxLayout* btnRow = new QHBoxLayout;
    QPushButton* clearDraw = new QPushButton(u8("清空"));
    QPushButton* clearOv = new QPushButton(u8("清空叠加"));
    QPushButton* ok = new QPushButton(u8("确定"));
    ok->setObjectName(QStringLiteral("primaryBtn"));
    ok->setMinimumWidth(88);
    btnRow->addWidget(clearDraw);
    btnRow->addWidget(clearOv);
    btnRow->addStretch();
    btnRow->addWidget(ok);
    connect(clearDraw, &QPushButton::clicked, this, &MainWindow::clearDrawings);
    connect(clearOv, &QPushButton::clicked, this, &MainWindow::clearOverlay);
    connect(ok, &QPushButton::clicked, this, &MainWindow::onConfirm);

    ll->addWidget(imageBox);

    QGroupBox* sessionBox = new QGroupBox(u8("绘制会话"));
    QVBoxLayout* sessionLay = new QVBoxLayout(sessionBox);
    QLabel* sessionHint = new QLabel(u8("一次一条。形状松开即提交。多边形右键或 Enter 结束。涂抹：左键涂、右键擦，Enter 或「确定」提交。Esc 取消。Ctrl+Z 撤销一笔，Delete 清空。双击不再适应窗口。"));
    sessionHint->setWordWrap(true);
    sessionHint->setStyleSheet(QStringLiteral("color:#888888;"));
    sessionLay->addWidget(sessionHint);
    QGridLayout* sessionGrid = new QGridLayout;
    QPushButton* sessionCircle = new QPushButton(u8("模态圆"));
    QPushButton* sessionRect = new QPushButton(u8("模态矩形"));
    QPushButton* sessionPoly = new QPushButton(u8("模态区域"));
    QPushButton* sessionBrush = new QPushButton(u8("屏蔽涂抹"));
    sessionGrid->addWidget(sessionCircle, 0, 0);
    sessionGrid->addWidget(sessionRect, 0, 1);
    sessionGrid->addWidget(sessionPoly, 1, 0);
    sessionGrid->addWidget(sessionBrush, 1, 1);
    sessionLay->addLayout(sessionGrid);
    QHBoxLayout* radiusRow = new QHBoxLayout;
    radiusRow->addWidget(new QLabel(u8("笔半径")));
    QDoubleSpinBox* radiusSpin = new QDoubleSpinBox;
    radiusSpin->setDecimals(1);
    radiusSpin->setSingleStep(0.1);
    radiusSpin->setRange(0.5, 256.0);
    radiusSpin->setValue(8.0);
    radiusSpin->setSuffix(u8(" px"));
    radiusRow->addWidget(radiusSpin, 1);
    sessionLay->addLayout(radiusRow);
    QHBoxLayout* sessionBtnRow = new QHBoxLayout;
    QPushButton* sessionOk = new QPushButton(u8("确定"));
    QPushButton* sessionCancel = new QPushButton(u8("取消"));
    sessionBtnRow->addWidget(sessionOk);
    sessionBtnRow->addWidget(sessionCancel);
    sessionLay->addLayout(sessionBtnRow);
    connect(sessionCircle, &QPushButton::clicked, this, &MainWindow::onSessionCircle);
    connect(sessionRect, &QPushButton::clicked, this, &MainWindow::onSessionRect);
    connect(sessionPoly, &QPushButton::clicked, this, &MainWindow::onSessionPolygon);
    connect(sessionBrush, &QPushButton::clicked, this, &MainWindow::onSessionBrush);
    connect(sessionOk, &QPushButton::clicked, this, &MainWindow::onSessionCommit);
    connect(sessionCancel, &QPushButton::clicked, this, &MainWindow::onSessionCancel);
    connect(radiusSpin, SIGNAL(valueChanged(double)), this, SLOT(onBrushRadius(double)));
    ll->addWidget(sessionBox);

    ll->addWidget(vpBox);
    ll->addWidget(refreshBox);
    ll->addWidget(roiBox, 1);
    rr->addWidget(barBox);
    rr->addWidget(brush);
    rr->addWidget(layerBox);
    rr->addWidget(algoBox, 1);

    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(cols);

    rl->addWidget(scroll, 1);
    rl->addLayout(btnRow);

    bodyLay->addWidget(left);
    bodyLay->addWidget(m_api->widget(), 1);
    bodyLay->addWidget(right);

    rootLay->addWidget(header);
    rootLay->addWidget(body, 1);
    setCentralWidget(root);

    connect(m_view, &HsImageWindow::roiCommitted, this, &MainWindow::onRoiCommitted);
    connect(m_view, &HsImageWindow::roiSelected, this, &MainWindow::onRoiSelected);
    connect(m_view, &HsImageWindow::drawSessionFinished, this, &MainWindow::onDrawSessionFinished);
    connect(m_view, &HsImageWindow::textAnchorPicked, this, &MainWindow::onTextAnchorPicked);
    syncTextToolToView();
    applyRefreshPolicyFromUi();
    updateFolderNav();
}

// 更新工具栏说明文字
void MainWindow::updateToolCaption()
{
    QString name = u8("浏览");
    switch (m_view->drawTool()) {
    case ToolCircle:  name = u8("圆"); break;
    case ToolRect1:   name = QStringLiteral("Rect1"); break;
    case ToolRect2:   name = QStringLiteral("Rect2"); break;
    case ToolEllipse: name = u8("椭圆"); break;
    case ToolLine:    name = u8("直线"); break;
    case ToolCross:   name = u8("十字"); break;
    case ToolPolygon: name = QStringLiteral("Region"); break;
    case ToolContour: name = QStringLiteral("XLD"); break;
    case ToolText:    name = u8("文本"); break;
    case ToolMeasureDist:   name = u8("距离"); break;
    case ToolMeasureAngle:  name = u8("夹角"); break;
    case ToolMeasureRadius: name = u8("半径"); break;
    case ToolMeasureRect:    name = u8("矩形"); break;
    case ToolMeasureEllipse: name = u8("椭圆"); break;
    case ToolBrush:   name = u8("屏蔽涂抹"); break;
    default: break;
    }
    if (m_view->drawSessionActive()) {
        name += u8("  [会话]");
    }
    m_toolCaption->setText(u8("当前工具: ") + name);
}

// 把左侧画笔写到窗口
void MainWindow::applyPenToView()
{
    HsCommData::Style s = HsCommData::Style::drawingStyle();
    s.stroke = HsCommData::Color4(static_cast<unsigned char>(m_penColor.red()),
                      static_cast<unsigned char>(m_penColor.green()),
                      static_cast<unsigned char>(m_penColor.blue()), 255);
    s.fill = HsCommData::Color4(static_cast<unsigned char>(m_penColor.red()),
                    static_cast<unsigned char>(m_penColor.green()),
                    static_cast<unsigned char>(m_penColor.blue()), 40);
    s.widthPx = m_widthSpin->value();
    s.line = static_cast<HsCommData::LineStyle>(m_lineCombo->currentData().toInt());
    s.draw = m_fillRadio->isChecked() ? HsCommData::DrawFill : HsCommData::DrawMargin;
    m_view->setDrawingStyle(s);
}

// 加载内置演示图为底图
// Demo 持有 QImage，经对外 API 交给视图
void MainWindow::pushSourceToView()
{
    if (m_api) {
        m_api->setImage(m_sourceImage);
    }
}

// 加载内置演示图为底图
void MainWindow::loadGeneratedImage()
{
    ++m_frameSeq;
    m_sourceImage = makeDemoImage();
    m_folderFiles.clear();
    m_folderIndex = -1;
    pushSourceToView();
    updateFolderNav();
}

// 打开图像文件并设为底图
void MainWindow::openImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, u8("打开图像"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (path.isEmpty()) {
        return;
    }
    if (!loadImagePath(path)) {
        return;
    }
    m_folderFiles = QStringList() << QFileInfo(path).absoluteFilePath();
    m_folderIndex = 0;
    updateFolderNav();
}

// 打开文件夹，加载第一张
void MainWindow::openFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, u8("加载文件夹"));
    if (dir.isEmpty()) {
        return;
    }
    const QStringList files = listImageFiles(dir);
    if (files.isEmpty()) {
        QMessageBox::information(this, u8("加载文件夹"), u8("该文件夹没有可识别的图像。"));
        return;
    }
    m_folderFiles = files;
    m_folderIndex = 0;
    loadImagePath(m_folderFiles.at(0));
    updateFolderNav();
}

// 文件夹上一张（循环）
void MainWindow::showPrevImage()
{
    if (m_folderFiles.size() < 2) {
        return;
    }
    m_folderIndex = (m_folderIndex + m_folderFiles.size() - 1) % m_folderFiles.size();
    loadImagePath(m_folderFiles.at(m_folderIndex));
    updateFolderNav();
}

// 文件夹下一张（循环）
void MainWindow::showNextImage()
{
    if (m_folderFiles.size() < 2) {
        return;
    }
    m_folderIndex = (m_folderIndex + 1) % m_folderFiles.size();
    loadImagePath(m_folderFiles.at(m_folderIndex));
    updateFolderNav();
}

// 解码文件为底图；换图走窗口刷新策略
bool MainWindow::loadImagePath(const QString& path)
{
    QImage img;
    if (!img.load(path)) {
        QMessageBox::warning(this, u8("加载图像"), u8("无法打开: ") + path);
        return false;
    }
    ++m_frameSeq;
    m_sourceImage = img;
    pushSourceToView();
    bindAlgoImage(m_sourceImage, false);
    return true;
}

// 刷新文件夹序号与翻页按钮
void MainWindow::updateFolderNav()
{
    const bool hasList = !m_folderFiles.isEmpty() && m_folderIndex >= 0
        && m_folderIndex < m_folderFiles.size();
    if (m_prevImageBtn) {
        m_prevImageBtn->setEnabled(m_folderFiles.size() > 1);
    }
    if (m_nextImageBtn) {
        m_nextImageBtn->setEnabled(m_folderFiles.size() > 1);
    }
    if (!m_imagePathLabel) {
        return;
    }
    if (!hasList) {
        m_imagePathLabel->setText(u8("内置演示图"));
        return;
    }
    const QFileInfo fi(m_folderFiles.at(m_folderIndex));
    m_imagePathLabel->setText(QStringLiteral("%1 / %2  %3")
                                  .arg(m_folderIndex + 1)
                                  .arg(m_folderFiles.size())
                                  .arg(fi.fileName()));
}

// 保存视口快照
void MainWindow::saveSnapshot()
{
    const QString path = QFileDialog::getSaveFileName(
        this, u8("保存快照"), QStringLiteral("snapshot.png"),
        QStringLiteral("PNG (*.png);;JPEG (*.jpg)"));
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_view->saveSnapshot(path, &err)) {
        QMessageBox::warning(this, u8("保存快照"), err);
    }
}

// 导出原图像素
void MainWindow::exportOriginal()
{
    const QString path = QFileDialog::getSaveFileName(
        this, u8("导出原图"), QStringLiteral("image.png"),
        QStringLiteral("PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)"));
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_view->saveCurrentFrame(path, &err)) {
        QMessageBox::warning(this, u8("导出原图"), err);
    }
}

// 整幅图适应窗口
void MainWindow::fitView()
{
    m_view->fitToWindow();
}

// 1:1 显示
void MainWindow::zoom1to1()
{
    m_view->zoomActualSize();
}

// 清空 Stack
void MainWindow::clearOverlay()
{
    m_view->clearWindow();
    m_view->clearDrawInfo();
}

// 清空 Drawing
void MainWindow::clearDrawings()
{
    m_view->clearDrawings();
    m_typeEdit->setText(QStringLiteral("--"));
    m_roleEdit->setText(QStringLiteral("--"));
    if (m_roiGeom) {
        m_roiGeom->clear();
    }
}

// 写入一组示例 Stack 图元（常用形状走 disp*，客户不必传 frameId）
void MainWindow::mockOverlay()
{
    if (!m_view->hasImage()) {
        return;
    }
    const QSize sz = m_view->currentImageSize();
    m_view->clearOverlays();

    m_view->setOverlayColor(QColor(0x52, 0xC4, 0x1A));
    m_view->setOverlayWidth(2.0);
    m_view->dispCircle(420, 380, 96);
    m_view->dispRegion(HsCommData::HsRegion::genRegion(HsCommData::IRect(180, 640, 280, 140)));
    m_view->dispCross(200, 180, 48, HsCommData::fromAngle(0.6));
    m_view->dispArrow(120, 260, 300, 180);
    m_view->dispLine(100, 100, 280, 160);

    m_view->setOverlayColor(QColor(0x18, 0x90, 0xFF));
    m_view->dispRect1(760, 240, 220, 160);
    HsCommData::EllipseP sampleEll;
    sampleEll.c = HsCommData::Point2d(900, 500);
    sampleEll.ra = 80;
    sampleEll.rb = 40;
    sampleEll.setAngRad(0.4);
    m_view->dispEllipse(sampleEll);

    // Keypoints / Skeleton / Badge / Mask 仍走协议整帧追加
    const qint64 fid = m_view->currentFrameId();
    std::shared_ptr<HsCommData::FrameOverlay> sc = HsCommData::FrameOverlay::create(fid);
    if (m_view->overlay()) {
        sc->items = m_view->overlay()->items;
    }

    HsCommData::Graphic kps;
    kps.id = "ann.kpts.1";
    kps.kind = HsCommData::KindKeypoints;
    kps.role = HsCommData::RoleStack;
    kps.geomStyle = HsCommData::Style::annotationStyle();
    kps.geomStyle.stroke = HsCommData::Color4(0xFF, 0x4D, 0x4F, 255);
    kps.geomStyle.widthPx = 2.0;
    kps.frameId = fid;
    kps.geom = HsCommData::HsContour();
    kps.geom.contourAt(0)->pushPt(HsCommData::Point2d(300, 300));
    kps.geom.contourAt(0)->pushPt(HsCommData::Point2d(360, 280));
    kps.geom.contourAt(0)->pushPt(HsCommData::Point2d(420, 310));
    kps.keypointVis.push_back(2);
    kps.keypointVis.push_back(1);
    kps.keypointVis.push_back(2);
    kps.updateBounds();
    sc->items.push_back(kps);

    HsCommData::Graphic skel;
    skel.id = "ann.skel.1";
    skel.kind = HsCommData::KindSkeleton;
    skel.role = HsCommData::RoleStack;
    skel.geomStyle = HsCommData::Style::annotationStyle();
    skel.geomStyle.stroke = HsCommData::Color4(0xFA, 0xAD, 0x14, 255);
    skel.geomStyle.widthPx = 2.0;
    skel.frameId = fid;
    skel.geom = HsCommData::HsContour();
    skel.geom.contourAt(0)->pushPt(HsCommData::Point2d(300, 300));
    skel.geom.contourAt(0)->pushPt(HsCommData::Point2d(360, 280));
    skel.geom.contourAt(0)->pushPt(HsCommData::Point2d(360, 280));
    skel.geom.contourAt(0)->pushPt(HsCommData::Point2d(420, 310));
    skel.edgeDashed.push_back(0);
    skel.edgeDashed.push_back(1);
    skel.updateBounds();
    sc->items.push_back(skel);

    HsCommData::Graphic badge;
    badge.id = "ann.badge.1";
    badge.kind = HsCommData::KindTextBadge;
    badge.role = HsCommData::RoleStack;
    badge.geomStyle = HsCommData::Style::annotationStyle();
    badge.geomStyle.stroke = HsCommData::Color4(0x18, 0x90, 0xFF, 255);
    badge.geomStyle.widthPx = 2.0;
    badge.frameId = fid;
    badge.text.resize(1);
    badge.text[0].anchor = HsCommData::Point2d(760, 216);
    badge.text[0].content = "person";
    badge.updateBounds();
    sc->items.push_back(badge);

    HsCommData::Graphic mask;
    mask.id = "ann.mask.1";
    mask.kind = HsCommData::KindMask;
    mask.role = HsCommData::RoleStack;
    mask.geomStyle = HsCommData::Style::annotationStyle();
    mask.geomStyle.fill = HsCommData::Color4(0x52, 0xC4, 0x1A, 140);
    mask.frameId = fid;
    mask.mask.width = sz.width();
    mask.mask.height = sz.height();
    mask.mask.pixels.assign(static_cast<size_t>(sz.width()) * static_cast<size_t>(sz.height()), 0);
    for (int y = 500; y < 620 && y < sz.height(); ++y) {
        for (int x = 980; x < 1140 && x < sz.width(); ++x) {
            const int dx = x - 1060;
            const int dy = y - 560;
            if (dx * dx + dy * dy < 55 * 55) {
                mask.mask.pixels[static_cast<size_t>(y) * static_cast<size_t>(sz.width()) + static_cast<size_t>(x)] = 180;
            }
        }
    }
    mask.updateBounds();
    sc->items.push_back(mask);

    m_view->setOverlay(sc);
    m_view->drawInfo(DrawInfoImage, u8("图像坐标"), QPointF(420, 272), 14, QColor(0x52, 0xC4, 0x1A));
    m_view->addDrawInfo(DrawInfoWindow, u8("窗口坐标"), QPointF(12, 36), 14, QColor(0xFA, 0xAD, 0x14));
}

// 模拟实时刷 Overlay
void MainWindow::toggleLive(bool on)
{
    m_view->setLive(on);
    if (on) {
        m_liveTimer->start(33);
    } else {
        m_liveTimer->stop();
    }
}

// 切换绘制工具
void MainWindow::onToolId(int id)
{
    if (m_view->drawSessionActive()) {
        m_toolGroup->setExclusive(false);
        if (QAbstractButton* b = m_toolGroup->checkedButton()) {
            b->setChecked(false);
        }
        m_toolGroup->setExclusive(true);
        updateToolCaption();
        return;
    }
    m_view->setDrawTool(static_cast<DrawTool>(id));
    updateToolCaption();
}

void MainWindow::onSessionCircle()
{
    beginSession(ToolCircle);
}

void MainWindow::onSessionRect()
{
    beginSession(ToolRect1);
}

void MainWindow::onSessionPolygon()
{
    beginSession(ToolPolygon);
}

void MainWindow::onSessionBrush()
{
    beginSession(ToolBrush);
}

void MainWindow::onSessionCommit()
{
    m_view->commitDrawSession();
}

void MainWindow::onSessionCancel()
{
    m_view->cancelDrawSession();
}

void MainWindow::onBrushRadius(double radius)
{
    m_view->setBrushRadius(radius);
}

void MainWindow::onDrawSessionFinished(const HsCommData::Graphic& g, bool accepted)
{
    Q_UNUSED(g);
    if (QAbstractButton* b = m_toolGroup->button(static_cast<int>(ToolBrowse))) {
        b->setChecked(true);
    }
    updateToolCaption();
    m_toolCaption->setText(m_toolCaption->text()
                           + (accepted ? u8("  | 已提交") : u8("  | 已取消")));
}

void MainWindow::beginSession(DrawTool tool)
{
    if (!m_view->beginDrawSession(tool)) {
        m_toolCaption->setText(u8("无法开始会话：无底图，或已经在会话中"));
        return;
    }
    m_toolGroup->setExclusive(false);
    if (QAbstractButton* b = m_toolGroup->checkedButton()) {
        b->setChecked(false);
    }
    m_toolGroup->setExclusive(true);
    m_view->setFocus();
    updateToolCaption();
}

// 画笔线宽变化
void MainWindow::onWidthChanged(int)
{
    applyPenToView();
}

// 预设画笔颜色
void MainWindow::onColorId(int id)
{
    const QColor presets[4] = {
        QColor(0xFA, 0xAD, 0x14),
        QColor(0x52, 0xC4, 0x1A),
        QColor(0xFF, 0x4D, 0x4F),
        QColor(0xFF, 0xFF, 0xFF)
    };
    if (id >= 0 && id < 4) {
        m_penColor = presets[id];
        applyPenToView();
    }
}

// 自定义画笔颜色
void MainWindow::onPickColor()
{
    const QColor c = QColorDialog::getColor(m_penColor, this, u8("选择颜色"));
    if (!c.isValid()) {
        return;
    }
    m_penColor = c;
    m_customColor = c;
    applyPenToView();
}

// 描边 / 填充切换
void MainWindow::onDrawModeChanged()
{
    applyPenToView();
}

// 底图层显隐
void MainWindow::onLayerImage(bool on)
{
    m_view->setImageVisible(on);
}

// Stack 层显隐
void MainWindow::onLayerStack(bool on)
{
    m_view->setStackVisible(on);
}

// Drawing 层显隐
void MainWindow::onLayerDrawing(bool on)
{
    m_view->setDrawingVisible(on);
}

// HUD 显隐
void MainWindow::onLayerHud(bool on)
{
    m_view->setHudVisible(on);
}

// 像素检验开关
void MainWindow::onPixelInspect(bool on)
{
    m_view->setPixelInspect(on);
}

// 浮动工具条显隐
void MainWindow::onToolbarVisible(bool on)
{
    m_view->setOverlayToolbarVisible(on);
}

// 按勾选决定工具条上显示哪些测量按钮
void MainWindow::applyShownMeasureTools()
{
    if (!m_view) {
        return;
    }
    std::vector<HsCtrl::DrawTool> tools;
    if (m_measDist && m_measDist->isChecked()) {
        tools.push_back(HsCtrl::ToolMeasureDist);
    }
    if (m_measAngle && m_measAngle->isChecked()) {
        tools.push_back(HsCtrl::ToolMeasureAngle);
    }
    if (m_measRadius && m_measRadius->isChecked()) {
        tools.push_back(HsCtrl::ToolMeasureRadius);
    }
    if (m_measRect && m_measRect->isChecked()) {
        tools.push_back(HsCtrl::ToolMeasureRect);
    }
    if (m_measEllipse && m_measEllipse->isChecked()) {
        tools.push_back(HsCtrl::ToolMeasureEllipse);
    }
    m_view->setMeasureTools(tools);
    m_view->setMeasureToolbarVisible(!tools.empty());
}

// 切换测试用像素比例。0 为 1:1
void MainWindow::onCalibPreset(int index)
{
    if (!m_calibUnit || !m_calibX || !m_calibY || index < 0) {
        return;
    }
    m_calibBusy = true;
    m_calibUnit->setCurrentIndex(0);
    m_calibX->setValue(1.0);
    m_calibY->setValue(1.0);
    if (m_showPx) m_showPx->setChecked(index == 0);
    if (m_showMm) m_showMm->setChecked(index == 1 || index == 2);
    if (m_showUm) m_showUm->setChecked(index == 2);
    if (m_showPulse) m_showPulse->setChecked(index == 3);
    if (index == 1 || index == 2) {
        m_calibUnit->setCurrentIndex(1);
        m_calibX->setValue(0.012);
        m_calibY->setValue(0.012);
    } else if (index == 3) {
        m_calibUnit->setCurrentIndex(3);
        m_calibX->setValue(10.0);
        m_calibY->setValue(8.0);
    }
    m_calibBusy = false;
    applyCalibFromUi();
}

// 用面板上的单位和每像素比例写入标定
void MainWindow::applyCalibFromUi()
{
    if (!m_view || m_calibBusy) {
        return;
    }
    HsCommData::ImageCalib calib;
    const int unitIndex = m_calibUnit ? m_calibUnit->currentIndex() : 0;
    HsCommData::LengthUnit unit = HsCommData::UnitPx;
    if (unitIndex == 1) {
        unit = HsCommData::UnitMm;
    } else if (unitIndex == 2) {
        unit = HsCommData::UnitUm;
    } else if (unitIndex == 3) {
        unit = HsCommData::UnitPulse;
    }
    if (unit != HsCommData::UnitPx) {
        HsCommData::UnitFactor factor;
        factor.unit = unit;
        factor.xPerPixel = m_calibX ? m_calibX->value() : 1.0;
        factor.yPerPixel = m_calibY ? m_calibY->value() : 1.0;
        calib.factors.push_back(factor);
    }
    calib.show.clear();
    if (m_showPx && m_showPx->isChecked()) {
        calib.show.push_back(HsCommData::UnitPx);
    }
    if (m_showMm && m_showMm->isChecked()) {
        calib.show.push_back(HsCommData::UnitMm);
    }
    if (m_showUm && m_showUm->isChecked()) {
        calib.show.push_back(HsCommData::UnitUm);
    }
    if (m_showPulse && m_showPulse->isChecked()) {
        calib.show.push_back(HsCommData::UnitPulse);
    }
    if (calib.show.empty()) {
        calib.show.push_back(unit);
    }
    m_view->setImageCalib(calib);
    refreshMeasureLog();
}

// 刷新测量结果框
void MainWindow::refreshMeasureLog()
{
    if (!m_view || !m_measureLog) {
        return;
    }
    const QStringList lines = m_view->measureResultLines();
    if (lines.isEmpty()) {
        m_measureLog->setPlainText(QString::fromUtf8("还没有测量"));
        return;
    }
    m_measureLog->setPlainText(lines.join(QStringLiteral("\n")));
}

// 工具条停靠角
void MainWindow::onToolbarCorner(int index)
{
    if (!m_toolbarCorner || index < 0) {
        return;
    }
    const int corner = m_toolbarCorner->itemData(index).toInt();
    m_view->setOverlayToolbarCorner(static_cast<Qt::Corner>(corner));
}

// 工具条垂直或水平
void MainWindow::onToolbarOrientation()
{
    m_view->setOverlayToolbarOrientation(
        m_toolbarHorizontal && m_toolbarHorizontal->isChecked() ? Qt::Horizontal : Qt::Vertical);
}

// 工具条皮肤
void MainWindow::onToolbarSkin(int index)
{
    if (!m_toolbarSkin || index < 0) {
        return;
    }
    m_view->setOverlayToolbarSkin(
        static_cast<OverlayToolbarSkin>(m_toolbarSkin->itemData(index).toInt()));
}

// 视口背景风格
void MainWindow::onViewportBackground(int index)
{
    if (!m_viewportBg || index < 0) {
        return;
    }
    m_view->setViewportBackground(
        static_cast<ViewportBackground>(m_viewportBg->itemData(index).toInt()));
}

// 视口自定义色（纯色或棋盘格浅格）
void MainWindow::onViewportColor()
{
    QColor cur = m_view->viewportBackgroundCustomColor();
    if (!cur.isValid()) {
        cur = m_view->backgroundColor();
    }
    const QColor c = QColorDialog::getColor(cur, this, u8("视口颜色"));
    if (!c.isValid()) {
        return;
    }
    m_view->setViewportBackgroundCustomColor(c);
    const ViewportBackground mode = m_view->viewportBackground();
    if (mode != ViewportCheckerboard && mode != ViewportCheckerboardDark) {
        m_view->setBackgroundColor(c);
        if (m_viewportBg) {
            for (int i = 0; i < m_viewportBg->count(); ++i) {
                if (m_viewportBg->itemData(i).toInt() == static_cast<int>(ViewportCustom)) {
                    m_viewportBg->blockSignals(true);
                    m_viewportBg->setCurrentIndex(i);
                    m_viewportBg->blockSignals(false);
                    break;
                }
            }
        }
    }
}

// 把刷新策略写到窗口（下次换底图生效）
void MainWindow::applyRefreshPolicyFromUi()
{
    ImageRefreshPolicy p;
    p.refreshStack = m_refreshStack && m_refreshStack->isChecked();
    p.refreshDrawing = m_refreshDrawing && m_refreshDrawing->isChecked();
    p.refreshText = m_refreshText && m_refreshText->isChecked();
    m_view->setImageRefreshPolicy(p);
}

// 选中 ROI 变化，刷新右侧几何
void MainWindow::onRoiSelected(const HsCommData::Graphic& g)
{
    if (g.id.empty()) {
        m_typeEdit->setText(QStringLiteral("--"));
        m_roleEdit->setText(QStringLiteral("--"));
        showRoiGeom(HsCommData::Graphic());
        return;
    }
    m_typeEdit->setText(kindName(g));
    m_roleEdit->setText(g.role == HsCommData::RoleDrawing ? QStringLiteral("Drawing") : QStringLiteral("Stack"));
    showRoiGeom(g);
    fillTextUi(g);
}

// 把 HsCommData::Graphic 几何写到右侧文本
void MainWindow::showRoiGeom(const HsCommData::Graphic& g)
{
    if (!m_roiGeom) {
        return;
    }
    if (g.id.empty()) {
        m_roiGeom->clear();
        return;
    }

    const QString deg = QString::fromUtf8("°");
    const QString unit = HsCommData::HsSystem::instance().angleUnit == HsCommData::HsSystem::UnitDegree ? deg : QString::fromUtf8("rad");
    QString t;
    if (g.kind == HsCommData::KindObject && g.geom.isInitialized()) {
    switch (g.geom.kind()) {
    case HsCommData::ObjCircle:
        t = u8("中心  (%1, %2)\n半径  %3")
                .arg(g.geom.circleAt(0)->c.x, 0, 'f', 2)
                .arg(g.geom.circleAt(0)->c.y, 0, 'f', 2)
                .arg(g.geom.circleAt(0)->r, 0, 'f', 2);
        break;
    case HsCommData::ObjEllipse:
        t = u8("中心  (%1, %2)\nra    %3\nrb    %4\nphi   %5 %6")
                .arg(g.geom.ellipseAt(0)->c.x, 0, 'f', 2)
                .arg(g.geom.ellipseAt(0)->c.y, 0, 'f', 2)
                .arg(g.geom.ellipseAt(0)->ra, 0, 'f', 2)
                .arg(g.geom.ellipseAt(0)->rb, 0, 'f', 2)
                .arg(g.geom.ellipseAt(0)->ang(), 0, 'f', 4)
                                .arg(unit);
        break;
    case HsCommData::ObjRect1: {
        const HsCommData::Box2 r = normalized(g.geom.rect1At(0)->r);
        t = u8("左上  (%1, %2)\n右下  (%3, %4)\n宽高  %5 × %6")
                .arg(HsCommData::left(r), 0, 'f', 2)
                .arg(HsCommData::top(r), 0, 'f', 2)
                .arg(HsCommData::right(r), 0, 'f', 2)
                .arg(HsCommData::bottom(r), 0, 'f', 2)
                .arg(HsCommData::width(r), 0, 'f', 2)
                .arg(HsCommData::height(r), 0, 'f', 2);
        break;
    }
    case HsCommData::ObjRect2:
        t = u8("中心  (%1, %2)\nl1    %3\nl2    %4\nphi   %5 %6")
                .arg(g.geom.rect2At(0)->c.x, 0, 'f', 2)
                .arg(g.geom.rect2At(0)->c.y, 0, 'f', 2)
                .arg(g.geom.rect2At(0)->l1, 0, 'f', 2)
                .arg(g.geom.rect2At(0)->l2, 0, 'f', 2)
                .arg(g.geom.rect2At(0)->ang(), 0, 'f', 4)
                                .arg(unit);
        break;
    case HsCommData::ObjLine:
        t = u8("起点  (%1, %2)\n终点  (%3, %4)\n长度  %5")
                .arg(g.geom.lineAt(0)->a.x, 0, 'f', 2)
                .arg(g.geom.lineAt(0)->a.y, 0, 'f', 2)
                .arg(g.geom.lineAt(0)->b.x, 0, 'f', 2)
                .arg(g.geom.lineAt(0)->b.y, 0, 'f', 2)
                .arg(QLineF(QPointF(g.geom.lineAt(0)->a.x, g.geom.lineAt(0)->a.y),
                            QPointF(g.geom.lineAt(0)->b.x, g.geom.lineAt(0)->b.y)).length(), 0, 'f', 2);
        break;
    case HsCommData::ObjContour: {
        const int n = static_cast<int>(g.geom.contourAt(0)->ptCount());
        t = u8("点数  %1\n闭合  %2")
                .arg(n)
                .arg(g.geom.contourAt(0)->closed ? u8("是") : u8("否"));
        if (n > 0) {
            t += u8("\n起点  (%1, %2)")
                     .arg(g.geom.contourAt(0)->ptFront().x, 0, 'f', 2)
                     .arg(g.geom.contourAt(0)->ptFront().y, 0, 'f', 2);
            t += u8("\n终点  (%1, %2)")
                     .arg(g.geom.contourAt(0)->ptBack().x, 0, 'f', 2)
                     .arg(g.geom.contourAt(0)->ptBack().y, 0, 'f', 2);
        }
        break;
    }
    case HsCommData::ObjRegion: {
        const HsCommData::IRect b = g.geom.regionAt(0)->bounds;
        t = u8("包围盒  (%1, %2) - (%3, %4)\n宽高    %5 × %6\n面积    %7\n弦数    %8")
                .arg(HsCommData::left(b))
                .arg(HsCommData::top(b))
                .arg(HsCommData::right(b))
                .arg(HsCommData::bottom(b))
                .arg(b.width)
                .arg(b.height)
                .arg(g.geom.regionAt(0)->area())
                .arg(static_cast<int>(g.geom.regionAt(0)->runCount()));
        break;
    }
    default:
        break;
    }
    } else {
    switch (g.kind) {
    case HsCommData::KindCross:
        if (g.cross.empty()) {
            break;
        }
        if (g.cross[0].size < 0.0) {
            t = u8("中心  (%1, %2)\n尺寸  穿窗\nphi   %3 %4")
                    .arg(g.cross[0].center.x, 0, 'f', 2)
                    .arg(g.cross[0].center.y, 0, 'f', 2)
                    .arg(g.cross[0].ang(), 0, 'f', 4)
                                        .arg(unit);
        } else {
            t = u8("中心  (%1, %2)\n半宽  %3\nphi   %4 %5")
                    .arg(g.cross[0].center.x, 0, 'f', 2)
                    .arg(g.cross[0].center.y, 0, 'f', 2)
                    .arg(g.cross[0].size, 0, 'f', 2)
                    .arg(g.cross[0].ang(), 0, 'f', 4)
                                        .arg(unit);
        }
        break;
    case HsCommData::KindArrow:
        if (g.arrow.empty()) {
            break;
        }
        t = u8("箭尾  (%1, %2)\n箭头  (%3, %4)\n长度  %5")
                .arg(g.arrow[0].tail.x, 0, 'f', 2)
                .arg(g.arrow[0].tail.y, 0, 'f', 2)
                .arg(g.arrow[0].head.x, 0, 'f', 2)
                .arg(g.arrow[0].head.y, 0, 'f', 2)
                .arg(QLineF(QPointF(g.arrow[0].tail.x, g.arrow[0].tail.y),
                            QPointF(g.arrow[0].head.x, g.arrow[0].head.y)).length(), 0, 'f', 2);
        break;
    case HsCommData::KindText:
        if (g.text.empty()) {
            break;
        }
        t = u8("文字  %1\n位置  (%2, %3)\n字号  %4 px")
                .arg(QString::fromUtf8(g.text[0].content.data(), static_cast<int>(g.text[0].content.size())))
                .arg(g.text[0].anchor.x, 0, 'f', 2)
                .arg(g.text[0].anchor.y, 0, 'f', 2)
                .arg(qRound(g.text[0].fontPx));
        break;
    default:
        t = u8("id  ") + QString::fromStdString(g.id);
        break;
    }
    }
    m_roiGeom->setPlainText(t);
}

// 选中文本 ROI 时回填文字面板
void MainWindow::fillTextUi(const HsCommData::Graphic& g)
{
    if (m_textUiBusy || !m_textEdit) {
        return;
    }
    if (g.kind != HsCommData::KindText || g.id.empty() || g.text.empty()) {
        return;
    }
    m_textUiBusy = true;
    m_textEdit->setText(QString::fromUtf8(g.text[0].content.data(), static_cast<int>(g.text[0].content.size())));
    m_textXSpin->setValue(g.text[0].anchor.x);
    m_textYSpin->setValue(g.text[0].anchor.y);
    const int px = qRound(g.text[0].fontPx);
    if (px >= m_textFontSpin->minimum() && px <= m_textFontSpin->maximum()) {
        m_textFontSpin->setValue(px);
    }
    m_textImageRadio->setChecked(true);
    m_textUiBusy = false;
    syncTextToolToView();
}

// 把文本面板同步到窗口工具状态
void MainWindow::syncTextToolToView()
{
    if (m_textUiBusy || !m_view || !m_textEdit) {
        return;
    }
    m_view->setDrawText(m_textEdit->text());
    m_view->setTextFontPx(m_textFontSpin->value());
    m_view->setTextToolSpace(m_textWindowRadio->isChecked() ? DrawInfoWindow : DrawInfoImage);
}

// 文本工具点下，回填坐标
void MainWindow::onTextAnchorPicked(const QPointF& imagePt, const QPointF& widgetPt)
{
    if (!m_textXSpin) {
        return;
    }
    m_textUiBusy = true;
    if (m_textWindowRadio->isChecked()) {
        m_textXSpin->setValue(widgetPt.x());
        m_textYSpin->setValue(widgetPt.y());
    } else {
        m_textXSpin->setValue(imagePt.x());
        m_textYSpin->setValue(imagePt.y());
    }
    m_textUiBusy = false;
}

// 把右侧文本坐标放到窗口
void MainWindow::onPlaceText()
{
    if (!m_view || !m_textEdit) {
        return;
    }
    QString msg = m_textEdit->text();
    if (msg.trimmed().isEmpty()) {
        msg = QStringLiteral("Text");
        m_textEdit->setText(msg);
    }
    syncTextToolToView();
    const QPointF pos(m_textXSpin->value(), m_textYSpin->value());
    const int px = m_textFontSpin->value();
    if (m_textWindowRadio->isChecked()) {
        m_view->drawInfo(DrawInfoWindow, msg, pos, px, m_penColor);
        return;
    }
    HsCommData::Graphic g = m_view->selectedDrawing();
    if (g.kind == HsCommData::KindText && !g.id.empty()) {
        const QByteArray bytes = msg.toUtf8();
        if (g.text.empty()) {
            g.text.resize(1);
        }
        g.text[0].content.assign(bytes.constData(), static_cast<size_t>(bytes.size()));
        g.text[0].anchor = HsCommData::Point2d(pos.x(), pos.y());
        g.text[0].fontPx = px;
        g.geomStyle = m_view->drawingStyle();
        m_view->setDrawing(g);
        return;
    }
    HsCommData::Graphic t;
    t.kind = HsCommData::KindText;
    t.role = HsCommData::RoleDrawing;
    t.geomStyle = m_view->drawingStyle();
    t.text.resize(1);
    t.text[0].anchor = HsCommData::Point2d(pos.x(), pos.y());
    const QByteArray bytes = msg.toUtf8();
    t.text[0].content.assign(bytes.constData(), static_cast<size_t>(bytes.size()));
    t.text[0].fontPx = px;
    t.visible = true;
    m_view->setDrawing(t);
}

// ROI 松手提交
void MainWindow::onRoiCommitted(const HsCommData::Graphic& g)
{
    onRoiSelected(g);
    refreshMeasureLog();
}

// 确认当前选中 ROI（占位）
void MainWindow::onConfirm()
{
    const HsCommData::Graphic g = m_view->selectedDrawing();
    if (!g.id.empty()) {
        onRoiSelected(g);
    }
}

// 绑定算法输入图
bool MainWindow::bindAlgoImage(const QImage& img, bool showInView)
{
    if (img.isNull()) {
        m_algoStats->setText(u8("图像为空"));
        return false;
    }
    if (showInView) {
        ++m_frameSeq;
        m_sourceImage = img;
        pushSourceToView();
    }
    QString err;
    const QImage src = showInView ? m_sourceImage : img;
    if (!prepareThresholdImage(src, &err)) {
        m_algoStats->setText(err);
        return false;
    }
    m_algoStats->setText(u8("引擎: ") + halconEngineName()
                         + u8("\n已绑定 ") + QString::number(src.width())
                         + QLatin1Char('x') + QString::number(src.height())
                         + u8("，拖动阈值滑杆提取区域。"));
    return true;
}

// 为算法测试加载图像
void MainWindow::loadAlgoImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, u8("加载测试图像"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (path.isEmpty()) {
        return;
    }
    QImage img;
    if (!img.load(path)) {
        m_algoStats->setText(u8("图像加载失败"));
        return;
    }
    if (!bindAlgoImage(img, true)) {
        return;
    }
    if (m_algoLive->isChecked()) {
        runAlgoRefresh();
    }
}

// 用当前底图做算法输入
void MainWindow::useCurrentImage()
{
    if (m_sourceImage.isNull()) {
        m_algoStats->setText(u8("当前没有图像"));
        return;
    }
    if (!bindAlgoImage(m_sourceImage, false)) {
        return;
    }
    if (m_algoLive->isChecked()) {
        runAlgoRefresh();
    }
}

// 阈值滑条变化，刷新 Overlay
void MainWindow::onThresholdChanged(int v)
{
    if (m_thSlider->value() != v) {
        m_thSlider->blockSignals(true);
        m_thSlider->setValue(v);
        m_thSlider->blockSignals(false);
    }
    if (m_thSpin->value() != v) {
        m_thSpin->blockSignals(true);
        m_thSpin->setValue(v);
        m_thSpin->blockSignals(false);
    }
    if (m_algoLive->isChecked() && !m_algoStress->isChecked()) {
        runAlgoRefresh();
    }
}

// 压力测试：定时反复刷阈值
void MainWindow::onAlgoStress(bool on)
{
    if (on) {
        m_algoPushCount = 0;
        m_algoPushHz = 0;
        m_algoHzClock.restart();
        m_algoTimer->start(0);
    } else {
        m_algoTimer->stop();
    }
}

// 跑一次阈值并 setOverlay
void MainWindow::runAlgoRefresh()
{
    if (m_algoBusy) {
        return;
    }
    m_algoBusy = true;

    if (m_sourceImage.isNull()) {
        m_algoBusy = false;
        return;
    }

    const int t = m_thSpin->value();
    ThresholdRst r = runGrayThreshold(t, 255);
    if (!r.ok && r.error.contains(QString::fromUtf8("尚未"))) {
        bindAlgoImage(m_sourceImage, false);
        r = runGrayThreshold(t, 255);
    }

    QElapsedTimer clock;
    clock.start();
    std::shared_ptr<HsCommData::FrameOverlay> sc = HsCommData::FrameOverlay::create(m_view->currentFrameId());
    qint64 pathUs = 0;
    if (r.ok) {
        HsCommData::Graphic g;
        g.id = "algo.threshold.region";
        g.kind = HsCommData::KindObject;
        g.role = HsCommData::RoleStack;
        g.geomStyle = HsCommData::Style::stackStyle();
        g.geomStyle.stroke = HsCommData::Color4(static_cast<unsigned char>(m_algoColor.red()),
                                static_cast<unsigned char>(m_algoColor.green()),
                                static_cast<unsigned char>(m_algoColor.blue()), 255);
        g.geomStyle.draw = HsCommData::DrawFill;
        const QColor fill = algoFillColor();
        g.geomStyle.fill = HsCommData::Color4(static_cast<unsigned char>(fill.red()),
                              static_cast<unsigned char>(fill.green()),
                              static_cast<unsigned char>(fill.blue()),
                              static_cast<unsigned char>(fill.alpha()));
        g.frameId = m_view->currentFrameId();
        g.geom = r.rle;
        g.updateBounds();
        sc->items.push_back(g);
        pathUs = clock.nsecsElapsed() / 1000;
    }

    clock.restart();
    m_view->setOverlay(sc);
    if (m_algoStress && m_algoStress->isChecked()) {
        m_view->viewport()->repaint();
    }
    const qint64 refreshUs = clock.nsecsElapsed() / 1000;

    ++m_algoPushCount;
    const qint64 hzMs = m_algoHzClock.elapsed();
    const bool report = !m_algoStress->isChecked() || hzMs >= 400;
    if (hzMs >= 400) {
        m_algoPushHz = m_algoPushCount * 1000.0 / static_cast<double>(qMax(hzMs, static_cast<qint64>(1)));
        m_algoPushCount = 0;
        m_algoHzClock.restart();
    }

    if (report) {
        const qint64 totalUs = r.thresholdUs + r.convertUs + pathUs + refreshUs;
        QString text;
        if (!r.ok) {
            text = r.error;
        } else {
            const double instHz = (totalUs > 0) ? (1000000.0 / static_cast<double>(totalUs)) : 0.0;
            text = u8("引擎: ") + (r.usedHalcon ? halconEngineName() : u8("本地扫描"))
                 + u8("\n阈值 ") + QString::number(t)
                 + u8("    面积 ") + QString::number(r.rle.area())
                 + u8("    弦 ") + QString::number(r.rle.runCount())
                 + u8("\n阈值 ") + QString::number(r.thresholdUs / 1000.0, 'f', 2) + u8(" ms")
                 + u8("  转换 ") + QString::number(r.convertUs / 1000.0, 'f', 2) + u8(" ms")
                 + u8("  构图 ") + QString::number(pathUs / 1000.0, 'f', 2) + u8(" ms")
                 + u8("\n刷新 ") + QString::number(refreshUs / 1000.0, 'f', 2) + u8(" ms")
                 + u8("  合计 ") + QString::number(totalUs / 1000.0, 'f', 2) + u8(" ms")
                 + u8("\n本次 ") + QString::number(instHz, 'f', 1) + u8(" Hz")
                 + u8("    连刷 ") + QString::number(m_algoPushHz, 'f', 1) + u8(" Hz")
                 + u8("    控件 ") + QString::number(m_view->fps(), 'f', 1) + u8(" FPS");
        }
        m_algoStats->setText(text);
    }
    m_algoBusy = false;
}

// 算法颜色按钮外观
void MainWindow::styleAlgoColorButton(QToolButton* btn, const QColor& c, bool checkedBorder)
{
    if (!btn) {
        return;
    }
    QString css = QStringLiteral("QToolButton { background:%1; border:1px solid #D9D9D9; border-radius:2px; }")
                      .arg(c.name());
    if (checkedBorder) {
        css += QStringLiteral("QToolButton:checked { border:2px solid #1890FF; }");
    }
    btn->setStyleSheet(css);
}

// 当前算法填充色（含透明度滑条）
QColor MainWindow::algoFillColor() const
{
    const int pct = m_algoOpacitySpin ? m_algoOpacitySpin->value() : 35;
    QColor c = m_algoColor;
    c.setAlpha(qBound(0, qRound(pct * 255.0 / 100.0), 255));
    return c;
}

// 叠加色预设
void MainWindow::onAlgoColorId(int id)
{
    const QColor presets[4] = {
        QColor(0x52, 0xC4, 0x1A),
        QColor(0xFA, 0xAD, 0x14),
        QColor(0xFF, 0x4D, 0x4F),
        QColor(0x18, 0x90, 0xFF)
    };
    if (id >= 0 && id < 4) {
        m_algoColor = presets[id];
        runAlgoRefresh();
    }
}

// 叠加色自定义
void MainWindow::onAlgoPickColor()
{
    const QColor c = QColorDialog::getColor(m_algoColor, this, u8("阈值显示颜色"));
    if (!c.isValid()) {
        return;
    }
    m_algoColor = QColor(c.red(), c.green(), c.blue());
    if (m_algoColorGroup) {
        const QList<QAbstractButton*> btns = m_algoColorGroup->buttons();
        for (int i = 0; i < btns.size(); ++i) {
            btns[i]->setChecked(false);
        }
    }
    styleAlgoColorButton(m_algoPickBtn, m_algoColor, false);
    runAlgoRefresh();
}

// 算法填充不透明度
void MainWindow::onAlgoOpacityChanged(int pct)
{
    pct = qBound(0, pct, 100);
    if (m_algoOpacitySlider && m_algoOpacitySlider->value() != pct) {
        m_algoOpacitySlider->blockSignals(true);
        m_algoOpacitySlider->setValue(pct);
        m_algoOpacitySlider->blockSignals(false);
    }
    if (m_algoOpacitySpin && m_algoOpacitySpin->value() != pct) {
        m_algoOpacitySpin->blockSignals(true);
        m_algoOpacitySpin->setValue(pct);
        m_algoOpacitySpin->blockSignals(false);
    }
    if (!m_algoStress || !m_algoStress->isChecked()) {
        runAlgoRefresh();
    }
}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#include "moc_MainWindow.cpp"
#endif
