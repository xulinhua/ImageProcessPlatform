/**
 * @file ProdDb.h
 * @brief 生产库 data\prod.db。按批次分表，跨月再加月份
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PRODDB_H
#define VISION_PRODDB_H
#include "VisionDbExport.h"

#include <stdint.h>
#include <string>

namespace Vision {

/**
 * @class ProdShot
 * @brief 生产一拍。测量值和多文件以后再挂，这一拍先记下追溯抬头
 */
struct VISION_DB_API ProdShot {
    std::string timeIso;   ///< 本地时间，空则由库填当前时间
    std::string solution;  ///< 方案名
    std::string stationId; ///< 工站
    std::string jobId;     ///< 流程，可空
    int64_t frameNo;       ///< 帧号
    std::string barcode;   ///< 条码，可空
    int ok;                ///< 1 合格，0 不合格
    std::string summary;   ///< 摘要
    int elapsedMs;         ///< 流程耗时
    int cycleMs;           ///< 节拍
    std::string imagePath; ///< 图像路径，可空

    ProdShot()
        : frameNo(0)
        , ok(0)
        , elapsedMs(0)
        , cycleMs(0)
    {
    }
};

/**
 * @class ProdDb
 * @brief 生产库。节拍线程只入队，单独的写线程落库
 */
class VISION_DB_API ProdDb {
public:
    /** @brief 进程内唯一生产库 */
    static ProdDb& instance();
    /** @brief 建库并启动写线程 */
    void startup();
    /** @brief 写完队列后停写线程 */
    void shutdown();
    /** @brief 一拍结束之后入队。队列满时等待，不在工站锁里调用 */
    void note(const ProdShot& shot);

private:
    ProdDb();
    ~ProdDb();
    ProdDb(const ProdDb&);
    ProdDb& operator=(const ProdDb&);

    struct Impl;
    Impl* m;
};

}

#endif
