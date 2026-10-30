/**
 * @file ObjectBench.h
 * @brief HsObject 结构评测：样本生成、拷贝计时与多线程读写验证
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_OBJECT_BENCH_H
#define HSIMAGE_OBJECT_BENCH_H

#include <HsCommData/HsObject.h>

#include <cstdint>

/** @brief 样本种类。载荷的含义见 fillSample */
enum ObjectSampleKind {
    SampleRegion = 0,  ///< 每个元素一条区域，载荷为弦数
    SampleContour = 1, ///< 每个元素一条轮廓，载荷为点数
    SamplePoly = 2,    ///< 每个元素一条折线逼近，载荷为段数
    SampleCircle = 3,  ///< 每个元素一个圆，载荷为半径
    SampleMixed = 4    ///< 区域、轮廓、圆、线段轮流
};

/**
 * @class ObjectSampleStats
 * @brief 一次生成的规模。fail：0 成功，1 参数非法，2 超过 800MB，3 异常
 */
struct ObjectSampleStats {
    bool ok;              ///< 生成并写入 dst 成功
    int fail;             ///< 0 成功，1 参数非法，2 超过 800MB，3 异常
    int errorCode;        ///< fail 为 3 且是 HsException 时的错误码，否则 0
    int count;            ///< 实际生成的元组个数
    int64_t payloadUnits; ///< 载荷总量：弦/点/段数之和，圆和线段各计 1
    int64_t bytes;        ///< 按种类估算的元素数据字节数（不含容器开销）；fail 为 2 时也填
    int imageW;           ///< 底图宽（固定 960）
    int imageH;           ///< 底图高（固定 640）

    /** @brief 默认失败（fail=1），其余清零 */
    ObjectSampleStats()
        : ok(false)
        , fail(1)
        , errorCode(0)
        , count(0)
        , payloadUnits(0)
        , bytes(0)
        , imageW(0)
        , imageH(0)
    {
    }
};

/**
 * @class CopyTimings
 * @brief 句柄拷贝、单类元素赋值、元组 clone。fail：0 成功，1 空样本，2 异常
 */
struct CopyTimings {
    bool ok;          ///< 计时与隔离检查都跑完（不代表 isolated 为真）
    int fail;         ///< 0 成功，1 空样本或 loops<1，2 异常
    int errorCode;    ///< fail 为 2 且是 HsException 时的错误码，否则 0
    int loops;        ///< 循环次数，三项耗时均为单轮平均
    double msHandle;  ///< 整个对象句柄拷贝的单轮耗时（毫秒）
    double msElement; ///< 逐个元素赋值（深拷）的单轮耗时（毫秒）；混合类型为 -1
    double msClone;   ///< 整个对象 clone 的单轮耗时（毫秒）
    bool isolated;    ///< clone 出的副本未受随后对原句柄写入的影响

    /** @brief 默认失败（fail=1），msElement 为 -1 */
    CopyTimings()
        : ok(false)
        , fail(1)
        , errorCode(0)
        , loops(0)
        , msHandle(0)
        , msElement(-1)
        , msClone(0)
        , isolated(false)
    {
    }
};

/**
 * @class LockTimings
 * @brief 多线程同时拷贝句柄并 append。cloneStable 表示第 0 个元素的深拷没有被写线程改掉
 */
struct LockTimings {
    bool ok;            ///< 无异常、个数一致且 clone 稳定；参数非法时为 false
    int errors;         ///< 异常次数与读者看到越界个数的次数之和
    int readerOps;      ///< 读者完成的操作总数
    int writerOps;      ///< 写者成功 append 的总数
    int startCount;     ///< 开始时元组个数
    int finalCount;     ///< 结束时元组个数
    int expectedCount;  ///< 期望个数 = startCount + writerOps
    double ms;          ///< 全部线程从启动到 join 完的总耗时（毫秒）
    bool countOk;       ///< finalCount 等于 expectedCount
    bool cloneStable;   ///< 读者 clone 的第 0 个元素签名始终与开始时一致

    /** @brief 默认全部清零，ok 为 false */
    LockTimings()
        : ok(false)
        , errors(0)
        , readerOps(0)
        , writerOps(0)
        , startCount(0)
        , finalCount(0)
        , expectedCount(0)
        , ms(0)
        , countOk(false)
        , cloneStable(false)
    {
    }
};

/**
 * @brief 生成样本并写入 dst。句柄赋值，元素本身已是独立副本
 * @param dst 输出对象，失败时不改动
 * @param kind ObjectSampleKind 取值
 * @param count 元组个数
 * @param payload 弦、点、段或半径
 */
ObjectSampleStats fillSample(HsCommData::HsObject* dst, int kind, int count, int payload);

/**
 * @brief 计时句柄拷贝、元素深拷、clone，并检查 clone 与随后的写入隔离
 * @param src 被测样本，不会被修改（写入只作用于拆开共享后的副本）
 * @param loops 循环次数，至少 1
 */
CopyTimings benchCopy(const HsCommData::HsObject& src, int loops);

/**
 * @brief 读者拷贝句柄并每 32 次 clone 第 0 个；写者 append 小圆
 * @param shared 被测对象，写者会变长
 * @param readers 读线程数，至少 1
 * @param writers 写线程数，可为 0
 * @param opsEach 每个线程的操作次数，至少 1
 */
LockTimings benchLock(HsCommData::HsObject* shared, int readers, int writers, int opsEach);

#endif
