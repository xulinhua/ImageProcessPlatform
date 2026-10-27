#ifndef HSCOMMDATA_HSCOMMDATA_H
#define HSCOMMDATA_HSCOMMDATA_H

/**
 * @file HsCommData.h
 * @brief Umbrella header for geometry and image protocol. No Qt.
 * Define HSCOMMDATA_LIB when building the DLL.
 * Leaf headers include HsCommDataExport.h only.
 */

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
