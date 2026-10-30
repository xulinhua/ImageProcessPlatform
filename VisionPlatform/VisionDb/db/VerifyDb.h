/**
 * @file VerifyDb.h
 * @brief 验证复核库 data\verify.db。按试验类型分表
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VERIFYDB_H
#define VISION_VERIFYDB_H
#include "VisionDbExport.h"

#include <stdint.h>
#include <string>

namespace Vision {

/**
 * @class VerifyStudy
 * @brief 一次专项试验的抬头。种类只接受 grr、repeat、reproduce、known_point
 */
struct VISION_DB_API VerifyStudy {
    std::string solution;  ///< 方案
    std::string stationId; ///< 工站
    std::string jobId;     ///< 流程
    std::string toolId;    ///< 工具
    std::string metric;    ///< 被测项
    std::string operatorName; ///< 操作员
    std::string specLow;   ///< 规格下限，文本数值
    std::string specHigh;  ///< 规格上限
    std::string startedAt; ///< 开始时间

    /** @brief 全部字段为空 */
    VerifyStudy() {}
};

/**
 * @class VerifyTrial
 * @brief 一次读数
 */
struct VISION_DB_API VerifyTrial {
    int64_t studyId;     ///< 抬头编号
    int sampleNo;        ///< 样件序号
    std::string appraiser; ///< 测量人
    int trialNo;         ///< 第几次
    std::string timeIso; ///< 时间
    std::string valueNum;  ///< 数值文本
    std::string valueText; ///< 文本结果
    std::string imagePath; ///< 图像路径
    int ok;              ///< 1 合格

    /** @brief 编号与序号清零，ok 为 0 */
    VerifyTrial()
        : studyId(0)
        , sampleNo(0)
        , trialNo(0)
        , ok(0)
    {
    }
};

/**
 * @class VerifyDb
 * @brief 验证复核。不写入生产库
 */
class VISION_DB_API VerifyDb {
public:
    /** @brief 进程内唯一验证库 */
    static VerifyDb& instance();
    /** @brief 打开并建好四类表 */
    bool open(std::string* error);
    /**
     * @brief 新开一次试验
     * @param kind grr、repeat、reproduce、known_point
     * @return 失败为 0
     */
    int64_t begin(const std::string& kind, const VerifyStudy& study, std::string* error);
    /** @brief 加一个样件。失败返回 0 */
    int64_t addSample(const std::string& kind, int64_t studyId, int sampleNo,
                      const std::string& barcode, const std::string& nominal, std::string* error);
    /** @brief 加一次读数 */
    bool addTrial(const std::string& kind, const VerifyTrial& trial, std::string* error);
    /** @brief 结束试验并写入结论 */
    bool finish(const std::string& kind, int64_t studyId, const std::string& summary, std::string* error);

private:
    /** @brief 单例私有构造 */
    VerifyDb() {}
};

}

#endif
