/**
 * @file WidgetPermBinder.h
 * @brief WidgetPermBinder头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_WIDGETPERMBINDER_H
#define VISION_WIDGETPERMBINDER_H
#include "VisionUiExport.h"

#include "Perm.h"
#include <vector>

class QWidget;
class QAction;

namespace Vision {

class Session;

/**
 * @class WidgetPermBinder
 * @brief 把页面上的控件、动作绑到权限位。登录或改权限后 apply
 */
class VISION_UI_API WidgetPermBinder {
public:
    /** @brief 绑定 QWidget（按钮等），无权限则禁用 */
    void bindWidget(QWidget* w, Perm perm);
    /** @brief 绑定 QAction，无权限则禁用 */
    void bindAction(QAction* a, Perm perm);
    /** @brief 按当前会话刷新全部绑定 */
    void apply(const Session& session) const;

private:
    struct Item {
        QWidget* widget;   ///< 控件，可空
        QAction* action;   ///< 动作，可空
        Perm perm;   ///< 所需权限
        Item()
            : widget(nullptr)
            , action(nullptr)
            , perm(PermNone)
        {
        }
    };
    std::vector<Item> m_items; ///< 绑定表
};

}

#endif
