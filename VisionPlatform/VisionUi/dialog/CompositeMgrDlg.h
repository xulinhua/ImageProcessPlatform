/**
 * @file CompositeMgrDlg.h
 * @brief CompositeMgrDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_COMPOSITEMGRDLG_H
#define VISION_COMPOSITEMGRDLG_H
#include <QDialog>
class QTableWidget;
class QListWidget;
class QLineEdit;
class QComboBox;
namespace Vision {
struct Solution;
/**
 * @class CompositeMgrDlg
 * @brief 组合工具列表：新建、修改、删除独立或项目组合工具
 */
class CompositeMgrDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 建表并载入列表；sol 为空时不能新建项目组合工具 */
    explicit CompositeMgrDlg(Solution* sol, QWidget* parent = 0);
private slots:
    /** @brief 按 CompositeMgr 重填列表 */
    void reload();
    /** @brief 输入名称与范围后新建；项目范围置方案 dirty */
    void onNew();
    /** @brief 打开选中行的编辑窗；保存了项目范围的才置方案 dirty */
    void onEdit();
    /** @brief 确认后删除选中行 */
    void onDelete();
private:
    Solution* m_sol; ///< 当前方案，不拥有，可空
    QTableWidget* m_table; ///< 列表；第 0 列 UserRole 存组合工具 id
};
/**
 * @class CompositeEditDlg
 * @brief 编辑一个组合工具：内部工具、各工具算法库、公开接口和公开参数
 * @note 直接改 CompositeMgr 里的定义；独立组合工具在保存时写盘
 */
class CompositeEditDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 按 id 载入组合工具；找不到时只显示空表单 */
    CompositeEditDlg(const QString& id, QWidget* parent = 0);
private slots:
    /************************************************** 编辑 **************************************************/
    /** @brief 选注册工具或其他组合工具加进来；独立组合工具不能引用项目组合工具 */
    void onAddTool();
    /** @brief 删除选中的内部工具 */
    void onDelTool();
    /** @brief 写回名称、说明和当前页的公开项；独立组合工具同时存盘 */
    void onSave();
    /** @brief 切换内部工具：先存上一工具的算法库与公开项，再载入新工具 */
    void onToolChanged();
    /** @brief 算法库下拉变化，写回当前工具 */
    void onAlgLibChanged(int index);
    /** @brief 上移选中的内部工具 */
    void onMoveUp();
    /** @brief 下移选中的内部工具 */
    void onMoveDown();
private:
    /************************************************** 公开项 **************************************************/
    /** @brief 按当前工具填算法库、可公开接口和参数，并勾上已公开的 */
    void loadPubs();
    /** @brief 用勾选结果替换当前工具的公开接口和参数 */
    void storePubs();
    /** @brief 把算法库下拉写回当前工具，并刷新列表文字 */
    void applyAlgLib();
    /** @brief 按列表顺序重排内部工具；数量对不上时不动 */
    void syncToolOrder();
    QString m_id; ///< 组合工具 id
    QString m_shownId; ///< 公开项页当前对应的内部工具 id；空为无
    bool m_filling; ///< 正在填算法库下拉，期间不写回
    QLineEdit* m_name; ///< 名称
    QLineEdit* m_desc; ///< 说明
    QListWidget* m_tools; ///< 内部工具；UserRole 存工具 id
    QComboBox* m_algLib; ///< 当前工具的算法库；数据为 AlgLibKind
    QListWidget* m_ports; ///< 可公开接口；UserRole 为方向，UserRole+1 为数据类型
    QListWidget* m_paras; ///< 可公开参数；UserRole 为参数键
};
}
#endif