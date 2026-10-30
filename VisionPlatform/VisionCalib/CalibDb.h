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
 * @note 进程内单例，全部写操作在一把全局锁里串行
 */
class CalibDb {
public:
    /** @brief 进程内单例 */
    static CalibDb& instance();

    /**
     * @brief 追加一条内参记录（applied 记 0）
     * @param halconSrc Halcon 相机参数文件，存在时拷到 plate_<id> 目录并回填路径；可空
     * @param offlineDir 离线图目录，其中文件全部拷到 plate_<id> 下的 offline 子目录并逐张登记；可空
     * @param id 新行 id，成功时才写；可空
     * @return 主表、路径回填和图片登记在一个事务里，全部写入返回 true；任一条写库失败整体回滚、
     *         删掉已拷的文件并返回 false。文件拷贝失败只跳过该文件，不影响返回值
     */
    bool recordPlate(const std::string& solutionDir, const std::string& cameraId,
                     const std::string& cameraName, int imageW, int imageH,
                     const CalibParam& param, const CamModel& model,
                     const std::string& halconSrc, const std::string& offlineDir,
                     int64_t* id, std::string* error);
    /**
     * @brief 追加一条平面映射记录及其样本点；父记录取该相机最近一条有效内参
     * @param param 当前未使用
     * @param id 新行 id，成功时才写；可空
     * @return 主表与样本点在一个事务里写入，任一条失败整体回滚并返回 false
     */
    bool recordPlane(const std::string& solutionDir, const std::string& cameraId,
                     const std::string& cameraName, const CalibParam& param,
                     const PlaneMap& model, int64_t* id, std::string* error);
    /**
     * @brief 追加一条旋转中心记录及其角度样本；父记录取该相机最近一条有效平面映射
     * @param param 当前未使用
     * @param id 新行 id，成功时才写；可空
     * @return 主表与角度样本在一个事务里写入，任一条失败整体回滚并返回 false
     */
    bool recordRotate(const std::string& solutionDir, const std::string& cameraId,
                      const std::string& cameraName, const CalibParam& param,
                      const RotateCenter& model, int64_t* id, std::string* error);
    /**
     * @brief 把某表某行的 applied 置 1
     * @param table plate、plane 或 rotate，须是安全标识符
     * @return id 非正、表名不安全、库打不开或执行失败返回 false
     */
    bool markApplied(const char* table, int64_t id);

private:
    /** @brief 只经 instance 取得 */
    CalibDb() {}
    /** @brief 打开 calib.db，版本低于 1 时建表；调用方须已持锁 */
    bool open(std::string* error);
};

}

#endif
