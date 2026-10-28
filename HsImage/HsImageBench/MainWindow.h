/**
 * @file MainWindow.h
 * @brief MainWindow头文件
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
    explicit BenchWorker(QObject* parent = 0);

public slots:
    /** @brief 生成灰度图 */
    void genGray(int width, int height);
    /** @brief 生成彩色图 */
    void genColor(int width, int height);
    /** @brief 测灰度转换 */
    void runGray(QImage src, int loops, bool toHalconOnly);
    /** @brief 测彩色转换 */
    void runColor(QImage src, int method, int loops, bool toHalconOnly);

signals:
    /** @brief 源图/测试图就绪 */
    void imageReady(const QImage& img, const QString& tip);
    /** @brief 转换前/后图就绪 */
    void convertPairReady(const QImage& before, const QImage& after);
    /** @brief 日志就绪 */
    void logReady(const QString& text);
    /** @brief 任务结束 */
    void finished();
};

/**
 * @class MainWindow
 * @brief HsImageBench 评测窗口。图像转换，以及 region、contour 等结构的大批量拷贝，都在本程序测试
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private slots:
    void onGenGray();
    void onGenColor();
    void onRunGray();
    void onRunColorRoundTrip();
    void onClearLog();
    void onImageReady(const QImage& img, const QString& tip);
    void onConvertPairReady(const QImage& before, const QImage& after);
    void onLogReady(const QString& text);
    void onWorkerFinished();

private:
    void setBusy(bool busy);
    void showBeforeImage();
    void showAfterImage();
    void appendLog(const QString& line);
    ColorConvertMethod currentColorMethod() const;
    void setViewerImage(image_widget::ImageWidgetApi* api, const QImage& img, const QString& tag);

    image_widget::ImageWidgetApi* m_apiBefore; ///< 转换前
    image_widget::ImageWidgetApi* m_apiAfter;  ///< 转换后
    QImage m_before;                           ///< 转换前
    QImage m_after;                            ///< 转换后

    QThread* m_thread;
    BenchWorker* m_worker;
    bool m_busy;

    QSpinBox* m_width;
    QSpinBox* m_height;
    QSpinBox* m_loops;
    QComboBox* m_colorMethod;
    QRadioButton* m_dirRound;
    QRadioButton* m_dirToH;
    QLabel* m_info;
    QPlainTextEdit* m_log;
    QPushButton* m_btnGray;
    QPushButton* m_btnColor;
    QPushButton* m_btnGrayRun;
    QPushButton* m_btnColorRun;
};

#endif