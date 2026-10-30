/**
 * @file CalibTypes.h
 * @brief CalibTypes头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIB_TYPES_H
#define VISION_CALIB_TYPES_H
#include "VisionCalibExport.h"

#include "ObjectId.h"
#include <string>
#include <vector>

namespace Vision {

/** @brief 标定种类。按相机各存一份生效结果 */
enum CalibKind {
    CalibKindUndefined = -1, ///< 未定义
    CalibKindPlate = 0,  ///< 相机模型（标定板内参）
    CalibKindPlane = 1,  ///< 平面映射（九点、比例尺）
    CalibKindRotate = 2,  ///< 旋转中心
    CalibKindMax ///< 个数，不是合法值
};

/** @brief 结果是否可供运行使用 */
enum CalibStatus {
    CalibDraft = 0,    ///< 草稿，运行时不用
    CalibValid = 1,    ///< 生效
    CalibExpired = 2   ///< 已被替换或与父结果对不上
};

/** @brief 样本从哪来 */
enum CalibSource {
    CalibSourceUndefined = -1, ///< 未定义
    CalibOnline = 0,        ///< 在线采集
    CalibOfflineImage = 1,  ///< 离线图片
    CalibOfflineTable = 2,  ///< 离线点表
    CalibImport = 3,         ///< 导入已有结果
    CalibSourceMax ///< 个数，不是合法值
};

/** @brief 标定板类型 */
enum CalibPlateKind {
    CalibPlateKindUndefined = -1, ///< 未定义
    CalibPlateCheckerboard = 0, ///< 棋盘格
    CalibPlateCircleGrid = 1,   ///< 圆点阵
    CalibPlateHalconCaltab = 2,  ///< Halcon 描述文件
    CalibPlateKindMax ///< 个数，不是合法值
};

/** @brief 平面映射拟合 */
enum CalibPlaneFit {
    CalibFitScale = 0,       ///< 比例尺（相似变换）
    CalibFitAffine = 1,      ///< 仿射，九点默认
    CalibFitHomography = 2   ///< 透视
};

/**
 * @class CalibParam
 * @brief 一路相机的标定参数。标定之前就可以填写
 */
struct VISION_CALIB_API CalibParam {
    CalibPlateKind plateKind; ///< 板型
    int rows;                 ///< 内角点或圆心行数
    int cols;                 ///< 内角点或圆心列数
    double spacingMm;         ///< 格距或圆心距，毫米
    double thicknessMm;       ///< 板厚，毫米
    CalibPlaneFit planeFit;   ///< 九点用哪种拟合
    double residualMaxPx;     ///< 像素残差上限
    double residualMaxMm;     ///< 毫米残差上限
    double angleRefDeg;       ///< 旋转补偿的示教角，度
    int minPlateImages;       ///< 标定板最少张数
    int minRotateAngles;      ///< 旋转中心最少角度数

    /** @brief 板型未定义，间距 1 毫米，仿射拟合，残差上限 0.5，至少 10 张图、3 个角度 */
    CalibParam();
};

/**
 * @class CamModel
 * @brief 相机内参与畸变。系数共用，不绑算法库句柄
 */
struct VISION_CALIB_API CamModel {
    bool present;             ///< 是否有这份结果
    CalibStatus status;       ///< 草稿、生效或失效
    CalibSource source;       ///< 来源
    double rms;               ///< 重投影均方根，像素
    double maxResidual;       ///< 最大残差，像素
    std::string updatedAt;    ///< 更新时间
    double fx;                ///< 焦距 x ，像素
    double fy;                ///< 焦距 y ，像素
    double cx;                ///< 主点 x
    double cy;                ///< 主点 y
    double k1;                ///< 径向畸变
    double k2;                ///< 径向畸变
    double k3;                ///< 径向畸变
    double p1;                ///< 切向畸变
    double p2;                ///< 切向畸变
    CalibPlateKind plateKind; ///< 标定时的板型
    int rows;                 ///< 标定时的行数
    int cols;                 ///< 标定时的列数
    double spacingMm;         ///< 标定时的间距
    int count;                ///< 实际采用的图像张数

    /** @brief 空模型：不存在、草稿，系数全 0 */
    CamModel();
};

/**
 * @class PlaneSample
 * @brief 九点或比例尺的一对点
 */
struct VISION_CALIB_API PlaneSample {
    double imageX; ///< 像素 x
    double imageY; ///< 像素 y
    double worldX; ///< 毫米 x
    double worldY; ///< 毫米 y
    bool used;     ///< 参与拟合

    /** @brief 坐标全 0，默认参与拟合 */
    PlaneSample();
};

/**
 * @class PlaneMap
 * @brief 像素到工作平面毫米的 3x3 映射，行主序
 */
struct VISION_CALIB_API PlaneMap {
    bool present;                    ///< 是否有这份结果
    CalibStatus status;              ///< 草稿、生效或失效
    CalibSource source;              ///< 来源
    double rms;                      ///< 毫米残差均方根
    double maxResidual;              ///< 最大毫米残差
    std::string updatedAt;           ///< 更新时间
    CalibPlaneFit fit;               ///< 拟合类型
    double imageToWorld[9];          ///< 像素齐次到毫米齐次
    double worldToImage[9];          ///< 逆矩阵
    double mmPerPixel;               ///< 仅比例尺有意义
    std::string parentModelStamp;    ///< 标定时相机模型的更新时间
    std::vector<PlaneSample> samples; ///< 点对

    /** @brief 空映射：不存在、草稿、仿射，两个矩阵都是单位阵 */
    PlaneMap();
};

/**
 * @class RotateSample
 * @brief 旋转中心的一个角度样本
 */
struct VISION_CALIB_API RotateSample {
    double angleDeg; ///< 平台角度，度
    double imageX;   ///< 标记像素 x
    double imageY;   ///< 标记像素 y

    /** @brief 角度与坐标全 0 */
    RotateSample();
};

/**
 * @class RotateCenter
 * @brief 旋转中心。有平面映射时同时留下毫米圆心
 */
struct VISION_CALIB_API RotateCenter {
    bool present;                  ///< 是否有这份结果
    CalibStatus status;            ///< 草稿、生效或失效
    CalibSource source;            ///< 来源
    double rms;                    ///< 点到圆的像素均方根
    double maxResidual;            ///< 最大像素偏差
    std::string updatedAt;         ///< 更新时间
    double imageX;                 ///< 像素圆心 x
    double imageY;                 ///< 像素圆心 y
    double worldX;                 ///< 毫米圆心 x
    double worldY;                 ///< 毫米圆心 y
    bool worldValid;               ///< 毫米圆心是否有效
    double radius;                 ///< 拟合半径，像素
    std::string parentPlaneStamp;  ///< 标定时平面映射的更新时间
    std::vector<RotateSample> samples; ///< 样本

    /** @brief 空结果：不存在、草稿，毫米圆心无效 */
    RotateCenter();
};

/**
 * @class CalibRst
 * @brief 一路相机当前三份结果
 */
struct VISION_CALIB_API CalibRst {
    CamModel camera;  ///< 相机模型
    PlaneMap plane;      ///< 平面映射
    RotateCenter rotate; ///< 旋转中心

    /** @brief 三份都是空结果 */
    CalibRst();
};

/**
 * @class CalibHistItem
 * @brief 被替换下来的一条旧结果
 */
struct VISION_CALIB_API CalibHistItem {
    CalibKind kind;        ///< 种类
    std::string fileName;  ///< history 目录下的文件名
    std::string updatedAt; ///< 当时的更新时间
    CalibStatus status;    ///< 一般是失效

    /** @brief 种类未定义，状态为失效 */
    CalibHistItem();
};

/**
 * @class CalibPose
 * @brief 像素或毫米位姿。角度为度，逆时针为正
 */
struct VISION_CALIB_API CalibPose {
    double x;     ///< x
    double y;     ///< y
    double angle; ///< 度

    /** @brief 零位姿 */
    CalibPose();
};

}

#endif
