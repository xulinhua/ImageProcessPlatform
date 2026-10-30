#ifndef VISION_PCLCLUSTER_H
#define VISION_PCLCLUSTER_H
#include "AlgToolBase.h"
namespace Vision {
class PclCluster : public AlgToolBase {
public:
    std::string toolName() const { return "detect3d.cluster"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclCluster(); }
};
}
#endif