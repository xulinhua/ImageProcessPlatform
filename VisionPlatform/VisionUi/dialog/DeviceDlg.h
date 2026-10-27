/**
 * @file DeviceDlg.h
 * @brief DeviceDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DEVICEDIALOG_H
#define VISION_DEVICEDIALOG_H
#include "VisionUiExport.h"

#include "RecipeDraft.h"
#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QCloseEvent;
class QWidget;
class QSpinBox;
class QListWidget;

namespace Vision {

class Session;
struct Solution;

/**
 * @class DeviceDlg
 * @brief 单设备配置：按相机/控制卡/PLC 显示不同字段
 */
class VISION_UI_API DeviceDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 编辑指定设备；会话与方案不拥有 */
    DeviceDlg(Session* session, Solution* solution, const QString& deviceId, QWidget* parent = 0);

private slots:
    /** @brief 写回草稿并关闭 */
    void onOk();
    /** @brief 选择文件源目录 */
    void onBrowse();
    /** @brief 控件改动后同步草稿与字段显隐 */
    void onEdited();
    /** @brief 种类变化后刷新可见字段 */
    void onKindChanged(int);
    /** @brief 添加要记录的图像 */
    void onAddImages();
    /** @brief 从列表移除选中图像 */
    void onRemoveImage();

protected:
    /** @brief 关闭前询问是否保存 */
    void reject();
    /** @brief 标题栏关闭时同样询问是否保存 */
    void closeEvent(QCloseEvent* event);

private:
    /** @brief 把界面字段写入设备草稿 */
    void flushUiToDraft();
    /**
     * @brief 未保存时询问是否保存
     * @return 0 取消，1 不保存，2 已保存
     */
    int askCloseAction();
    /** @brief 提交草稿到方案；失败则弹窗 */
    bool trySave();
    /** @brief 按相机、控制卡、PLC 显隐字段 */
    void syncFieldVisibility();
    /** @brief 用方案里的通信通道填充下拉 */
    void reloadCommCombo();

    Session* m_session;            ///< 会话
    Solution* m_sol;               ///< 方案
    QString m_id;                  ///< 设备 id
    DeviceRecipe m_draft;          ///< draft
    DeviceRecipe m_formalSnapshot; ///< 快照
    QLineEdit* m_name;             ///< 名称
    QComboBox* m_kind;             ///< 种类
    QComboBox* m_backend;          ///< 相机后端
    QLineEdit* m_path;             ///< 文件源目录
    QWidget* m_pathRow;            ///< 目录行容器
    QListWidget* m_images;         ///< 图像路径列表
    QWidget* m_imageRow;           ///< 图像列表行
    QLineEdit* m_vendor;           ///< 控制卡厂商
    QLineEdit* m_address;          ///< 控制卡地址
    QComboBox* m_comm;             ///< PLC 绑定通道
    QSpinBox* m_slave;             ///< PLC 从站
};

}

#endif
