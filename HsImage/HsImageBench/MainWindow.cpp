/**
 * @file MainWindow.cpp
 * @brief 评测主窗口与转换工作线程实现：界面搭建、任务派发与结果汇总
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MainWindow.h"
#include "ObjectPage.h"
#include <QTabWidget>

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaType>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

static QString L8(const char* s)
{
    return QString::fromUtf8(s);
}

BenchWorker::BenchWorker(QObject* parent)
    : QObject(parent)
{
}

void BenchWorker::genGray(int width, int height)
{
    const QImage img = makeBenchGrayImage(width, height);
    emit imageReady(img, L8("已生成灰度图 %1x%2").arg(img.width()).arg(img.height()));
    emit finished();
}

void BenchWorker::genColor(int width, int height)
{
    const QImage img = makeBenchColorImage(width, height);
    emit imageReady(img, L8("已生成彩色图 %1x%2").arg(img.width()).arg(img.height()));
    emit finished();
}

void BenchWorker::runGray(QImage src, int loops, bool toHalconOnly)
{
    // 前置检查：须是灰度图且链接了 Halcon
    if (src.isNull() || src.format() != QImage::Format_Grayscale8) {
        emit logReady(L8("请先生成灰度图"));
        emit finished();
        return;
    }
    if (!convertBenchHasHalcon()) {
        emit logReady(L8("未链接 Halcon, 无法转换"));
        emit finished();
        return;
    }
    if (loops < 1) {
        loops = 1;
    }
    double sumTo = 0;
    double sumFrom = 0;
    int fromCount = 0;
    QImage lastOut;
    QString all;
    // 逐轮计时并记录明细；任一轮失败即结束
    for (int i = 0; i < loops; ++i) {
        const ConvertBenchRst r = benchGrayRoundTrip(src, toHalconOnly);
        if (!r.ok) {
            emit logReady(L8("失败: %1").arg(r.error));
            emit finished();
            return;
        }
        sumTo += r.msToH;
        if (r.msFromH >= 0.0) {
            sumFrom += r.msFromH;
            ++fromCount;
            lastOut = r.outImage;
        }
        if (r.msFromH >= 0.0) {
            all += L8("[%1/%2] %3\n  buf->H: %4 ms\n  H->buf: %5 ms\n")
                       .arg(i + 1).arg(loops).arg(r.detail)
                       .arg(r.msToH, 0, 'f', 3).arg(r.msFromH, 0, 'f', 3);
        } else {
            all += L8("[%1/%2] %3\n  buf->H: %4 ms\n")
                       .arg(i + 1).arg(loops).arg(r.detail)
                       .arg(r.msToH, 0, 'f', 3);
        }
        if (!r.stages.isEmpty()) {
            all += r.stages;
        }
    }
    // 汇总平均耗时；H->buf 只按实际测到的轮数平均
    all += L8("平均 buf->H: %1 ms (%2)\n").arg(sumTo / loops, 0, 'f', 3).arg(loops);
    if (fromCount > 0) {
        all += L8("平均 H->buf: %1 ms (%2)\n").arg(sumFrom / fromCount, 0, 'f', 3).arg(fromCount);
    }
    all += L8("(仅 buffer->Halcon / Halcon->buffer 接口, 不含 CopyImage)\n");
    emit convertPairReady(src, lastOut);
    emit logReady(all);
    emit finished();
}

void BenchWorker::runColor(QImage src, int method, int loops, bool toHalconOnly)
{
    if (src.isNull() || src.format() == QImage::Format_Grayscale8) {
        emit logReady(L8("请先生成彩色图"));
        emit finished();
        return;
    }
    if (!convertBenchHasHalcon()) {
        emit logReady(L8("未链接 Halcon, 无法转换"));
        emit finished();
        return;
    }
    if (loops < 1) {
        loops = 1;
    }
    ColorConvertMethod m = ColorMethodForLoop;
    if (method == static_cast<int>(ColorMethodInterleave)) {
        m = ColorMethodInterleave;
    } else if (method == static_cast<int>(ColorMethodGenImage3)) {
        m = ColorMethodGenImage3;
    } else if (method == static_cast<int>(ColorMethodOpenCvMerge)) {
        m = ColorMethodOpenCvMerge;
    }
    // 丢弃首轮冷启动（Halcon/堆首次触达），避免平均被拉高
    {
        const ConvertBenchRst warm = benchColorConvert(src, m, toHalconOnly);
        if (!warm.ok) {
            emit logReady(L8("失败: %1").arg(warm.error));
            emit finished();
            return;
        }
    }
    double sumTo = 0;
    double sumFrom = 0;
    int fromCount = 0;
    QImage lastOut;
    QString all;
    for (int i = 0; i < loops; ++i) {
        const ConvertBenchRst r = benchColorConvert(src, m, toHalconOnly);
        if (!r.ok) {
            emit logReady(L8("失败: %1").arg(r.error));
            emit finished();
            return;
        }
        sumTo += r.msToH;
        if (r.msFromH >= 0.0) {
            sumFrom += r.msFromH;
            ++fromCount;
            lastOut = r.outImage;
        }
        if (r.msFromH >= 0.0) {
            all += L8("[%1/%2] %3\n  buf->H: %4 ms\n  H->buf: %5 ms\n")
                       .arg(i + 1).arg(loops).arg(r.detail)
                       .arg(r.msToH, 0, 'f', 3).arg(r.msFromH, 0, 'f', 3);
        } else {
            all += L8("[%1/%2] %3\n  buf->H: %4 ms\n")
                       .arg(i + 1).arg(loops).arg(r.detail)
                       .arg(r.msToH, 0, 'f', 3);
        }
        if (!r.stages.isEmpty()) {
            all += r.stages;
        }
    }
    all += L8("平均 buf->H: %1 ms (%2)\n").arg(sumTo / loops, 0, 'f', 3).arg(loops);
    if (fromCount > 0) {
        all += L8("平均 H->buf: %1 ms (%2)\n").arg(sumFrom / fromCount, 0, 'f', 3).arg(fromCount);
    }
    all += L8("(仅 buffer->Halcon / Halcon->buffer 接口, 不含 CopyImage)\n");
    emit convertPairReady(src, lastOut);
    emit logReady(all);
    emit finished();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_apiBefore(nullptr)
    , m_apiAfter(nullptr)
    , m_thread(nullptr)
    , m_worker(nullptr)
    , m_busy(false)
    , m_width(nullptr)
    , m_height(nullptr)
    , m_loops(nullptr)
    , m_colorMethod(nullptr)
    , m_dirRound(nullptr)
    , m_dirToH(nullptr)
    , m_info(nullptr)
    , m_log(nullptr)
    , m_btnGray(nullptr)
    , m_btnColor(nullptr)
    , m_btnGrayRun(nullptr)
    , m_btnColorRun(nullptr)
{
    qRegisterMetaType<QImage>("QImage");

    // 图像转换页左侧：转换前/后两个图像控件并排
    setWindowTitle(L8("HsImageBench"));
    resize(1480, 920);

    QTabWidget* tabs = new QTabWidget(this);
    setCentralWidget(tabs);
    QWidget* central = new QWidget(tabs);
    QHBoxLayout* root = new QHBoxLayout(central);

    QWidget* viewers = new QWidget(central);
    QVBoxLayout* viewLay = new QVBoxLayout(viewers);
    viewLay->setContentsMargins(0, 0, 0, 0);

    QWidget* rowBefore = new QWidget(viewers);
    QVBoxLayout* layBefore = new QVBoxLayout(rowBefore);
    layBefore->setContentsMargins(0, 0, 0, 0);
    QLabel* labBefore = new QLabel(L8("转换前"), rowBefore);
    m_apiBefore = new image_widget::ImageWidgetApi(rowBefore);
    layBefore->addWidget(labBefore);
    layBefore->addWidget(m_apiBefore->widget(), 1);

    QWidget* rowAfter = new QWidget(viewers);
    QVBoxLayout* layAfter = new QVBoxLayout(rowAfter);
    layAfter->setContentsMargins(0, 0, 0, 0);
    QLabel* labAfter = new QLabel(L8("转换后"), rowAfter);
    m_apiAfter = new image_widget::ImageWidgetApi(rowAfter);
    layAfter->addWidget(labAfter);
    layAfter->addWidget(m_apiAfter->widget(), 1);

    QHBoxLayout* pair = new QHBoxLayout();
    pair->addWidget(rowBefore, 1);
    pair->addWidget(rowAfter, 1);
    viewLay->addLayout(pair, 1);
    root->addWidget(viewers, 1);

    // 右侧面板：库链接状态、生成测试图、转换计时参数、日志
    QWidget* side = new QWidget(central);
    side->setMinimumWidth(340);
    side->setMaximumWidth(420);
    QVBoxLayout* sideLay = new QVBoxLayout(side);

    m_info = new QLabel(side);
    m_info->setWordWrap(true);
    m_info->setText(L8("Halcon: %1\nOpenCV: %2")
                        .arg(convertBenchHasHalcon() ? L8("已链接") : L8("未链接"))
                        .arg(convertBenchHasOpenCv() ? L8("已链接") : L8("未链接(Interleave 走 QImage)")));
    sideLay->addWidget(m_info);

    QGroupBox* genBox = new QGroupBox(L8("生成测试图(约 500 万像素)"), side);
    QFormLayout* genForm = new QFormLayout(genBox);
    m_width = new QSpinBox(genBox);
    m_width->setRange(64, 8192);
    m_width->setValue(2448);
    m_height = new QSpinBox(genBox);
    m_height->setRange(64, 8192);
    m_height->setValue(2048);
    genForm->addRow(L8("宽"), m_width);
    genForm->addRow(L8("高"), m_height);
    m_btnGray = new QPushButton(L8("生成灰度图"), genBox);
    m_btnColor = new QPushButton(L8("生成彩色图"), genBox);
    genForm->addRow(m_btnGray);
    genForm->addRow(m_btnColor);
    sideLay->addWidget(genBox);

    QGroupBox* runBox = new QGroupBox(L8("转换计时"), side);
    QVBoxLayout* runLay = new QVBoxLayout(runBox);
    m_loops = new QSpinBox(runBox);
    m_loops->setRange(1, 50);
    m_loops->setValue(3);
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(new QLabel(L8("循环次数"), runBox));
        row->addWidget(m_loops);
        runLay->addLayout(row);
    }
    m_colorMethod = new QComboBox(runBox);
    m_colorMethod->addItem(L8("1. For循环 / Pointer3"), static_cast<int>(ColorMethodForLoop));
    m_colorMethod->addItem(L8("2. InterleaveChannels+memcpy"), static_cast<int>(ColorMethodInterleave));
    m_colorMethod->addItem(L8("3. GenImage3/Decompose3"), static_cast<int>(ColorMethodGenImage3));
    m_colorMethod->addItem(L8("4. OpenCV split/merge"), static_cast<int>(ColorMethodOpenCvMerge));
    runLay->addWidget(new QLabel(L8("彩色方式"), runBox));
    runLay->addWidget(m_colorMethod);

    m_dirRound = new QRadioButton(L8("往返 缓冲->H->缓冲"), runBox);
    m_dirToH = new QRadioButton(L8("仅 ->Halcon"), runBox);
    m_dirRound->setChecked(true);
    runLay->addWidget(m_dirRound);
    runLay->addWidget(m_dirToH);

    m_btnGrayRun = new QPushButton(L8("测灰度转换"), runBox);
    m_btnColorRun = new QPushButton(L8("测彩色转换"), runBox);
    runLay->addWidget(m_btnGrayRun);
    runLay->addWidget(m_btnColorRun);
    sideLay->addWidget(runBox);

    m_log = new QPlainTextEdit(side);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    sideLay->addWidget(m_log, 1);

    QPushButton* btnClear = new QPushButton(L8("清空日志"), side);
    sideLay->addWidget(btnClear);
    root->addWidget(side);

    // 工作对象移入独立线程，结果经排队信号回到界面线程
    m_thread = new QThread(this);
    m_worker = new BenchWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, SIGNAL(finished()), m_worker, SLOT(deleteLater()));
    connect(m_worker, SIGNAL(imageReady(QImage,QString)), this, SLOT(onImageReady(QImage,QString)));
    connect(m_worker, SIGNAL(convertPairReady(QImage,QImage)), this, SLOT(onConvertPairReady(QImage,QImage)));
    connect(m_worker, SIGNAL(logReady(QString)), this, SLOT(onLogReady(QString)));
    connect(m_worker, SIGNAL(finished()), this, SLOT(onWorkerFinished()));
    m_thread->start();

    connect(m_btnGray, SIGNAL(clicked()), this, SLOT(onGenGray()));
    connect(m_btnColor, SIGNAL(clicked()), this, SLOT(onGenColor()));
    connect(m_btnGrayRun, SIGNAL(clicked()), this, SLOT(onRunGray()));
    connect(m_btnColorRun, SIGNAL(clicked()), this, SLOT(onRunColorRoundTrip()));
    connect(btnClear, SIGNAL(clicked()), this, SLOT(onClearLog()));

    // 默认停在结构与锁页，同时预生成一张彩色图供转换页使用
    tabs->addTab(central, L8("图像转换"));
    tabs->addTab(new ObjectPage(tabs), L8("结构与锁"));
    tabs->setCurrentIndex(1);
    onGenColor();
}

MainWindow::~MainWindow()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
    if (m_apiBefore) {
        delete m_apiBefore;
        m_apiBefore = nullptr;
    }
    if (m_apiAfter) {
        delete m_apiAfter;
        m_apiAfter = nullptr;
    }
}

void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    const bool en = !busy;
    if (m_btnGray) m_btnGray->setEnabled(en);
    if (m_btnColor) m_btnColor->setEnabled(en);
    if (m_btnGrayRun) m_btnGrayRun->setEnabled(en);
    if (m_btnColorRun) m_btnColorRun->setEnabled(en);
}

ColorConvertMethod MainWindow::currentColorMethod() const
{
    const int v = m_colorMethod->currentData().toInt();
    if (v == static_cast<int>(ColorMethodInterleave)) return ColorMethodInterleave;
    if (v == static_cast<int>(ColorMethodGenImage3)) return ColorMethodGenImage3;
    if (v == static_cast<int>(ColorMethodOpenCvMerge)) return ColorMethodOpenCvMerge;
    return ColorMethodForLoop;
}

void MainWindow::setViewerImage(image_widget::ImageWidgetApi* api, const QImage& img, const QString& tag)
{
    if (!api) return;
    if (img.isNull()) {
        api->imageWindow()->clearImage();
        api->setImageInfoText(tag);
        return;
    }
    api->setImage(img);
    const qint64 pix = static_cast<qint64>(img.width()) * static_cast<qint64>(img.height());
    api->setImageInfoText(L8("%1  %2x%3  %4 万像素  %5")
                              .arg(tag)
                              .arg(img.width())
                              .arg(img.height())
                              .arg(pix / 10000.0, 0, 'f', 2)
                              .arg(img.format() == QImage::Format_Grayscale8 ? L8("Gray8") : L8("RGB888")));
}

void MainWindow::showBeforeImage()
{
    setViewerImage(m_apiBefore, m_before, L8("转换前"));
}

void MainWindow::showAfterImage()
{
    setViewerImage(m_apiAfter, m_after, L8("转换后"));
}

void MainWindow::appendLog(const QString& line)
{
    m_log->appendPlainText(line);
}

void MainWindow::onGenGray()
{
    if (m_busy) return;
    setBusy(true);
    appendLog(L8("正在生成灰度图..."));
    QMetaObject::invokeMethod(m_worker, "genGray", Qt::QueuedConnection,
                              Q_ARG(int, m_width->value()), Q_ARG(int, m_height->value()));
}

void MainWindow::onGenColor()
{
    if (m_busy) return;
    setBusy(true);
    appendLog(L8("正在生成彩色图..."));
    QMetaObject::invokeMethod(m_worker, "genColor", Qt::QueuedConnection,
                              Q_ARG(int, m_width->value()), Q_ARG(int, m_height->value()));
}

void MainWindow::onRunGray()
{
    if (m_busy) return;
    setBusy(true);
    appendLog(L8("正在测灰度转换..."));
    QMetaObject::invokeMethod(m_worker, "runGray", Qt::QueuedConnection, Q_ARG(QImage, m_before),
                              Q_ARG(int, m_loops->value()), Q_ARG(bool, m_dirToH->isChecked()));
}

void MainWindow::onRunColorRoundTrip()
{
    if (m_busy) return;
    setBusy(true);
    appendLog(L8("正在测彩色转换..."));
    QMetaObject::invokeMethod(m_worker, "runColor", Qt::QueuedConnection, Q_ARG(QImage, m_before),
                              Q_ARG(int, static_cast<int>(currentColorMethod())),
                              Q_ARG(int, m_loops->value()), Q_ARG(bool, m_dirToH->isChecked()));
}

void MainWindow::onClearLog()
{
    m_log->clear();
}

void MainWindow::onImageReady(const QImage& img, const QString& tip)
{
    m_before = img;
    m_after = QImage();
    showBeforeImage();
    showAfterImage();
    appendLog(tip);
}

void MainWindow::onConvertPairReady(const QImage& before, const QImage& after)
{
    m_before = before;
    m_after = after;
    showBeforeImage();
    showAfterImage();
    if (after.isNull()) {
        appendLog(L8("仅->H 无 H->buf 图, 转换后空"));
    }
}

void MainWindow::onLogReady(const QString& text)
{
    appendLog(text);
}

void MainWindow::onWorkerFinished()
{
    setBusy(false);
}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#include "moc_MainWindow.cpp"
#endif