/**
 * @file JobRstRuleDlg.h
 * @brief JobRstRuleDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_JOBRESULTRULEDIALOG_H
#define VISION_JOBRESULTRULEDIALOG_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QDialog>

namespace Vision {

class JobRstRulePanel;

/**
 * @class JobRstRuleDlg
 * @brief 输出结果配置独立页：逻辑判断矩阵与流程结果取自
 */
class VISION_UI_API JobRstRuleDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 打开配置页
     * @param job 当前编辑中的 JobCfg draft（不拥有）
     * @param editable 是否可改
     */
    JobRstRuleDlg(JobCfg* job, bool editable, QWidget* parent = 0);

    /** @brief 确定：写回 JobCfg */
    void accept();

private:
    JobRstRulePanel* m_panel; ///< 矩阵编辑区
    JobCfg* m_job;                  ///< 绑定的 JobCfg draft
};

}

#endif
