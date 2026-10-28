/**
 * @file AboutDlg.h
 * @brief AboutDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_ABOUTDIALOG_H
#define VISION_ABOUTDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>

namespace Vision {

/**
 * @class AboutDlg
 * @brief 关于：品牌、简介、版本摘要与版权（详细版本见「版本管理」）
 */
class VISION_UI_API AboutDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 构造关于对话框 */
    explicit AboutDlg(QWidget* parent = 0);

private slots:
    /** @brief 打开版本管理并关闭本窗 */
    void onOpenVersions();
};

}

#endif