/**
 * @file Theme.h
 * @brief Theme头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_THEME_H
#define VISION_THEME_H
#include "VisionWidgetsExport.h"

#include <QString>
#include <string>

class QWidget;
class QPushButton;
class QFrame;
class QDialog;
class QBoxLayout;
class QVBoxLayout;
class QLabel;

namespace Vision {

/** @brief 当前默认档（浅色工业）的全局 QSS */
VISION_WIDGETS_API QString appStyleSheet();
/**
 * @brief 按主题档生成 QSS。id 为 industrial、industrialLarge、industrialContrast
 * @param themeId 存盘键；不认识则回退 industrial
 */
VISION_WIDGETS_API QString appStyleSheet(const std::string& themeId);
/** @brief 应用到 qApp，立即换皮 */
VISION_WIDGETS_API void applyAppTheme(const std::string& themeId);
/** @brief 主题档数量 */
VISION_WIDGETS_API int themeCount();
/** @brief 第 i 个主题存盘键 */
VISION_WIDGETS_API const char* themeIdAt(int i);
/** @brief 主题显示名（走 Translate） */
VISION_WIDGETS_API std::string themeTitle(const std::string& themeId);

/**
 * @brief 给按钮打 role 属性，配合 QSS 区分主/次/危险
 * @param role 如 "primary" / "danger"
 */
VISION_WIDGETS_API void setButtonRole(QPushButton* btn, const char* role);
/** @brief 卡片容器（圆角面板） */
VISION_WIDGETS_API QFrame* makeCard(QWidget* parent);
/**
 * @brief 允许对话框手动调整尺寸（去固定大小标记、启用右下角 grip）
 * @param dlg 目标对话框
 */
VISION_WIDGETS_API void enableDialogResize(QDialog* dlg);
/** @brief 去掉系统标题栏，保留任务栏标题 */
VISION_WIDGETS_API void applyFramelessDialog(QDialog* dlg);
/** @brief 创建右上角关闭按钮，点击触发 dlg->close() */
VISION_WIDGETS_API QPushButton* makeDialogCloseButton(QDialog* dlg, QWidget* parent);
/** @brief 在 handle 区域按住左键拖动对话框 */
VISION_WIDGETS_API void installDialogDragHandle(QDialog* dlg, QWidget* handle);
/**
 * @brief 在根布局顶插入标题栏（标题 + 关闭），并启用拖拽
 * @return 标题文字标签，objectName 为 dialogTitleText
 */
VISION_WIDGETS_API QLabel* prependFramelessTitleBar(QDialog* dlg, QBoxLayout* rootLayout, const QString& title);
/**
 * @brief 无边框对话框内容区（左右/下留白，顶栏仍贴边）
 * @param margin 内容区左右/下内边距 px
 * @param topMargin 内容区与顶栏间距 px
 */
VISION_WIDGETS_API QVBoxLayout* framelessDialogContentLayout(QDialog* dlg, QBoxLayout* rootLayout,
                                          int margin = 12, int topMargin = 10);
/** @brief 同步自绘标题栏文字（仍保留 setWindowTitle 供任务栏） */
VISION_WIDGETS_API void syncDialogTitleBar(QDialog* dlg, const QString& title);
/**
 * @brief 创建 CV-X 品牌顶栏（logo + 标题 + 可选副标题/帮助 + 关闭）
 * @param titleLabelOut 若非空则写出标题标签指针
 */
VISION_WIDGETS_API QFrame* makeCvXTitleBar(QDialog* dlg, QWidget* parent, const QString& title,
                        const QString& subtitle = QString(), bool withHelp = false,
                        QLabel** titleLabelOut = 0);
/**
 * @brief 弹出未保存关窗询问（保存 / 不保存 / 取消）
 * @return 0 取消，1 不保存，2 点保存
 */
VISION_WIDGETS_API int askUnsavedCloseQuestion(QWidget* parent);

}

#endif
