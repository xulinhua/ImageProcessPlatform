/**
 * @file ParaAdaptDlg.h
 * @brief ParaAdaptDlgÍ·ÎÄ¼þ
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
class ParaAdaptDlg : public QDialog {
    Q_OBJECT
public:
    explicit ParaAdaptDlg(Solution* sol, QWidget* parent = 0);
private slots:
    void reload();
    void onUpgrade();
    void onDowngrade();
    void onPreview();
    void onOpenCompare(QTableWidgetItem* item);
private:
    bool applyRow(int row, const std::string& target);
    Solution* m_sol;
    QTableWidget* m_table;
};
}
#endif