/**
 * @file DraftRegistry.h
 * @brief DraftRegistry头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DRAFTREGISTRY_H
#define VISION_DRAFTREGISTRY_H
#include "VisionCoreExport.h"

#include "ObjectId.h"

namespace Vision {

struct Solution;

/** @class DraftRegistry @brief 配方草稿打开/脏标记，阻止未保存时跑 formal */
class VISIONCORE_API DraftRegistry {
public:
    /** @brief 工具详细调试页打开 */
    static void registerToolDraft(const ObjectId& toolId);
    /** @brief 更新工具草稿脏标记 */
    static void setToolDraftDirty(const ObjectId& toolId, bool dirty);
    /** @brief 工具详细调试页关闭 */
    static void unregisterToolDraft(const ObjectId& toolId);
    /** @brief 工具草稿是否阻止正式运行 */
    static bool toolDraftBlocksFormalRun(const ObjectId& toolId);

    /** @brief 流程配置页打开 */
    static void registerJobDraft(const ObjectId& jobId);
    /** @brief 更新流程草稿脏标记 */
    static void setJobDraftDirty(const ObjectId& jobId, bool dirty);
    /** @brief 流程配置页关闭 */
    static void unregisterJobDraft(const ObjectId& jobId);
    /** @brief 流程草稿是否阻止正式运行 */
    static bool jobDraftBlocksFormalRun(const ObjectId& jobId);

    /** @brief 流程内是否有工具停在未保存调试草稿 */
    static bool anyToolInJobBlocked(const Solution* sol, const ObjectId& jobId);
};

}

#endif
