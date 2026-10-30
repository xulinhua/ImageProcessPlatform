/**
 * @file MainWindow.h
 * @brief 评测主窗口与转换工作线程：图像转换页，并挂载结构与锁页
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_BENCH_MAINWINDOW_H
#define HSIMAGE_BENCH_MAINWINDOW_H

#include "ConvertBench.h"

#include <HsCtrl/HsImageCtrl.h>

#include <QImage>
#include <QMainWindow>
#include <QString>
#include <QThread>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;

/**
 * @class BenchWorker
 * @brief 工作线程对象: 生成图/转换计时
 */
class BenchWorker : public QObject {
    Q_OBJECT
public:
    /** @brief 无状态，由 MainWindow 创建后移入工作线程 */
    explicit BenchWorker(QObject* parent = 0);

    /************************ 任务 ************************/
public slots:
    /** @brief 生成灰度图，发 imageReady 后发 finished */
    void genGray(int width, int height);
    /** @brief 生成 RGB888 彩色图，发 imageReady 后发 finished */
    void genColor(int width, int height);
    /**
     * @brief 灰度转换循环计时，逐轮与平均耗时经 logReady 输出
     * @param src 须为 Gray8，否则只提示并结束
     * @param loops 循环次数，小于 1 按 1
     * @param toHalconOnly 真则只测 buffer->H
     */
    void runGray(QImage src, int loops, bool toHalconOnly);
    /**
     * @brief 彩色转换循环计时，先丢弃一轮冷启动再计 loops 轮
     * @param src 非灰度图，否则只提示并结束
     * @param method ColorConvertMethod 取值，未知值按 for 循环
     * @param loops 循环次数，小于 1 按 1
     * @param toHalconOnly 真则只测 buffer->H
     */
    void runColor(QImage src, int method, int loops, bool toHalconOnly);

    /************************ 信号 ************************/
signals:
    /** @brief 新测试图就绪，tip 为日志提示 */
    void imageReady(const QImage& img, const QString& tip);
    /** @brief 转换前/后图就绪；仅->H 时 after 为空 */
    void convertPairReady(const QImage& before, const QImage& after);
    /** @brief 一段日志文本（可含多行） */
    void logReady(const QString& text);
    /** @brief 任务结束（成功或失败都发），界面据此解除忙状态 */
    void finished();
};

/**
 * @class MainWindow
 * @brief HsImageBench 评测窗口。图像转换，以及 region、contour 等结构的大批量拷贝，都在本程序测试
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /** @brief 搭建两页界面、启动工作线程，默认显示结构与锁页并生成一张彩色图 */
    explicit MainWindow(QWidget* parent = 0);
    /** @brief 停止工作线程（最多等 5 秒）并释放两个图像控件 */
    ~MainWindow();

    /************************ 按钮 ************************/
private slots:
    /** @brief 按宽高在工作线程生成灰度图；忙时忽略 */
    void onGenGray();
    /** @brief 按宽高在工作线程生成彩色图；忙时忽略 */
    void onGenColor();
    /** @brief 用当前源图测灰度转换；忙时忽略 */
    void onRunGray();
    /** @brief 用当前源图和所选方式测彩色转换；忙时忽略 */
    void onRunColorRoundTrip();
    /** @brief 清空日志 */
    void onClearLog();

    /************************ 结果 ************************/
    /** @brief 新图作为转换前图显示，清空转换后图 */
    void onImageReady(const QImage& img, const QString& tip);
    /** @brief 刷新转换前/后两图；转换后为空时写一条提示 */
    void onConvertPairReady(const QImage& before, const QImage& after);
    /** @brief 把工作线程日志追加到日志框 */
    void onLogReady(const QString& text);
    /** @brief 任务结束，解除忙状态 */
    void onWorkerFinished();

    /************************ 辅助 ************************/
private:
    /** @brief 记录忙状态并启用/禁用四个任务按钮 */
    void setBusy(bool busy);
    /** @brief 把 m_before 显示到左侧控件 */
    void showBeforeImage();
    /** @brief 把 m_after 显示到右侧控件 */
    void showAfterImage();
    /** @brief 追加一行日志 */
    void appendLog(const QString& line);
    /** @brief 下拉框当前选中的彩色转换方式，未知值按 for 循环 */
    ColorConvertMethod currentColorMethod() const;
    /**
     * @brief 在图像控件上显示图片及尺寸、像素数、格式信息
     * @param api 目标控件，空则不处理
     * @param img 空图则清空显示，只写 tag
     * @param tag 信息栏前缀
     */
    void setViewerImage(image_widget::ImageWidgetApi* api, const QImage& img, const QString& tag);

    image_widget::ImageWidgetApi* m_apiBefore; ///< 转换前
    image_widget::ImageWidgetApi* m_apiAfter;  ///< 转换后
    QImage m_before;                           ///< 转换前
    QImage m_after;                            ///< 转换后

    QThread* m_thread;     ///< 转换工作线程，析构时退出
    BenchWorker* m_worker; ///< 工作对象，线程结束时 deleteLater
    bool m_busy;           ///< 有任务在跑，按钮槽据此忽略点击

    QSpinBox* m_width;         ///< 测试图宽（64~8192，默认 2448）
    QSpinBox* m_height;        ///< 测试图高（64~8192，默认 2048）
    QSpinBox* m_loops;         ///< 转换循环次数（1~50）
    QComboBox* m_colorMethod;  ///< 彩色转换方式，item data 为 ColorConvertMethod
    QRadioButton* m_dirRound;  ///< 往返 buffer->H->buffer（默认）
    QRadioButton* m_dirToH;    ///< 仅测 buffer->H
    QLabel* m_info;            ///< Halcon/OpenCV 链接状态
    QPlainTextEdit* m_log;     ///< 日志框，最多 500 行
    QPushButton* m_btnGray;    ///< 生成灰度图
    QPushButton* m_btnColor;   ///< 生成彩色图
    QPushButton* m_btnGrayRun; ///< 测灰度转换
    QPushButton* m_btnColorRun; ///< 测彩色转换
};

#endif