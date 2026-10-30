/**
 * @file HsCommData.h
 * @brief 几何与图像协议的总头文件，不依赖 Qt。编 DLL 时定义 HSCOMMDATA_LIB；
 *        各叶子头文件只包含 HsCommDataExport.h
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSCOMMDATA_H
#define HSCOMMDATA_HSCOMMDATA_H

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/HsException.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

#include <HsCommData/Geom.h>
#include <HsCommData/Shape.h>
#include <HsCommData/MeasureCalib.h>
#include <HsCommData/HsObject.h>
#include <HsCommData/FrameOverlay.h>
#include <HsCommData/HsConst.h>
#include <HsCommData/HsSystem.h>
#include <HsCommData/HsInterface2D.h>
#include <HsCommData/HsInterface3D.h>
#include <HsCommData/Shape3.h>
#include <HsCommData/HsCloud.h>
#include <HsCommData/HsMesh.h>
#include <HsCommData/HsContour3.h>
#include <HsCommData/HsTuple.h>
#include <HsCommData/HsMat.h>

#endif
