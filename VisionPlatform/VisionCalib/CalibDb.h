/**
 * @file CalibDb.h
 * @brief 标定记录库 data\calib.db。运行仍读方案里的当前结果
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIBDB_H
#define VISION_CALIBDB_H

#include "CalibTypes.h"
#include <stdint.h>
#include <string>

namespace Vision {

/**
 * @class CalibDb
 * @brief 每次提交追加一行。applied 为 1 表示这次已经写进方案里的生效文件
 */
class CalibDb {
public:
    static CalibDb& instance();

    bool recordPlate(const std::string& solutionDir, const std::string& cameraId,
                     const std::string& cameraName, int imageW, int imageH,
                     const CalibParam& param, const CamModel& model,
                     const std::string& halconSrc, const std::string& offlineDir,
                     int64_t* id, std::string* error);
    bool recordPlane(const std::string& solutionDir, const std::string& cameraId,
                     const std::string& cameraName, const CalibParam& param,
                     const PlaneMap& model, int64_t* id, std::string* error);
    bool recordRotate(const std::string& solutionDir, const std::string& cameraId,
                      const std::string& cameraName, const CalibParam& param,
                      const RotateCenter& model, int64_t* id, std::string* error);
    bool markApplied(const char* table, int64_t id);

private:
    CalibDb() {}
    bool open(std::string* error);
};

}

#endif
