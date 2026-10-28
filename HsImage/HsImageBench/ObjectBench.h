/**
 * @file ObjectBench.h
 * @brief ObjectBench头文件
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
    bool ok;
    int fail;
    int errorCode;
    int count;
    int64_t payloadUnits;
    int64_t bytes;
    int imageW;
    int imageH;

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
    bool ok;
    int fail;
    int errorCode;
    int loops;
    double msHandle;
    double msElement;
    double msClone;
    bool isolated;

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
    bool ok;
    int errors;
    int readerOps;
    int writerOps;
    int startCount;
    int finalCount;
    int expectedCount;
    double ms;
    bool countOk;
    bool cloneStable;

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
 * @param count 元组个数
 * @param payload 弦、点、段或半径
 */
ObjectSampleStats fillSample(HsCommData::HsObject* dst, int kind, int count, int payload);

/** @brief 计时句柄拷贝、元素深拷、clone，并检查 clone 与随后的写入隔离 */
CopyTimings benchCopy(const HsCommData::HsObject& src, int loops);

/**
 * @brief 读者拷贝句柄并偶尔 clone 第 0 个；写者 append 小圆
 * @param shared 被测对象，写者会变长
 */
LockTimings benchLock(HsCommData::HsObject* shared, int readers, int writers, int opsEach);

#endif
