/**
 * @file GlobalVarDlg.h
 * @brief 全局变量管理对话框
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_GLOBALVARDLG_H
#define VISION_GLOBALVARDLG_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QDialog>

class QComboBox;
class QTableWidget;

namespace Vision {

/**
 * @class GlobalVarDlg
 * @brief 编辑方案级全局变量表：名称、类型、值、注释；支持导入导出
 */
class VISION_UI_API GlobalVarDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 编辑 sol->globalVars
     * @param sol 方案（不拥有）
     */
    explicit GlobalVarDlg(Solution* sol, QWidget* parent = 0);

private slots:
    /** @brief 添加一行默认变量 */
    void onAdd();
    /** @brief 删除当前行 */
    void onRemove();
    /** @brief 从 JSON/CSV 导入 */
    void onImport();
    /** @brief 导出为 JSON */
    void onExport();
    /** @brief 写回方案并关闭 */
    void onSave();

private:
    /** @brief 从 VarStore 填充表格 */
    void loadTable();
    /** @brief 表格 → 变量列表；校验名称 */
    bool collectItems(std::vector<GlobalVar>* out, QString* err) const;
    /** @brief 类型显示名 */
    static QString typeTitle(GlobalVarType t);
    /** @brief 下拉下标 → 类型 */
    static GlobalVarType typeFromCombo(int idx);
    /** @brief 类型下拉 */
    static QComboBox* makeTypeCombo(QWidget* parent, GlobalVarType t);

    Solution* m_sol;       ///< 方案（不拥有）
    QTableWidget* m_table; ///< 变量表
};

}

#endif
