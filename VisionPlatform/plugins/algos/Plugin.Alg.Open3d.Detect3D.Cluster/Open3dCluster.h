#ifndef VISION_OPEN3DCLUSTER_H
#define VISION_OPEN3DCLUSTER_H
#include "AlgToolBase.h"
namespace Vision {
class Open3dCluster : public AlgToolBase {
public:
    std::string toolName() const { return "detect3d.cluster"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dCluster(); }
};
}
#endif