/**
 * @file ToolRstPanel.h
 * @brief ToolRstPanel头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLRESULTPANEL_H
#define VISION_TOOLRESULTPANEL_H
#include "VisionToolUiExport.h"

#include "ToolBase.h"
#include <QWidget>

class QLabel;
class QTableWidget;

namespace Vision {

class Job;
struct Solution;
struct ToolCfg;
struct JobCfg;

/**
 * @class ToolRstPanel
 * @brief 设计区右侧：运行结果与输出数值（只读，不含算法参数）
 */
class VISION_TOOLUI_API ToolRstPanel : public QWidget {
public:
    /** @brief 运行结果只读区 */
    explicit ToolRstPanel(QWidget* parent = 0);

    /**
     * @brief 按工具与流程缓存刷新
     * @param lastRun 非空且 toolId 匹配时优先显示本次运行摘要
     * @param afterJobRun true 表示刚跑完整流程，优先读流程缓存
     */
    void refresh(Solution* sol, ToolCfg* tool, Job* job, const ToolOut* lastRun,
                 bool afterJobRun = false);
    /**
     * @brief 设计态：按整 JobCfg 汇总各工具运行结果与输出
     * @param jobOut 非空时为本次流程运行摘要
     */
    void refreshJob(Solution* sol, const JobCfg* jobConfig, Job* job, const ToolOut* jobOut);

private:
    /** @brief 填输出数值表 */
    void fillOutputTable(const ParamMap& values);

    QLabel* m_toolLab;         ///< 当前工具名
    QLabel* m_statusLab;       ///< OK/NG/未运行
    QLabel* m_msgLab;          ///< 运行说明
    QLabel* m_timeLab;         ///< 耗时
    QTableWidget* m_outTable;  ///< 输出键值
};

}

#endif
