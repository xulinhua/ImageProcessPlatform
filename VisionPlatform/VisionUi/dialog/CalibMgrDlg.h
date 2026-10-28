/**
 * @file CalibMgrDlg.h
 * @brief CalibMgrDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CALIBMANAGERDIALOG_H
#define VISION_CALIBMANAGERDIALOG_H
#include "VisionUiExport.h"

#include <QDialog>
#include <QString>

class QListWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

namespace Vision {

struct Solution;

/**
 * @class CalibMgrDlg
 * @brief 标定管理：按相机改参数、看生效结果、导入点表或结果
 */
class VISION_UI_API CalibMgrDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 方案不拥有，只用来列台账里的相机 */
    CalibMgrDlg(Solution* solution, QWidget* parent = 0);

private slots:
    /** @brief 切换相机并刷新右侧 */
    void onCameraChanged();
    /** @brief 把表单写入当前相机 */
    void onSaveParam();
    /** @brief 导入离线点表 */
    void onImportTable();
    /** @brief 导入已算好的结果文件 */
    void onImportResult();
    /** @brief 添加一路还没进台账的临时相机 */
    void onAddCamera();
    /** @brief 把临时相机绑到台账相机 */
    void onBindCamera();
    /** @brief 切换标定方式，只显示这一方式的参数 */
    void onKindChanged();

private:
    /** @brief 重新列出台账相机和已有标定 */
    void reloadCameras(const QString& selectId);
    /** @brief 把当前相机的参数填进表单 */
    void loadForm();
    /** @brief 显示生效结果和历史 */
    void showResult();
    /** @brief 当前选中的相机 id */
    QString currentId() const;

    Solution* m_solution;              ///< 当前方案
    QString m_currentId;               ///< 正在编辑的相机
    bool m_loading;                    ///< 正在刷新表单，忽略信号
    QListWidget* m_list;               ///< 相机列表
    QComboBox* m_bindTarget;           ///< 要绑定到的台账相机
    QPushButton* m_bindBtn;            ///< 绑定按钮
    QComboBox* m_kind;                 ///< 标定方式
    QStackedWidget* m_pages;           ///< 三种方式的参数页
    QPushButton* m_tableBtn;           ///< 导入点表
    QComboBox* m_plate;                ///< 板型
    QSpinBox* m_rows;                  ///< 行数
    QSpinBox* m_cols;                  ///< 列数
    QDoubleSpinBox* m_spacing;         ///< 间距
    QDoubleSpinBox* m_thickness;       ///< 板厚
    QComboBox* m_fit;                  ///< 九点拟合
    QDoubleSpinBox* m_resPx;           ///< 像素残差上限
    QDoubleSpinBox* m_resMm;           ///< 毫米残差上限
    QDoubleSpinBox* m_angle;           ///< 示教角
    QSpinBox* m_minImages;             ///< 标定板最少张数
    QSpinBox* m_minAngles;             ///< 旋转最少角度
    QSpinBox* m_width;                 ///< 图像宽
    QSpinBox* m_height;                ///< 图像高
    QPlainTextEdit* m_result;          ///< 生效结果
};

}

#endif