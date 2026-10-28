/**
 * @file VersionDlg.h
 * @brief VersionDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_VERSIONDIALOG_H
#define VISION_VERSIONDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>

class QLabel;
class QTableWidget;

namespace Vision {

/**
 * @class VersionDlg
 * @brief 版本管理：软件 / 模块 / 已注册工具版本
 */
class VISION_UI_API VersionDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 构造并填充版本表 */
    explicit VersionDlg(QWidget* parent = 0);

private:
    /** @brief 填充模块与工具表 */
    void fillTables();

    QLabel* m_summary;          ///< 产品名、版本、构建时间
    QTableWidget* m_modules;    ///< 模块版本表
    QTableWidget* m_tools;      ///< 工具版本表
};

}

#endif