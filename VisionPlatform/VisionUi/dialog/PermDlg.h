/**
 * @file PermDlg.h
 * @brief PermDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PERMISSIONDIALOG_H
#define VISION_PERMISSIONDIALOG_H
#include "VisionUiExport.h"
#include "AppSettings.h"
#include <QDialog>
#include <vector>

class QTreeWidget;
class QTreeWidgetItem;

namespace Vision {

class Session;
class Solution;

/**
 * @class PermDlg
 * @brief 按页面、按控件勾选三角色权限。厂家不出现在表里
 * @note 勾选页面或工具时，下属控件一起变。实例清单由已打开的方案交进来
 */
class VISION_UI_API PermDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造权限树
     * @param session 会话（不拥有）
     * @param solution 当前方案（不拥有）。实例管控时用来列出流程里的工具
     */
    explicit PermDlg(Session* session, Solution* solution, QWidget* parent = 0);

private slots:
    /** @brief 把树中勾选写入三角色掩码并保存 */
    void onSave();
    /** @brief 用默认权限重填（不立刻落盘） */
    void onReset();
    /** @brief 勾选上层时，下属同一列一起变 */
    void onItemChanged(QTreeWidgetItem* item, int column);

private:
    /**
     * @brief 填充权限树
     * @param useDefaults true 用 defaultPerms，false 用当前 settings
     */
    void loadTree(bool useDefaults);
    /** @brief 把一列的勾选写到全部子孙。锁定项保持勾选 */
    void setColumnDown(QTreeWidgetItem* item, int col, Qt::CheckState state);
    /** @brief 按子孙状态回写父节点 */
    void refreshColumnUp(QTreeWidgetItem* item, int col);
    /** @brief 收集工具和实例节点 */
    void collectNodes(QTreeWidgetItem* item, std::vector<AppSettings::PermNodeGrant>* out) const;
    /** @brief 加一个树节点。perm 用来取默认和锁定，节点键始终写入树 */
    QTreeWidgetItem* addCheckRow(QTreeWidgetItem* parent, const QString& title, const QString& tip,
                                 Perm perm, const QString& nodeKey);

    Session* m_session;     ///< 会话（不拥有）
    Solution* m_solution;   ///< 当前方案（不拥有）
    QTreeWidget* m_tree;    ///< 页面为父节点，控件为子节点
    bool m_guard;           ///< 级联勾选时挡住重入
    bool m_resetNodes;      ///< 保存时清掉未出现在树上的旧节点
};

}

#endif
