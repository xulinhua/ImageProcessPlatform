/**
 * @file ToolLibDlg.h
 * @brief ToolLibDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLLIBDIALOG_H
#define VISION_TOOLLIBDIALOG_H
#include "VisionUiExport.h"
#include "PluginHost.h"
#include <QDialog>
#include <vector>
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QLabel;
namespace Vision {
/**
 * @class ToolLibDlg
 * @brief 工具库管理：插件树表一体，选中节点出详情
 */
class VISION_UI_API ToolLibDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 打开工具库管理 */
    explicit ToolLibDlg(QWidget* parent = 0);
private slots:
    /** @brief 再扫插件目录，只加新 DLL */
    void onReload();
    /** @brief 选中节点变化，刷新详情 */
    void onCurrentChanged();
    /** @brief 搜索框变化，显隐树行 */
    void onSearch(const QString& text);
private:
    /** @brief 重读登记并重建树表 */
    void reloadLists();
    /** @brief 按搜索词显隐节点，命中的父节点展开 */
    void applyFilter();
    /** @brief 按节点种类写详情 */
    void showNode(QTreeWidgetItem* item);
    /** @brief 登记叶子是否命中搜索 */
    bool leafHit(const ToolLibReg& row, const QString& key) const;

    QTreeWidget* m_tree;                 ///< 插件树表
    QLineEdit* m_search;                 ///< 搜索
    QLabel* m_kind;                      ///< 详情：类型
    QLabel* m_display;                   ///< 详情：显示名
    QLabel* m_name;                      ///< 详情：注册名
    QLabel* m_alg;                       ///< 详情：算法库
    QLabel* m_ver;                       ///< 详情：版本
    QLabel* m_file;                      ///< 详情：来源 DLL
    QLabel* m_stamp;                     ///< 详情：文件时间
    QLabel* m_desc;                      ///< 详情：说明
    std::vector<ToolLibReg> m_regs;      ///< 登记缓存
    std::vector<ToolLibItem> m_files;    ///< 已载入 DLL
};
}
#endif