/**
 * @file ObjectPage.h
 * @brief ObjectPage头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_OBJECT_PAGE_H
#define HSIMAGE_OBJECT_PAGE_H

#include <HsCommData/HsCommData.h>
#include "ObjectBench.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;

namespace image_widget {
class ImageWidgetApi;
}

/**
 * @class ObjectWorker
 * @brief 生成、深拷、锁测试都在工作线程。不碰控件
 */
class ObjectWorker : public QObject {
    Q_OBJECT
public:
    explicit ObjectWorker(QObject* parent = 0);

    /** @brief 当前样本的句柄拷贝。只在任务结束后从界面线程调用 */
    HsCommData::HsObject sample() const;

public slots:
    void buildSample(int kind, int count, int payload);
    void runCopy(int loops);
    void runLock(int readers, int writers, int opsEach);

signals:
    void sampleReady(int count, qint64 units, qint64 bytes, int w, int h, int fail, int errorCode);
    void copyReady(int fail, int errorCode, int loops, double msHandle, double msElement, double msClone, bool isolated);
    void lockReady(bool ok, int errors, int readerOps, int writerOps, int startCount, int finalCount, double ms, bool countOk, bool cloneStable);
    void taskFinished();

private:
    HsCommData::HsObject m_sample;
};

/**
 * @class ObjectPage
 * @brief 结构评测页：深拷、Stack 刷新、多线程锁
 */
class ObjectPage : public QWidget {
    Q_OBJECT
public:
    explicit ObjectPage(QWidget* parent = 0);
    ~ObjectPage();

private slots:
    void onBuild();
    void onCopy();
    void onRefresh();
    void onLock();
    void onClearLog();
    void onKindChanged(int index);
    void onSampleReady(int count, qint64 units, qint64 bytes, int w, int h, int fail, int errorCode);
    void onCopyReady(int fail, int errorCode, int loops, double msHandle, double msElement, double msClone, bool isolated);
    void onLockReady(bool ok, int errors, int readerOps, int writerOps, int startCount, int finalCount, double ms, bool countOk, bool cloneStable);
    void onTaskFinished();

private:
    void setBusy(bool busy);
    void appendLog(const QString& text);
    void ensurePlate(int w, int h);
    QString kindText(int kind) const;
    QString payloadCaption(int kind) const;

    image_widget::ImageWidgetApi* m_api;
    ObjectWorker* m_worker;
    QThread* m_thread;
    bool m_busy;
    bool m_hasSample;
    int m_kind;
    qint64 m_units;
    qint64 m_bytes;

    QLabel* m_vHandle;
    QLabel* m_vClone;
    QLabel* m_vStack;
    QLabel* m_vLock;
    QLabel* m_sampleInfo;
    QLabel* m_payloadCaption;
    QComboBox* m_kindBox;
    QSpinBox* m_count;
    QSpinBox* m_payload;
    QSpinBox* m_loops;
    QSpinBox* m_show;
    QSpinBox* m_refreshN;
    QSpinBox* m_readers;
    QSpinBox* m_writers;
    QSpinBox* m_ops;
    QPlainTextEdit* m_log;
    QPushButton* m_btnBuild;
    QPushButton* m_btnCopy;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnLock;
};

#endif
