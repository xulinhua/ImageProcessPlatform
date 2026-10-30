#include "Open3dFitLine.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>

#if defined(__has_include)
#  if __has_include(<open3d/geometry/PointCloud.h>)
#    define VISION_O3_HAVE 1
#  endif
#endif

namespace Vision {
namespace {
const VerNoteRaw kVerNotes[] = { { "1.0.0.0", "Open3D 拟合直线" } };
}
std::vector<VerNote> Open3dFitLine::versionNotes() const { return verNotesFrom(kVerNotes, 1); }

int Open3dFitLine::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (!out) { return HS_ERR_NULL; }
    *out = AlgToolOut();
    (void)in;
#if !defined(VISION_O3_HAVE)
    return fail(HS_ERR_OPERATOR, "未安装 Open3D", out);
#else
    return fail(HS_ERR_OPERATOR, "Open3D 未实现", out);
#endif
}
}