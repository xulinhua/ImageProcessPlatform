/**
 * @file DeviceMgrDlg.h
 * @brief DeviceMgrDlg头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_DEVICEMANAGERDIALOG_H
#define VISION_DEVICEMANAGERDIALOG_H
#include "VisionUiExport.h"

#include "DeviceControl.h"
#include <QDialog>
#include <QString>

class QListWidget;
class QLabel;
class QTabWidget;
class QPushButton;

namespace HsCtrl { class HsImageWindow; }

namespace Vision {

class Session;
struct Solution;

/**
 * @class DeviceMgrDlg
 * @brief 设备管理：相机/控制卡/PLC 三类；每类含遍历与管理
 */
class VISION_UI_API DeviceMgrDlg : public QDialog {
    Q_OBJECT
public:
    /** @brief 打开设备管理；会话与方案不拥有 */
    DeviceMgrDlg(Session* session, Solution* solution, QWidget* parent = 0);

signals:
    /** @brief 请求打开单设备配置 */
    void openDeviceRequested(const QString& deviceId);

private slots:
    /** @brief 切换相机、控制卡或 PLC 并重新遍历 */
    void onKindTabChanged(int index);
    /** @brief 枚举当前种类的设备 */
    void onScan();
    /** @brief 把遍历结果登记进台账 */
    void onRegisterProbe();
    /** @brief 手工加一台并打开配置 */
    void onAddManual();
    /** @brief 从台账移除当前设备 */
    void onRemove();
    /** @brief 打开当前台账设备的配置 */
    void onOpen();
    /** @brief 选中台账项后刷新摘要 */
    void onManagedSel();
    /** @brief 试打开再关闭，并提示成败 */
    void onTestOpen();

private:
    /** @brief 按当前种类重填台账列表 */
    void refreshManaged();
    /** @brief 显示最近一次遍历结果 */
    void refreshProbe();
    /** @brief 台账当前选中设备 id */
    QString currentManagedId() const;
    /** @brief 当前页签对应的设备种类 */
    DeviceKind currentKind() const;

    Session* m_session;                 ///< 会话
    Solution* m_sol;                    ///< 方案
    QTabWidget* m_kindTabs;             ///< 三类页签
    QListWidget* m_probeList;           ///< 遍历列表
    QListWidget* m_managedList;         ///< 台账列表
    QLabel* m_summary;                  ///< 摘要
    HsCtrl::HsImageWindow* m_preview;  ///< 相机预览
    QPushButton* m_testBtn;             ///< 连接测试
    CamDeviceControl m_camCtrl;      ///< 相机控制
    ControlCardDeviceControl m_cardCtrl;///< 控制卡控制
    PlcDeviceControl m_plcCtrl;         ///< PLC 控制
    std::vector<DeviceProbeInfo> m_probes; ///< 最近遍历结果
};

}

#endif
