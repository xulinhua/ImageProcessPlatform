#include "Open3dFitSphere.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_SPHERE_USE_OPEN3D 1
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 拟合球" } };
}
std::vector<VerNote> Open3dFitSphere::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dFitSphere::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) {
        return HS_ERR_NULL;
    }
    *out = AlgToolOut();
#if !defined(VISION_SPHERE_USE_OPEN3D)
    (void)in;
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    (void)in;
    // Open3D 无现成球体 RANSAC API 时与缺库同等处理，避免壳侧再造一套算法
    return fail(HS_ERR_OPERATOR, "Open3D 未实现", out);
#endif
}
}
