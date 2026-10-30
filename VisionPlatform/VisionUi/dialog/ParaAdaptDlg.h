/**
 * @file ParaAdaptDlg.h
 * @brief ParaAdaptDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_PARAADAPTDLG_H
#define VISION_PARAADAPTDLG_H
#include <QDialog>
#include <string>
class QTableWidget;
class QTableWidgetItem;
namespace Vision {
struct Solution;
/**
 * @class ParaAdaptDlg
 * @brief 参数版本页：列出方案内全部工具的参数、工具、算法版本，可升级、降级或对比
 */
class ParaAdaptDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 建表并按方案填行；sol 不拥有，可空（空时表为空） */
    explicit ParaAdaptDlg(Solution* sol, QWidget* parent = 0);
private slots:
    /** @brief 重建全部工具行，并按参数版本比较给出状态 */
    void reload();
    /** @brief 选中行的参数适配到程序当前参数版本 */
    void onUpgrade();
    /** @brief 选中行的参数适配到用户输入的版本 */
    void onDowngrade();
    /** @brief 打开选中行的参数对比，不改方案 */
    void onPreview();
    /** @brief 打开 item 所在行的参数对比窗 */
    void onOpenCompare(QTableWidgetItem* item);
private:
    /**
     * @brief 对一行调用 adaptParam，成功后更新工具版本并置方案 dirty
     * @param target 目标参数版本；空表示程序当前版本
     * @return 适配成功；失败时已弹窗提示
     */
    bool applyRow(int row, const std::string& target);
    Solution* m_sol; ///< 当前方案，不拥有
    QTableWidget* m_table; ///< 工具行表；第 0 列 UserRole 起存工站、流程、工具下标
};
}
#endif