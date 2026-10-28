/**
 * @file ObjectPage.cpp
 * @brief ObjectPage实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ObjectPage.h"

#include <HsCommData/Graphic.h>
#include <HsCommData/Style.h>
#include <HsCtrl/HsImageCtrl.h>

#include <QColor>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QThread>
#include <QVBoxLayout>

#include <vector>

static QString L8(const char* s)
{
    return QString::fromUtf8(s);
}

static QString formatMs(double ms)
{
    if (ms < 0) {
        return QStringLiteral("--");
    }
    if (ms < 0.01) {
        return QString::number(ms * 1000.0, 'f', 1) + QStringLiteral(" us");
    }
    if (ms >= 1000.0) {
        return QString::number(ms / 1000.0, 'f', 2) + QStringLiteral(" s");
    }
    return QString::number(ms, 'f', 2) + QStringLiteral(" ms");
}

static QString formatBytes(qint64 n)
{
    if (n < 1024) {
        return QString::number(n) + QStringLiteral(" B");
    }
    if (n < 1024 * 1024) {
        return QString::number(n / 1024.0, 'f', 1) + QStringLiteral(" KB");
    }
    return QString::number(n / 1024.0 / 1024.0, 'f', 1) + QStringLiteral(" MB");
}

static QFrame* metricCard(QWidget* parent, const QString& title, QLabel** value)
{
    QFrame* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("metric"));
    QVBoxLayout* lay = new QVBoxLayout(frame);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(2);
    QLabel* cap = new QLabel(title, frame);
    cap->setObjectName(QStringLiteral("metricTitle"));
    QLabel* val = new QLabel(QStringLiteral("--"), frame);
    val->setObjectName(QStringLiteral("metricValue"));
    lay->addWidget(cap);
    lay->addWidget(val);
    *value = val;
    return frame;
}

static QString failText(int fail, int errorCode)
{
    if (fail == 1) {
        return L8("参数无效或还没有样本");
    }
    if (fail == 2) {
        return L8("样本超过 800MB，请减小个数或载荷");
    }
    if (fail == 3) {
        return L8("异常，错误码 %1").arg(errorCode);
    }
    return L8("失败");
}

ObjectWorker::ObjectWorker(QObject* parent)
    : QObject(parent)
{
}

HsCommData::HsObject ObjectWorker::sample() const
{
    return m_sample;
}

void ObjectWorker::buildSample(int kind, int count, int payload)
{
    const ObjectSampleStats st = fillSample(&m_sample, kind, count, payload);
    emit sampleReady(st.count, st.payloadUnits, st.bytes, st.imageW, st.imageH, st.fail, st.errorCode);
    emit taskFinished();
}

void ObjectWorker::runCopy(int loops)
{
    const CopyTimings t = benchCopy(m_sample, loops);
    emit copyReady(t.fail, t.errorCode, t.loops, t.msHandle, t.msElement, t.msClone, t.isolated);
    emit taskFinished();
}

void ObjectWorker::runLock(int readers, int writers, int opsEach)
{
    const LockTimings t = benchLock(&m_sample, readers, writers, opsEach);
    emit lockReady(t.ok, t.errors, t.readerOps, t.writerOps, t.startCount, t.finalCount, t.ms, t.countOk, t.cloneStable);
    emit taskFinished();
}

ObjectPage::ObjectPage(QWidget* parent)
    : QWidget(parent)
    , m_api(nullptr)
    , m_worker(nullptr)
    , m_thread(nullptr)
    , m_busy(false)
    , m_hasSample(false)
    , m_kind(SampleRegion)
    , m_units(0)
    , m_bytes(0)
    , m_vHandle(nullptr)
    , m_vClone(nullptr)
    , m_vStack(nullptr)
    , m_vLock(nullptr)
    , m_sampleInfo(nullptr)
    , m_payloadCaption(nullptr)
    , m_kindBox(nullptr)
    , m_count(nullptr)
    , m_payload(nullptr)
    , m_loops(nullptr)
    , m_show(nullptr)
    , m_refreshN(nullptr)
    , m_readers(nullptr)
    , m_writers(nullptr)
    , m_ops(nullptr)
    , m_log(nullptr)
    , m_btnBuild(nullptr)
    , m_btnCopy(nullptr)
    , m_btnRefresh(nullptr)
    , m_btnLock(nullptr)
{
    setStyleSheet(QStringLiteral(
        "QFrame#metric { background:#1b2330; border-radius:6px; }"
        "QLabel#metricTitle { color:#8b98a8; font-size:11px; }"
        "QLabel#metricValue { color:#f4f7fb; font-size:18px; font-weight:600; }"
        "QLabel#hint { color:#5c6770; }"
        "QLabel#sampleInfo { color:#1f2a33; }"
        "QPlainTextEdit#benchLog { background:#0f141c; color:#d5deea;"
        " font-family:Consolas,monospace; font-size:12px; border:none; border-radius:4px; }"
        "QPushButton#go { background:#2f6fed; color:white; border:none; border-radius:4px; padding:6px 10px; }"
        "QPushButton#go:disabled { background:#b7c3d6; color:#f7f8fa; }"));

    QVBoxLayout* left = new QVBoxLayout();
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(8);

    QHBoxLayout* metrics = new QHBoxLayout();
    metrics->setSpacing(8);
    metrics->addWidget(metricCard(this, L8("句柄拷贝"), &m_vHandle), 1);
    metrics->addWidget(metricCard(this, L8("元组 clone"), &m_vClone), 1);
    metrics->addWidget(metricCard(this, L8("Stack 刷新"), &m_vStack), 1);
    metrics->addWidget(metricCard(this, L8("多线程锁"), &m_vLock), 1);
    left->addLayout(metrics);

    QLabel* hint = new QLabel(L8(
        "句柄拷贝只共享元组。clone，以及 Region / Contour / Poly 的赋值，才复制行程和点列。"
        "刷新 Stack 时先 clone 要显示的那一段，再 setOverlays，避免和锁测试写同一份数据。"), this);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    left->addWidget(hint);

    m_api = new image_widget::ImageWidgetApi(this);
    m_api->setLayoutFillEnabled(true);
    m_api->setOverlayToolbarEnabled(false);
    m_api->setViewportBackground(HsCtrl::ViewportDark);
    if (m_api->imageWindow()) {
        m_api->imageWindow()->setWindowMode(HsCtrl::WindowRuntime);
    }
    left->addWidget(m_api->widget(), 1);

    QWidget* side = new QWidget(this);
    side->setMinimumWidth(320);
    side->setMaximumWidth(420);
    QVBoxLayout* sideLay = new QVBoxLayout(side);
    sideLay->setContentsMargins(0, 0, 0, 0);
    sideLay->setSpacing(8);

    m_sampleInfo = new QLabel(L8("还没有样本"), side);
    m_sampleInfo->setObjectName(QStringLiteral("sampleInfo"));
    m_sampleInfo->setWordWrap(true);
    sideLay->addWidget(m_sampleInfo);

    QFrame* sampleBox = new QFrame(side);
    QVBoxLayout* sampleLay = new QVBoxLayout(sampleBox);
    sampleLay->setContentsMargins(0, 0, 0, 0);
    sampleLay->addWidget(new QLabel(L8("样本"), sampleBox));
    m_kindBox = new QComboBox(sampleBox);
    m_kindBox->addItem(L8("Region 区域"), SampleRegion);
    m_kindBox->addItem(L8("Contour 轮廓"), SampleContour);
    m_kindBox->addItem(L8("Poly 折线"), SamplePoly);
    m_kindBox->addItem(L8("Circle 圆"), SampleCircle);
    m_kindBox->addItem(L8("混合"), SampleMixed);
    sampleLay->addWidget(m_kindBox);

    m_count = new QSpinBox(sampleBox);
    m_count->setRange(1, 200000);
    m_count->setValue(16);
    m_payload = new QSpinBox(sampleBox);
    m_payload->setRange(1, 500000);
    m_payload->setValue(2000);
    m_payloadCaption = new QLabel(payloadCaption(SampleRegion), sampleBox);
    sampleLay->addWidget(new QLabel(L8("对象个数"), sampleBox));
    sampleLay->addWidget(m_count);
    sampleLay->addWidget(m_payloadCaption);
    sampleLay->addWidget(m_payload);
    m_btnBuild = new QPushButton(L8("生成样本"), sampleBox);
    m_btnBuild->setObjectName(QStringLiteral("go"));
    sampleLay->addWidget(m_btnBuild);
    sideLay->addWidget(sampleBox);

    QFrame* copyBox = new QFrame(side);
    QVBoxLayout* copyLay = new QVBoxLayout(copyBox);
    copyLay->setContentsMargins(0, 0, 0, 0);
    copyLay->addWidget(new QLabel(L8("深拷贝"), copyBox));
    m_loops = new QSpinBox(copyBox);
    m_loops->setRange(1, 100);
    m_loops->setValue(3);
    QHBoxLayout* loopRow = new QHBoxLayout();
    loopRow->addWidget(new QLabel(L8("循环"), copyBox));
    loopRow->addWidget(m_loops, 1);
    copyLay->addLayout(loopRow);
    m_btnCopy = new QPushButton(L8("测句柄 / 元素 / clone"), copyBox);
    m_btnCopy->setObjectName(QStringLiteral("go"));
    copyLay->addWidget(m_btnCopy);
    sideLay->addWidget(copyBox);

    QFrame* stackBox = new QFrame(side);
    QVBoxLayout* stackLay = new QVBoxLayout(stackBox);
    stackLay->setContentsMargins(0, 0, 0, 0);
    stackLay->addWidget(new QLabel(L8("Stack 刷新"), stackBox));
    m_show = new QSpinBox(stackBox);
    m_show->setRange(1, 5000);
    m_show->setValue(16);
    m_refreshN = new QSpinBox(stackBox);
    m_refreshN->setRange(1, 30);
    m_refreshN->setValue(3);
    QHBoxLayout* showRow = new QHBoxLayout();
    showRow->addWidget(new QLabel(L8("显示个数"), stackBox));
    showRow->addWidget(m_show, 1);
    stackLay->addLayout(showRow);
    QHBoxLayout* refRow = new QHBoxLayout();
    refRow->addWidget(new QLabel(L8("重复次数"), stackBox));
    refRow->addWidget(m_refreshN, 1);
    stackLay->addLayout(refRow);
    m_btnRefresh = new QPushButton(L8("刷新 Stack"), stackBox);
    m_btnRefresh->setObjectName(QStringLiteral("go"));
    stackLay->addWidget(m_btnRefresh);
    sideLay->addWidget(stackBox);

    QFrame* lockBox = new QFrame(side);
    QVBoxLayout* lockLay = new QVBoxLayout(lockBox);
    lockLay->setContentsMargins(0, 0, 0, 0);
    lockLay->addWidget(new QLabel(L8("多线程锁"), lockBox));
    m_readers = new QSpinBox(lockBox);
    m_readers->setRange(1, 16);
    m_readers->setValue(4);
    m_writers = new QSpinBox(lockBox);
    m_writers->setRange(0, 8);
    m_writers->setValue(2);
    m_ops = new QSpinBox(lockBox);
    m_ops->setRange(1, 5000);
    m_ops->setValue(50);
    QHBoxLayout* rw = new QHBoxLayout();
    rw->addWidget(new QLabel(L8("读者"), lockBox));
    rw->addWidget(m_readers, 1);
    rw->addWidget(new QLabel(L8("写者"), lockBox));
    rw->addWidget(m_writers, 1);
    lockLay->addLayout(rw);
    QHBoxLayout* opRow = new QHBoxLayout();
    opRow->addWidget(new QLabel(L8("每线程次数"), lockBox));
    opRow->addWidget(m_ops, 1);
    lockLay->addLayout(opRow);
    m_btnLock = new QPushButton(L8("验证锁"), lockBox);
    m_btnLock->setObjectName(QStringLiteral("go"));
    lockLay->addWidget(m_btnLock);
    sideLay->addWidget(lockBox);

    m_log = new QPlainTextEdit(side);
    m_log->setObjectName(QStringLiteral("benchLog"));
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(400);
    sideLay->addWidget(m_log, 1);
    QPushButton* clearBtn = new QPushButton(L8("清空日志"), side);
    sideLay->addWidget(clearBtn);

    QSplitter* split = new QSplitter(Qt::Horizontal, this);
    QWidget* leftWrap = new QWidget(split);
    leftWrap->setLayout(left);
    split->addWidget(leftWrap);
    split->addWidget(side);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    QHBoxLayout* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->addWidget(split);

    m_thread = new QThread(this);
    m_worker = new ObjectWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, SIGNAL(finished()), m_worker, SLOT(deleteLater()));
    connect(m_worker, SIGNAL(sampleReady(int,qint64,qint64,int,int,int,int)),
            this, SLOT(onSampleReady(int,qint64,qint64,int,int,int,int)));
    connect(m_worker, SIGNAL(copyReady(int,int,int,double,double,double,bool)),
            this, SLOT(onCopyReady(int,int,int,double,double,double,bool)));
    connect(m_worker, SIGNAL(lockReady(bool,int,int,int,int,int,double,bool,bool)),
            this, SLOT(onLockReady(bool,int,int,int,int,int,double,bool,bool)));
    connect(m_worker, SIGNAL(taskFinished()), this, SLOT(onTaskFinished()));
    m_thread->start();

    connect(m_btnBuild, SIGNAL(clicked()), this, SLOT(onBuild()));
    connect(m_btnCopy, SIGNAL(clicked()), this, SLOT(onCopy()));
    connect(m_btnRefresh, SIGNAL(clicked()), this, SLOT(onRefresh()));
    connect(m_btnLock, SIGNAL(clicked()), this, SLOT(onLock()));
    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onClearLog()));
    connect(m_kindBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onKindChanged(int)));

    ensurePlate(960, 640);
}

ObjectPage::~ObjectPage()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
    if (m_api) {
        delete m_api;
        m_api = nullptr;
    }
}

void ObjectPage::setBusy(bool busy)
{
    m_busy = busy;
    const bool en = !busy;
    m_btnBuild->setEnabled(en);
    m_btnCopy->setEnabled(en);
    m_btnRefresh->setEnabled(en);
    m_btnLock->setEnabled(en);
}

void ObjectPage::appendLog(const QString& text)
{
    m_log->appendPlainText(text);
}

QString ObjectPage::kindText(int kind) const
{
    if (kind == SampleContour) return L8("Contour");
    if (kind == SamplePoly) return L8("Poly");
    if (kind == SampleCircle) return L8("Circle");
    if (kind == SampleMixed) return L8("混合");
    return L8("Region");
}

QString ObjectPage::payloadCaption(int kind) const
{
    if (kind == SampleContour) return L8("每条轮廓的点数");
    if (kind == SamplePoly) return L8("每条折线的段数");
    if (kind == SampleCircle) return L8("圆的半径（像素）");
    if (kind == SampleMixed) return L8("区域弦数 / 轮廓点数");
    return L8("每个区域的弦数");
}

void ObjectPage::onKindChanged(int)
{
    const int kind = m_kindBox->currentData().toInt();
    m_payloadCaption->setText(payloadCaption(kind));
}

void ObjectPage::ensurePlate(int w, int h)
{
    if (!m_api || w < 1 || h < 1) {
        return;
    }
    QImage plate(w, h, QImage::Format_RGB32);
    plate.fill(QColor(22, 26, 34));
    m_api->setImage(plate);
    m_api->fitToWindow();
}

void ObjectPage::onBuild()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    const int kind = m_kindBox->currentData().toInt();
    appendLog(L8("生成 %1，个数 %2，载荷 %3")
                  .arg(kindText(kind))
                  .arg(m_count->value())
                  .arg(m_payload->value()));
    QMetaObject::invokeMethod(m_worker, "buildSample", Qt::QueuedConnection,
                              Q_ARG(int, kind),
                              Q_ARG(int, m_count->value()),
                              Q_ARG(int, m_payload->value()));
}

void ObjectPage::onCopy()
{
    if (m_busy) {
        return;
    }
    if (!m_hasSample) {
        appendLog(L8("先生成样本"));
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(m_worker, "runCopy", Qt::QueuedConnection,
                              Q_ARG(int, m_loops->value()));
}

void ObjectPage::onLock()
{
    if (m_busy) {
        return;
    }
    if (!m_hasSample) {
        appendLog(L8("先生成样本"));
        return;
    }
    setBusy(true);
    appendLog(L8("锁测试：读者 %1，写者 %2，每线程 %3。写者只 append，不改第 0 个元素")
                  .arg(m_readers->value())
                  .arg(m_writers->value())
                  .arg(m_ops->value()));
    QMetaObject::invokeMethod(m_worker, "runLock", Qt::QueuedConnection,
                              Q_ARG(int, m_readers->value()),
                              Q_ARG(int, m_writers->value()),
                              Q_ARG(int, m_ops->value()));
}

void ObjectPage::onRefresh()
{
    if (m_busy || !m_api) {
        return;
    }
    if (!m_hasSample) {
        appendLog(L8("先生成样本"));
        return;
    }
    setBusy(true);
    HsCommData::HsObject view = m_worker->sample();
    const int cap = m_show->value();
    const int total = view.count();
    if (cap > 0 && total > cap) {
        view = view.slice(0, cap);
    }
    QElapsedTimer clock;
    clock.start();
    view = view.clone();
    const double msClone = clock.nsecsElapsed() / 1.0e6;

    HsCommData::Graphic g;
    g.kind = HsCommData::KindObject;
    g.role = HsCommData::RoleStack;
    g.geomStyle = HsCommData::Style::stackStyle();
    g.geomStyle.draw = HsCommData::DrawFill;
    g.geomStyle.fill = HsCommData::Color4(46, 204, 113, 80);
    g.geomStyle.stroke = HsCommData::Color4(46, 204, 113, 255);
    g.geom = view;
    g.frameId = m_api->currentFrameId();
    g.updateBounds();

    std::vector<HsCommData::Graphic> items(1, g);
    const int times = m_refreshN->value();
    clock.restart();
    for (int i = 0; i < times; ++i) {
        m_api->setOverlays(items);
    }
    const double ms = (clock.nsecsElapsed() / 1.0e6) / times;
    const qreal fps = m_api->fps();
    m_vStack->setText(formatMs(ms));
    appendLog(L8("Stack  显示 %1 / %2\n  显示用 clone %3\n  setOverlays 平均 %4（%5 次）  控件 fps %6")
                  .arg(view.count())
                  .arg(total)
                  .arg(formatMs(msClone))
                  .arg(formatMs(ms))
                  .arg(times)
                  .arg(fps, 0, 'f', 1));
    setBusy(false);
}

void ObjectPage::onSampleReady(int count, qint64 units, qint64 bytes, int w, int h, int fail, int errorCode)
{
    if (fail != 0) {
        appendLog(failText(fail, errorCode));
        return;
    }
    m_hasSample = true;
    m_kind = m_kindBox->currentData().toInt();
    m_units = units;
    m_bytes = bytes;
    m_sampleInfo->setText(L8("%1 · %2 个 · 载荷 %3 · 约 %4")
                              .arg(kindText(m_kind))
                              .arg(count)
                              .arg(units)
                              .arg(formatBytes(bytes)));
    ensurePlate(w, h);
    m_api->clearOverlays();
    appendLog(L8("样本就绪。%1").arg(m_sampleInfo->text()));
}

void ObjectPage::onCopyReady(int fail, int errorCode, int loops, double msHandle, double msElement, double msClone, bool isolated)
{
    if (fail != 0) {
        appendLog(failText(fail, errorCode));
        return;
    }
    m_vHandle->setText(formatMs(msHandle));
    m_vClone->setText(formatMs(msClone));
    const QString element = (msElement < 0) ? L8("混合类型不单测元素赋值") : formatMs(msElement);
    appendLog(L8("深拷贝  %1 次平均\n  句柄 %2\n  元素赋值 %3\n  clone %4\n  隔离 %5")
                  .arg(loops)
                  .arg(formatMs(msHandle))
                  .arg(element)
                  .arg(formatMs(msClone))
                  .arg(isolated ? L8("通过：clone 看不到随后的写入") : L8("失败：clone 被后面的写入改到了")));
}

void ObjectPage::onLockReady(bool ok, int errors, int readerOps, int writerOps, int startCount, int finalCount, double ms, bool countOk, bool cloneStable)
{
    m_vLock->setText(ok ? L8("通过") : L8("失败"));
    m_vLock->setStyleSheet(ok ? QStringLiteral("color:#3dd68c;") : QStringLiteral("color:#ff6b6b;"));
    const int expect = startCount + writerOps;
    appendLog(L8("锁  %1  %2\n  读者操作 %3，写者 append %4\n  个数 %5 -> %6，期望 %7（%8）\n  第 0 个元素的 clone %9\n  异常次数 %10")
                  .arg(ok ? L8("通过") : L8("失败"))
                  .arg(formatMs(ms))
                  .arg(readerOps)
                  .arg(writerOps)
                  .arg(startCount)
                  .arg(finalCount)
                  .arg(expect)
                  .arg(countOk ? L8("一致") : L8("不一致"))
                  .arg(cloneStable ? L8("未被改写") : L8("被改写"))
                  .arg(errors));
    if (m_hasSample) {
        m_sampleInfo->setText(L8("%1 · 现有 %2 个（含锁测试 append 的圆）· 约 %3")
                                  .arg(kindText(m_kind))
                                  .arg(finalCount)
                                  .arg(formatBytes(m_bytes)));
    }
}

void ObjectPage::onTaskFinished()
{
    setBusy(false);
}

void ObjectPage::onClearLog()
{
    m_log->clear();
}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#include "moc_ObjectPage.cpp"
#endif
