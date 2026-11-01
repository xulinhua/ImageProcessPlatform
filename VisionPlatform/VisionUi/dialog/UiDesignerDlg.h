/**
 * @file UiDesignerDlg.h
 * @brief 自定义 UI 设计器
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UIDESIGNERDLG_H
#define VISION_UIDESIGNERDLG_H
#include "VisionUiExport.h"

#include "Solution.h"
#include <QDialog>
#include <QMap>

class QCheckBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QLabel;

namespace Vision {

/**
 * @class UiDesignerDlg
 * @brief 四区设计器：控件板 / 画布 / 属性 / 顶栏；编辑 solution.uiPages[0]
 */
class VISION_UI_API UiDesignerDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 编辑方案首个 UI 页；若无页则新建
     * @param sol 方案（不拥有）
     */
    explicit UiDesignerDlg(Solution* sol, QWidget* parent = 0);

private slots:
    /** @brief 从左侧面板添加控件 */
    void onAddWidget();
    /** @brief 选中画布控件 */
    void onSelectWidget();
    /** @brief 属性变更写回选中控件 */
    void onPropChanged();
    /** @brief 删除选中控件 */
    void onRemoveWidget();
    /** @brief 保存到方案并关闭 */
    void onSave();

private:
    /** @brief 确保至少有一页 */
    void ensurePage();
    /** @brief 重建画布子控件 */
    void rebuildCanvas();
    /** @brief 把属性栏刷成当前选中项 */
    void loadProps();
    /** @brief 当前选中下标；无则 -1 */
    int selectedIndex() const;
    /** @brief 种类显示名 */
    static QString kindTitle(UiWidgetKind k);

    Solution* m_sol;          ///< 方案
    UiPage m_page;            ///< 编辑中的页
    QWidget* m_canvas;        ///< 绝对定位画布
    QListWidget* m_palette;   ///< 左侧控件种类
    QLineEdit* m_pageName;    ///< 页名
    QCheckBox* m_enabled;     ///< 启用
    QLineEdit* m_text;        ///< 文案
    QLineEdit* m_dataPath;    ///< 数据路径
    QLineEdit* m_action;      ///< 动作
    QSpinBox* m_x;            ///< x
    QSpinBox* m_y;            ///< y
    QSpinBox* m_w;            ///< w
    QSpinBox* m_h;            ///< h
    QString m_selectedId;     ///< 当前选中控件 id
    QMap<QString, QWidget*> m_proxy; ///< id → 画布代理控件
    bool m_blockProp;         ///< 刷属性时抑制回写
};

}

#endif
