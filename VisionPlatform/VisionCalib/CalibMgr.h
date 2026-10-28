/**
 * @file CalibMgr.h
 * @brief CalibMgr头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIB_MGR_H
#define VISION_CALIB_MGR_H
#include "VisionCalibExport.h"

#include "CalibTypes.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class CalibMgr
 * @brief 标定管理。进程内单例，按相机保存参数和结果
 */
class VISION_CALIB_API CalibMgr {
public:
    /** @brief 进程内唯一实例 */
    static CalibMgr& instance();

    /**
     * @brief 从方案目录下的 calib 读入。目录不存在视为空
     * @param solutionDir 方案文件所在目录， UTF-8
     */
    bool load(const std::string& solutionDir, std::string* err);
    /** @brief 把内存中的参数和生效结果写回当前方案目录 */
    bool save(std::string* err);
    /** @brief 卸掉当前方案的标定，不删磁盘 */
    void unload();
    /** @brief 之后 save 写到这个方案目录。不重新读盘 */
    void setSolutionDir(const std::string& solutionDir);

    /** @brief 已有标定目录的相机 */
    std::vector<ObjectId> cameras() const;
    /** @brief 参数。没有这路相机时返回默认值 */
    CalibParam param(const ObjectId& cameraId) const;
    /** @brief 写参数。相机不存在则建一条 */
    bool setParam(const ObjectId& cameraId, const CalibParam& param, std::string* err);
    /** @brief 这路相机的显示名 */
    std::string cameraName(const ObjectId& cameraId) const;
    /** @brief 改显示名 */
    bool setCameraName(const ObjectId& cameraId, const std::string& name, std::string* err);
    /** @brief 标定针对的图像宽高。没有这路相机时写出 0 */
    void imageSize(const ObjectId& cameraId, int* width, int* height) const;
    /** @brief 标定针对的图像宽高。与已有结果不一致时把结果标成失效 */
    bool setImageSize(const ObjectId& cameraId, int width, int height, std::string* err);

    /** @brief 当前结果。运行时只使用状态为生效的模型 */
    CalibRst active(const ObjectId& cameraId) const;
    /** @brief 超阈值留下的草稿。运行时不用 */
    CalibRst draft(const ObjectId& cameraId) const;
    /** @brief 这路相机的离线图片目录。方案未打开时为空 */
    std::string offlineDir(const ObjectId& cameraId) const;
    /** @brief 读 camera.halcon.dat。没有则空串 */
    std::string readHalconDat(const ObjectId& cameraId) const;
    /** @brief 写 camera.halcon.dat */
    bool writeHalconDat(const ObjectId& cameraId, const std::string& text, std::string* err);
    /** @brief 同相机、同种类的历史文件 */
    std::vector<CalibHistItem> history(const ObjectId& cameraId, CalibKind kind) const;

    /** @brief 提交相机模型。残差超阈值则不替换当前生效结果 */
    bool commitCamera(const ObjectId& cameraId, CamModel model, std::string* err);
    /** @brief 提交平面映射 */
    bool commitPlane(const ObjectId& cameraId, PlaneMap model, std::string* err);
    /** @brief 提交旋转中心。若已有生效平面映射，会补毫米圆心 */
    bool commitRotate(const ObjectId& cameraId, RotateCenter model, std::string* err);

    /**
     * @brief 离线点表。每行 imageX,imageY,worldX,worldY ，按这路相机的 planeFit 拟合并提交
     * @param csvPath UTF-8 路径
     */
    bool importPointTable(const ObjectId& cameraId, const std::string& csvPath, std::string* err);
    /** @brief 离线导入一份结果 json （相机、平面或旋转） */
    bool importResult(const ObjectId& cameraId, const std::string& filePath, std::string* err);
    /** @brief 临时相机 id 改成台账里的设备 id ，目录一起改名 */
    bool bindCamera(const ObjectId& fromId, const ObjectId& deviceId, std::string* err);

    /**
     * @brief 用生效的平面映射和旋转中心把像素位姿换成毫米
     * @param out 没有生效平面映射时不改，并返回假
     */
    bool mapPose(const ObjectId& cameraId, const CalibPose& in, CalibPose* out, std::string* err) const;

private:
    CalibMgr();
    ~CalibMgr();
    CalibMgr(const CalibMgr&);
    CalibMgr& operator=(const CalibMgr&);

    /** @class Impl @brief 按相机存放的参数和结果 */
    struct Impl;
    Impl* m_impl; ///< 相机表
};

}

#endif