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

#include <QDialog>

class QTreeWidget;

namespace Vision {

class Session;

/**
 * @class PermDlg
 * @brief 按页面、按控件勾选三角色权限；管理员锁定项不可关闭
 */
class VISION_UI_API PermDlg : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief 构造权限树表
     * @param session 会话（不拥有），读写 settings 中的权限掩码
     */
    explicit PermDlg(Session* session, QWidget* parent = 0);

private slots:
    /** @brief 把树中勾选写入三角色掩码并保存 */
    void onSave();
    /** @brief 用默认权限重填（不立刻落盘） */
    void onReset();

private:
    /**
     * @brief 填充权限树
     * @param useDefaults true 用 defaultPerms，false 用当前 settings
     */
    void loadTree(bool useDefaults);
    /**
     * @brief 把指定列勾选合成掩码
     * @param col 1=操作员，2=工程师，3=管理员
     */
    quint32 maskFromColumn(int col) const;

    Session* m_session;     ///< 会话（不拥有）
    QTreeWidget* m_tree;    ///< 页面为父节点，控件为子节点
};

}

#endif
