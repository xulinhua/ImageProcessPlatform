/**
 * @file CalibTypes.cpp
 * @brief CalibTypes实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibTypes.h"

namespace Vision {

// 默认标定参数
CalibParam::CalibParam()
    : plateKind(CalibPlateKindUndefined)
    , rows(0)
    , cols(0)
    , spacingMm(1.0)
    , thicknessMm(0.0)
    , planeFit(CalibFitAffine)
    , residualMaxPx(0.5)
    , residualMaxMm(0.5)
    , angleRefDeg(0.0)
    , minPlateImages(10)
    , minRotateAngles(3)
{}

// 空的相机模型
CamModel::CamModel()
    : present(false)
    , status(CalibDraft)
    , source(CalibSourceUndefined)
    , rms(0.0)
    , maxResidual(0.0)
    , fx(0.0)
    , fy(0.0)
    , cx(0.0)
    , cy(0.0)
    , k1(0.0)
    , k2(0.0)
    , k3(0.0)
    , p1(0.0)
    , p2(0.0)
    , plateKind(CalibPlateKindUndefined)
    , rows(0)
    , cols(0)
    , spacingMm(0.0)
    , count(0)
{}

// 默认参与拟合的点对
PlaneSample::PlaneSample()
    : imageX(0.0)
    , imageY(0.0)
    , worldX(0.0)
    , worldY(0.0)
    , used(true)
{}

// 单位矩阵的空平面映射
PlaneMap::PlaneMap()
    : present(false)
    , status(CalibDraft)
    , source(CalibSourceUndefined)
    , rms(0.0)
    , maxResidual(0.0)
    , fit(CalibFitAffine)
    , mmPerPixel(0.0)
{
    for (int i = 0; i < 9; ++i) {
        imageToWorld[i] = 0.0;
        worldToImage[i] = 0.0;
    }
    imageToWorld[0] = imageToWorld[4] = imageToWorld[8] = 1.0;
    worldToImage[0] = worldToImage[4] = worldToImage[8] = 1.0;
}

// 空的旋转样本
RotateSample::RotateSample()
    : angleDeg(0.0)
    , imageX(0.0)
    , imageY(0.0)
{}

// 空的旋转中心
RotateCenter::RotateCenter()
    : present(false)
    , status(CalibDraft)
    , source(CalibSourceUndefined)
    , rms(0.0)
    , maxResidual(0.0)
    , imageX(0.0)
    , imageY(0.0)
    , worldX(0.0)
    , worldY(0.0)
    , worldValid(false)
    , radius(0.0)
{}

// 空的三份结果
CalibRst::CalibRst()
{}

// 空的历史条目
CalibHistItem::CalibHistItem()
    : kind(CalibKindUndefined)
    , status(CalibExpired)
{}

// 零位姿
CalibPose::CalibPose()
    : x(0.0)
    , y(0.0)
    , angle(0.0)
{}

}
