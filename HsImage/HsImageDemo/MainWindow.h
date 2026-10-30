/**
 * @file MainWindow.h
 * @brief MainWindow头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_DEMO_MAINWINDOW_H
#define HSIMAGE_DEMO_MAINWINDOW_H

#include <HsCtrl/HsImageCtrl.h>

#include <QMainWindow>
#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QStringList>
#include <QTimer>

class QToolButton;
class QButtonGroup;
class QSpinBox;
class QRadioButton;
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QSlider;
class QPlainTextEdit;
class QDoubleSpinBox;
class QComboBox;

/**
 * @class MainWindow
 * @brief Demo 壳：左工具，中 ImageWidgetApi，右两列参数。界面全 C++ 布局
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /** @brief 搭建界面并连接画布信号 */
    explicit MainWindow(QWidget* parent = 0);
    /** @brief 先释放 ImageWidgetApi，再交给 Qt 拆子控件 */
    ~MainWindow();

private slots:
    /** @brief 打开单张图像并设为底图 */
    void openImage();
    /** @brief 打开文件夹，按文件名排序浏览 */
    void openFolder();
    /** @brief 文件夹上一张 */
    void showPrevImage();
    /** @brief 文件夹下一张 */
    void showNextImage();
    /** @brief 保存视口快照（含叠加，不含工具条） */
    void saveSnapshot();
    /** @brief 导出当前原图像素 */
    void exportOriginal();
    /** @brief 整幅图适应窗口 */
    void fitView();
    /** @brief 1:1 显示 */
    void zoom1to1();
    /** @brief 清空 Stack */
    void clearOverlay();
    /** @brief 清空 Drawing */
    void clearDrawings();
    /** @brief 写入一组示例 Stack 图元 */
    void mockOverlay();
    /** @brief 模拟实时刷 Overlay */
    void toggleLive(bool on);
    /** @brief 切换绘制工具 */
    void onToolId(int id);
    /** @brief 进入绘制会话 */
    void onSessionCircle();
    /** @brief 进入绘制会话 */
    void onSessionRect();
    /** @brief 进入绘制会话 */
    void onSessionPolygon();
    /** @brief 进入屏蔽涂抹会话 */
    void onSessionBrush();
    /** @brief 提交绘制会话 */
    void onSessionCommit();
    /** @brief 取消绘制会话 */
    void onSessionCancel();
    /** @brief 画笔半径（图像像素，一位小数） */
    void onBrushRadius(double radius);
    /** @brief 绘制会话结束 */
    void onDrawSessionFinished(const HsCommData::Graphic& g, bool accepted);
    /** @brief 设置画笔线宽 */
    void onWidthChanged(int w);
    /** @brief 预设画笔颜色 */
    void onColorId(int id);
    /** @brief 自定义画笔颜色 */
    void onPickColor();
    /** @brief 描边 / 填充切换 */
    void onDrawModeChanged();
    /** @brief 底图层显隐 */
    void onLayerImage(bool on);
    /** @brief Stack 层显隐 */
    void onLayerStack(bool on);
    /** @brief Drawing 层显隐 */
    void onLayerDrawing(bool on);
    /** @brief HUD 显隐 */
    void onLayerHud(bool on);
    /** @brief 像素检查开关 */
    void onPixelInspect(bool on);
    /** @brief 浮动工具条显隐 */
    void onToolbarVisible(bool on);
    /** @brief 按勾选刷新工具条上的测量按钮 */
    void applyShownMeasureTools();
    /** @brief 切换测试用的像素比例 */
    void onCalibPreset(int index);
    /** @brief 用面板上的比例和单位写入标定 */
    void applyCalibFromUi();
    /** @brief 把已测结果写到结果框 */
    void refreshMeasureLog();
    /** @brief 工具条停靠角 */
    void onToolbarCorner(int index);
    /** @brief 工具条垂直/水平 */
    void onToolbarOrientation();
    /** @brief 工具条皮肤 */
    void onToolbarSkin(int index);
    /** @brief 视口背景风格 */
    void onViewportBackground(int index);
    /** @brief 视口自定义色 */
    void onViewportColor();
    /** @brief 进入一次绘制会话并同步工具按钮 */
    void beginSession(HsCtrl::DrawTool tool);
    /** @brief 把刷新策略写到窗口 */
    void applyRefreshPolicyFromUi();
    /** @brief 选中 ROI 后，刷新右侧几何 */
    void onRoiSelected(const HsCommData::Graphic& g);
    /** @brief ROI 松手提交 */
    void onRoiCommitted(const HsCommData::Graphic& g);
    /** @brief 确认当前选中 ROI（占位） */
    void onConfirm();
    /** @brief 把右侧文字/坐标放到窗口 */
    void onPlaceText();
    /** @brief 文本工具点下，回填坐标 */
    void onTextAnchorPicked(const QPointF& imagePt, const QPointF& widgetPt);
    /** @brief 把文本面板同步到窗口工具状态 */
    void syncTextToolToView();
    /** @brief 为算法测试加载图像 */
    void loadAlgoImage();
    /** @brief 用当前底图做算法输入 */
    void useCurrentImage();
    /** @brief 阈值滑条变化并刷新 Overlay */
    void onThresholdChanged(int v);
    /** @brief 压力测试：定时反复刷阈值 */
    void onAlgoStress(bool on);
    /** @brief 跑一次阈值并 setOverlay */
    void runAlgoRefresh();
    /** @brief 叠加色预设 */
    void onAlgoColorId(int id);
    /** @brief 叠加色自定义 */
    void onAlgoPickColor();
    /** @brief 叠加填充透明度 */
    void onAlgoOpacityChanged(int pct);

private:
    /** @brief 组装菜单与三栏布局 */
    void buildUi();
    /** @brief 浅色壳样式 */
    void applyShellStyle();
    /** @brief 加载内置演示图 */
    void loadGeneratedImage();
    /** @brief 把 Demo 持有的 QImage 以缓冲形式送进控件 */
    void pushSourceToView();
    /** @brief 把左侧画笔写到窗口 */
    void applyPenToView();
    /** @brief 更新工具栏说明文字 */
    void updateToolCaption();
    /** @brief 把 HsCommData::Graphic 几何写到右侧文本 */
    void showRoiGeom(const HsCommData::Graphic& g);
    /** @brief 选中文本 ROI 时回填文字面板 */
    void fillTextUi(const HsCommData::Graphic& g);
    /**
     * @brief 设置叠加颜色按钮外观
     * @param checkedBorder 选中时画边框
     */
    void styleAlgoColorButton(QToolButton* btn, const QColor& c, bool checkedBorder);
    /** @brief 当前叠加填充色（含透明度滑条） */
    QColor algoFillColor() const;
    /**
     * @brief 绑定算法输入图
     * @param showInView true 时同时设为底图
     */
    bool bindAlgoImage(const QImage& img, bool showInView);
    /** @brief 左侧工具条按钮 */
    QToolButton* addToolButton(QWidget* parent, const QIcon& icon, const QString& text, bool checkable);
    /**
     * @brief 加载指定路径为底图（走窗口刷新策略）
     * @return 解码成功
     */
    bool loadImagePath(const QString& path);
    /** @brief 刷新文件夹序号与上一张/下一张使能 */
    void updateFolderNav();

    image_widget::ImageWidgetApi* m_api; ///< 对外图像控件
    HsCtrl::HsImageWindow* m_view; ///< 画布，等于 m_api->imageWindow()
    QLabel* m_toolCaption;          ///< 当前工具说明
    QButtonGroup* m_toolGroup;      ///< 绘制工具互斥
    QButtonGroup* m_colorGroup;     ///< 画笔预设色
    QToolButton* m_colorBtns[4];    ///< 四个预设色按钮
    QSpinBox* m_widthSpin;          ///< 线宽
    QComboBox* m_lineCombo;         ///< 实线、虚线、点线、点划线
    QRadioButton* m_strokeRadio;    ///< 描边
    QRadioButton* m_fillRadio;      ///< 填充
    QLineEdit* m_typeEdit;          ///< 选中图元 kind
    QLineEdit* m_roleEdit;          ///< 选中图元 role
    QPlainTextEdit* m_roiGeom;      ///< 选中几何文本
    QLineEdit* m_textEdit;          ///< 要显示的文字
    QDoubleSpinBox* m_textXSpin;    ///< 文本 X
    QDoubleSpinBox* m_textYSpin;    ///< 文本 Y
    QSpinBox* m_textFontSpin;       ///< 文本字号
    QRadioButton* m_textImageRadio; ///< 图像坐标
    QRadioButton* m_textWindowRadio; ///< 窗口坐标
    bool m_textUiBusy;              ///< 回填面板时不触发放置
    QCheckBox* m_layerImage;        ///< 底图显隐
    QCheckBox* m_layerStack;        ///< Stack 显隐
    QCheckBox* m_layerDrawing;      ///< Drawing 显隐
    QCheckBox* m_layerHud;          ///< HUD 显隐
    QCheckBox* m_pixelInspect;      ///< 像素检验
    QCheckBox* m_toolbarVisible;    ///< 浮动工具条显隐
    QCheckBox* m_measDist;          ///< 工具条是否显示距离
    QCheckBox* m_measAngle;         ///< 工具条是否显示夹角
    QCheckBox* m_measRadius;        ///< 工具条是否显示圆
    QCheckBox* m_measRect;          ///< 工具条是否显示矩形
    QCheckBox* m_measEllipse;       ///< 工具条是否显示椭圆
    QComboBox* m_calibPreset;        ///< 标定预设：1:1 px / 0.012 mm / mm + μm / pulse
    QComboBox* m_calibUnit;          ///< 标定单位
    QDoubleSpinBox* m_calibX;        ///< 列方向每像素
    QDoubleSpinBox* m_calibY;        ///< 行方向每像素
    QCheckBox* m_showPx;             ///< 结果显示像素
    QCheckBox* m_showMm;             ///< 结果显示毫米
    QCheckBox* m_showUm;             ///< 结果显示微米
    QCheckBox* m_showPulse;          ///< 结果显示脉冲
    QPlainTextEdit* m_measureLog;    ///< 测量结果
    bool m_calibBusy;                ///< 回填标定控件时不重复写入
    QComboBox* m_toolbarCorner;     ///< 工具条停靠角
    QRadioButton* m_toolbarVertical; ///< 工具条竖排
    QRadioButton* m_toolbarHorizontal; ///< 工具条横排
    QComboBox* m_toolbarSkin;       ///< 工具条皮肤
    QComboBox* m_viewportBg;        ///< 视口背景风格
    QToolButton* m_viewportColorBtn; ///< 视口自定义色
    QCheckBox* m_refreshStack;      ///< 换底图时清 Stack
    QCheckBox* m_refreshDrawing;    ///< 换底图时清 Drawing ROI
    QCheckBox* m_refreshText;       ///< 换底图时清 drawInfo
    QLabel* m_imagePathLabel;       ///< 当前文件名 / 序号
    QPushButton* m_prevImageBtn;    ///< 文件夹上一张
    QPushButton* m_nextImageBtn;    ///< 文件夹下一张
    QSlider* m_thSlider;            ///< 阈值
    QSpinBox* m_thSpin;             ///< 阈值数值
    QLabel* m_algoStats;            ///< 算法耗时 / FPS
    QCheckBox* m_algoLive;          ///< 阈值滑杆实时提取
    QCheckBox* m_algoStress;        ///< 压力测试
    QButtonGroup* m_algoColorGroup; ///< 叠加色预设
    QToolButton* m_algoColorBtns[4]; ///< 叠加四色按钮
    QToolButton* m_algoPickBtn;     ///< 叠加自定义色
    QSlider* m_algoOpacitySlider;   ///< 填充透明度
    QSpinBox* m_algoOpacitySpin;    ///< 透明度百分比
    QTimer* m_liveTimer;            ///< 模拟实时 Overlay
    QTimer* m_algoTimer;            ///< 压力测试定时器
    QElapsedTimer m_algoHzClock;    ///< 算法推送频率计时
    int m_algoPushCount;            ///< 本秒内推送次数
    double m_algoPushHz;            ///< 算法推送 Hz
    bool m_algoBusy;                ///< 防止重入刷新
    qint64 m_frameSeq;              ///< Demo 底图帧号
    QImage m_sourceImage;           ///< Demo 自己持有的底图副本（不从控件回读缓存）
    QColor m_penColor;              ///< 当前画笔色
    QColor m_customColor;           ///< 自定义画笔色
    QColor m_algoColor;             ///< 叠加色
    QStringList m_folderFiles;      ///< 当前文件夹图像列表
    int m_folderIndex;              ///< 当前文件下标
};

#endif
