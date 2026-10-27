/**
 * @file HsArrow.h
 * @brief HsArrowÍ·ÎÄ¼þ
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSARROW_H
#define HSCOMMDATA_HSARROW_H

#include <HsCommData/HsCommDataExport.h>
#include <HsCommData/Geom.h>

namespace HsCommData {

/**
 * @class HsArrow
 * @brief Arrow marker. Tail, head and head size stay together.
 */
struct HSCOMMDATA_EXPORT HsArrow {
    Point2d tail;      ///< tail
    Point2d head;      ///< head
    double headLen;   ///< head length in pixels; <0 uses body/4 capped at 16
    double headWidth; ///< half width in pixels; <0 uses open angle 0.4 rad

    HsArrow(); ///< headLen/headWidth <0 for defaults

    /**
     * @brief compute wing endpoints
     * @return true if both ends written
     */
    bool wings(Point2d* left, Point2d* right) const;
};

}

#endif
