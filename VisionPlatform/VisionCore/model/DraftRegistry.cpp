/**
 * @file DraftRegistry.cpp
 * @brief DraftRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DraftRegistry.h"
#include "Solution.h"
#include <map>

namespace Vision {

namespace {

struct DraftState {
    bool open;   ///< 配置页是否打开
    bool dirty;  ///< 是否有未保存修改
    DraftState() : open(false), dirty(false) {}
};

typedef std::map<std::string, DraftState> DraftMap;

// 工具草稿的打开与脏标记
DraftMap& toolDrafts()
{
    static DraftMap g;
    return g;
}

// 流程草稿的打开与脏标记
DraftMap& jobDrafts()
{
    static DraftMap g;
    return g;
}

}

// 工具详细调试页打开
void DraftRegistry::registerToolDraft(const ObjectId& toolId)
{
    if (toolId.isNull()) {
        return;
    }
    DraftState& s = toolDrafts()[toolId.toString()];
    s.open = true;
    s.dirty = false;
}

// 更新工具 draft 脏标记
void DraftRegistry::setToolDraftDirty(const ObjectId& toolId, bool dirty)
{
    if (toolId.isNull()) {
        return;
    }
    DraftState& s = toolDrafts()[toolId.toString()];
    s.dirty = dirty;
}

// 工具详细调试页关闭
void DraftRegistry::unregisterToolDraft(const ObjectId& toolId)
{
    if (toolId.isNull()) {
        return;
    }
    toolDrafts().erase(toolId.toString());
}

// 工具 draft 是否阻止正式运行
bool DraftRegistry::toolDraftBlocksFormalRun(const ObjectId& toolId)
{
    if (toolId.isNull()) {
        return false;
    }
    DraftMap::const_iterator it = toolDrafts().find(toolId.toString());
    return it != toolDrafts().end() && it->second.open && it->second.dirty;
}

// 流程配置页打开
void DraftRegistry::registerJobDraft(const ObjectId& jobId)
{
    if (jobId.isNull()) {
        return;
    }
    DraftState& s = jobDrafts()[jobId.toString()];
    s.open = true;
    s.dirty = false;
}

// 更新流程 draft 脏标记
void DraftRegistry::setJobDraftDirty(const ObjectId& jobId, bool dirty)
{
    if (jobId.isNull()) {
        return;
    }
    DraftState& s = jobDrafts()[jobId.toString()];
    s.dirty = dirty;
}

// 流程配置页关闭
void DraftRegistry::unregisterJobDraft(const ObjectId& jobId)
{
    if (jobId.isNull()) {
        return;
    }
    jobDrafts().erase(jobId.toString());
}

// 流程 draft 是否阻止正式运行
bool DraftRegistry::jobDraftBlocksFormalRun(const ObjectId& jobId)
{
    if (jobId.isNull()) {
        return false;
    }
    DraftMap::const_iterator it = jobDrafts().find(jobId.toString());
    return it != jobDrafts().end() && it->second.open && it->second.dirty;
}

// 流程内是否有工具在未保存调试 draft 中
bool DraftRegistry::anyToolInJobBlocked(const Solution* sol, const ObjectId& jobId)
{
    if (jobDraftBlocksFormalRun(jobId)) {
        return true;
    }
    if (!sol) {
        return false;
    }
    const JobCfg* job = nullptr;
    for (size_t si = 0; si < sol->stations.size(); ++si) {
        for (size_t ji = 0; ji < sol->stations[si].jobs.size(); ++ji) {
            if (sol->stations[si].jobs[ji].id == jobId) {
                job = &sol->stations[si].jobs[ji];
                break;
            }
        }
        if (job) {
            break;
        }
    }
    if (!job) {
        return false;
    }
    for (size_t i = 0; i < job->tools.size(); ++i) {
        if (toolDraftBlocksFormalRun(job->tools[i].id)) {
            return true;
        }
    }
    return false;
}

}
