/**
 * @file ObjectPage.h
 * @brief 结构与锁评测页及其工作线程对象
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
    /** @brief 初始无样本，由 ObjectPage 创建后移入工作线程 */
    explicit ObjectWorker(QObject* parent = 0);

    /** @brief 当前样本的句柄拷贝。只在任务结束后从界面线程调用 */
    HsCommData::HsObject sample() const;

    /************************ 任务 ************************/
public slots:
    /** @brief 按参数重建样本（见 fillSample），发 sampleReady 后发 taskFinished */
    void buildSample(int kind, int count, int payload);
    /** @brief 对当前样本跑 benchCopy，发 copyReady 后发 taskFinished */
    void runCopy(int loops);
    /** @brief 对当前样本跑 benchLock（写者会让样本变长），发 lockReady 后发 taskFinished */
    void runLock(int readers, int writers, int opsEach);

    /************************ 信号 ************************/
signals:
    /** @brief 样本生成结果，字段同 ObjectSampleStats；fail 非 0 表示失败 */
    void sampleReady(int count, qint64 units, qint64 bytes, int w, int h, int fail, int errorCode);
    /** @brief 拷贝计时结果，字段同 CopyTimings */
    void copyReady(int fail, int errorCode, int loops, double msHandle, double msElement, double msClone, bool isolated);
    /** @brief 锁测试结果，字段同 LockTimings */
    void lockReady(bool ok, int errors, int readerOps, int writerOps, int startCount, int finalCount, double ms, bool countOk, bool cloneStable);
    /** @brief 任意任务结束，界面据此解除忙状态 */
    void taskFinished();

private:
    HsCommData::HsObject m_sample; ///< 当前样本；工作线程读写，界面线程只在空闲时经 sample() 取句柄
};

/**
 * @class ObjectPage
 * @brief 结构评测页：深拷、Stack 刷新、多线程锁
 */
class ObjectPage : public QWidget {
    Q_OBJECT
public:
    /** @brief 搭建界面、启动工作线程，并铺一张 960×640 深色底图 */
    explicit ObjectPage(QWidget* parent = 0);
    /** @brief 停止工作线程（最多等 5 秒）并释放图像控件 */
    ~ObjectPage();

    /************************ 按钮 ************************/
private slots:
    /** @brief 按所选种类、个数、载荷在工作线程生成样本；忙时忽略 */
    void onBuild();
    /** @brief 在工作线程测句柄/元素/clone 拷贝；无样本时只提示 */
    void onCopy();
    /** @brief 在界面线程取前 N 个 clone 后重复 setOverlays，计 Stack 刷新耗时 */
    void onRefresh();
    /** @brief 在工作线程跑多线程读写锁测试；无样本时只提示 */
    void onLock();
    /** @brief 清空日志 */
    void onClearLog();
    /** @brief 样本种类切换时更新载荷标签；index 未用，取下拉框当前 data */
    void onKindChanged(int index);

    /************************ 结果 ************************/
    /** @brief 记录样本规模、更新样本信息并清空叠加层；失败只写日志 */
    void onSampleReady(int count, qint64 units, qint64 bytes, int w, int h, int fail, int errorCode);
    /** @brief 显示句柄拷贝与 clone 耗时，日志写明细和隔离结论；失败只写日志 */
    void onCopyReady(int fail, int errorCode, int loops, double msHandle, double msElement, double msClone, bool isolated);
    /** @brief 显示锁测试通过/失败，日志写明细，并刷新样本现有个数 */
    void onLockReady(bool ok, int errors, int readerOps, int writerOps, int startCount, int finalCount, double ms, bool countOk, bool cloneStable);
    /** @brief 工作线程任务结束，解除忙状态 */
    void onTaskFinished();

    /************************ 辅助 ************************/
private:
    /** @brief 记录忙状态并启用/禁用四个任务按钮 */
    void setBusy(bool busy);
    /** @brief 追加一段日志 */
    void appendLog(const QString& text);
    /** @brief 显示 w×h 深色底图并适应窗口；控件为空或尺寸非法时不处理 */
    void ensurePlate(int w, int h);
    /** @brief 样本种类的显示名 */
    QString kindText(int kind) const;
    /** @brief 该种类下载荷输入框的说明文字 */
    QString payloadCaption(int kind) const;

    image_widget::ImageWidgetApi* m_api; ///< 显示底图与 Stack 叠加层，析构时释放
    ObjectWorker* m_worker;              ///< 工作对象，线程结束时 deleteLater
    QThread* m_thread;                   ///< 工作线程，析构时退出
    bool m_busy;                         ///< 有任务在跑，按钮槽据此忽略点击
    bool m_hasSample;                    ///< 至少成功生成过一次样本
    int m_kind;                          ///< 最近样本的种类（ObjectSampleKind），取自完成时的下拉框
    qint64 m_units;                      ///< 最近样本的载荷总量
    qint64 m_bytes;                      ///< 最近样本的估算字节数，锁测试后不更新

    QLabel* m_vHandle;         ///< 指标卡：句柄拷贝耗时
    QLabel* m_vClone;          ///< 指标卡：元组 clone 耗时
    QLabel* m_vStack;          ///< 指标卡：单次 setOverlays 平均耗时
    QLabel* m_vLock;           ///< 指标卡：锁测试通过/失败
    QLabel* m_sampleInfo;      ///< 当前样本种类、个数、载荷、大小
    QLabel* m_payloadCaption;  ///< 载荷输入框的说明，随种类变化
    QComboBox* m_kindBox;      ///< 样本种类，item data 为 ObjectSampleKind
    QSpinBox* m_count;         ///< 对象个数（1~200000）
    QSpinBox* m_payload;       ///< 载荷：弦/点/段数或半径（1~500000）
    QSpinBox* m_loops;         ///< 拷贝计时循环次数（1~100）
    QSpinBox* m_show;          ///< Stack 显示前多少个对象（1~5000）
    QSpinBox* m_refreshN;      ///< setOverlays 重复次数（1~30）
    QSpinBox* m_readers;       ///< 读线程数（1~16）
    QSpinBox* m_writers;       ///< 写线程数（0~8）
    QSpinBox* m_ops;           ///< 每线程操作次数（1~5000）
    QPlainTextEdit* m_log;     ///< 日志框，最多 400 行
    QPushButton* m_btnBuild;   ///< 生成样本
    QPushButton* m_btnCopy;    ///< 测拷贝
    QPushButton* m_btnRefresh; ///< 刷新 Stack
    QPushButton* m_btnLock;    ///< 验证锁
};

#endif
