#ifndef VISION_OPEN3DNORMALS_H
#define VISION_OPEN3DNORMALS_H
#include "AlgToolBase.h"
namespace Vision {
class Open3dNormals : public AlgToolBase {
public:
    std::string toolName() const { return "prepare3d.normals"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dNormals(); }
};
}
#endif