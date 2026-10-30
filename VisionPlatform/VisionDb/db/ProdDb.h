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
#include <functional>
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

    /** @brief 数值字段清零，ok 为 0（不合格） */
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
    /** @brief 写完队列后停写线程。此时仍写不进去的批次只再试一次，失败记入 lastError */
    void shutdown();
    /**
     * @brief 一拍结束之后入队；写线程未启动时先启动。不在工站锁里调用
     * @note 队列不设上限，不会阻塞；stationId 为空的拍会被写线程丢弃
     */
    void note(const ProdShot& shot);
    /**
     * @brief 写线程最近一次放弃整批的原因；从未放弃为空
     * @note 一批写入失败会整批回滚并隔 1 秒重试，重试用尽或停止时仍失败才放弃
     */
    std::string lastError() const;
    /** @brief 累计放弃的拍数，0 表示没有丢过数据 */
    int64_t droppedShots() const;
    /**
     * @brief 设置放弃整批时的通知；传空函数取消
     * @param handler 参数为放弃原因和这批的拍数。在写线程或 shutdown 的调用线程里调，不持有库内锁
     * @note VisionDb 不能依赖 VisionLog，由日志模块在启动时挂上，把丢数据写进系统日志
     */
    void setDropHandler(const std::function<void(const std::string&, int64_t)>& handler);

private:
    /** @brief 单例私有构造 */
    ProdDb();
    /** @brief 单例私有析构 */
    ~ProdDb();
    /** @brief 禁止拷贝（只声明不实现） */
    ProdDb(const ProdDb&);
    /** @brief 禁止赋值（只声明不实现） */
    ProdDb& operator=(const ProdDb&);

    struct Impl; ///< 写线程、队列与连接的私有实现
    Impl* m;     ///< 私有实现，本类拥有
};

}

#endif
