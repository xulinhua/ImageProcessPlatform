/**
 * @file CalibJson.h
 * @brief CalibJson头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIB_JSON_H
#define VISION_CALIB_JSON_H
#include "CalibTypes.h"
#include "Json.h"
#include <string>

namespace Vision {

/** @brief 读整个文本文件；path 为 UTF-8 */
bool calibReadText(const std::string& path, std::string* text);
/** @brief 写文本文件；父目录不存在则创建 */
bool calibWriteText(const std::string& path, const std::string& text);

/** @brief 标定参数写成 JSON 对象 */
JsonValue paramToJson(const std::string& name, int imageWidth, int imageHeight, const CalibParam& p);
/**
 * @brief 从 JSON 对象填标定参数
 * @return 不是对象时返回 false
 */
bool paramFromJson(const JsonValue& j, std::string* name, int* imageWidth, int* imageHeight, CalibParam* p);

/** @brief 相机模型写成 JSON 对象 */
JsonValue cameraToJson(const CamModel& m);
/** @brief 从 JSON 对象填相机模型；缺 fx 返回 false */
bool cameraFromJson(const JsonValue& j, CamModel* m);

/** @brief 平面映射写成 JSON 对象 */
JsonValue planeToJson(const PlaneMap& m);
/** @brief 从 JSON 对象填平面映射；缺矩阵返回 false */
bool planeFromJson(const JsonValue& j, PlaneMap* m);

/** @brief 旋转中心写成 JSON 对象 */
JsonValue rotateToJson(const RotateCenter& m);
/** @brief 从 JSON 对象填旋转中心；缺半径返回 false */
bool rotateFromJson(const JsonValue& j, RotateCenter* m);

}

#endif