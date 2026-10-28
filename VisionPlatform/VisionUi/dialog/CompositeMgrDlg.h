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
class CompositeMgrDlg : public QDialog {
    Q_OBJECT
public:
    explicit CompositeMgrDlg(Solution* sol, QWidget* parent = 0);
private slots:
    void reload();
    void onNew();
    void onEdit();
    void onDelete();
private:
    Solution* m_sol;
    QTableWidget* m_table;
};
class CompositeEditDlg : public QDialog {
    Q_OBJECT
public:
    CompositeEditDlg(const QString& id, QWidget* parent = 0);
private slots:
    void onAddTool();
    void onDelTool();
    void onSave();
    void onToolChanged();
    void onAlgLibChanged(int index);
    void onMoveUp();
    void onMoveDown();
private:
    void loadPubs();
    void storePubs();
    void applyAlgLib();
    void syncToolOrder();
    QString m_id;
    QString m_shownId;
    bool m_filling;
    QLineEdit* m_name;
    QLineEdit* m_desc;
    QListWidget* m_tools;
    QComboBox* m_algLib;
    QListWidget* m_ports;
    QListWidget* m_paras;
};
}
#endif