/**
 * @file DeviceMgrDlg.cpp
 * @brief DeviceMgrDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeviceMgrDlg.h"
#include "AppPaths.h"
#include "Solution.h"
#include "QtConv.h"
#include "VpUtil.h"
#include "Session.h"
#include "Theme.h"
#include "UiText.h"
#include "Perm.h"
#include <HsCtrl/HsImageWindow.h>
#include <cstdio>
#include <string>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Vision {

// 设备管理：三类页签 + 遍历/管理
DeviceMgrDlg::DeviceMgrDlg(Session* session, Solution* solution, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_sol(solution)
    , m_testBtn(nullptr)
{
    setWindowTitle(T("设备管理"));
    resize(1000, 600);
    enableDialogResize(this);

    // 页签只用来选设备种类，页本身为空；下方三栏随页签刷新
    m_kindTabs = new QTabWidget(this);
    m_kindTabs->addTab(new QWidget(m_kindTabs), T("相机"));
    m_kindTabs->addTab(new QWidget(m_kindTabs), T("文件夹"));
    m_kindTabs->addTab(new QWidget(m_kindTabs), T("图像"));
    m_kindTabs->addTab(new QWidget(m_kindTabs), T("控制卡"));
    m_kindTabs->addTab(new QWidget(m_kindTabs), T("PLC"));
    connect(m_kindTabs, SIGNAL(currentChanged(int)), this, SLOT(onKindTabChanged(int)));

    m_probeList = new QListWidget(this);
    m_managedList = new QListWidget(this);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_preview = new HsCtrl::HsImageWindow(this);
    m_preview->setWindowMode(HsCtrl::WindowRuntime);
    m_preview->setDrawTool(HsCtrl::ToolBrowse);
    m_preview->setMinimumSize(280, 200);

    // 无编辑设备权限时只能查看和进入配置
    QPushButton* scanBtn = new QPushButton(T("重新遍历"), this);
    QPushButton* regBtn = new QPushButton(T("登记选中"), this);
    QPushButton* addBtn = new QPushButton(T("手动登记"), this);
    QPushButton* delBtn = new QPushButton(T("删除"), this);
    QPushButton* openBtn = new QPushButton(T("进入配置"), this);
    m_testBtn = new QPushButton(T("连接测试"), this);
    setButtonRole(openBtn, "primary");
    const bool can = session->has(PermEditDevice);
    scanBtn->setEnabled(can);
    regBtn->setEnabled(can);
    addBtn->setEnabled(can);
    delBtn->setEnabled(can);
    m_testBtn->setEnabled(can);
    connect(scanBtn, SIGNAL(clicked()), this, SLOT(onScan()));
    connect(regBtn, SIGNAL(clicked()), this, SLOT(onRegisterProbe()));
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddManual()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onRemove()));
    connect(openBtn, SIGNAL(clicked()), this, SLOT(onOpen()));
    connect(m_testBtn, SIGNAL(clicked()), this, SLOT(onTestOpen()));
    connect(m_managedList, SIGNAL(itemSelectionChanged()), this, SLOT(onManagedSel()));
    connect(m_managedList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onOpen()));

    // 三栏：遍历到的候选 | 台账内设备 | 属性与预览
    QVBoxLayout* probeLay = new QVBoxLayout();
    QLabel* pTitle = new QLabel(T("遍历"), this);
    pTitle->setObjectName(QLatin1String("section"));
    probeLay->addWidget(pTitle);
    probeLay->addWidget(new QLabel(T("发现本机候选，可登记到台账"), this));
    probeLay->addWidget(m_probeList, 1);
    QHBoxLayout* pb = new QHBoxLayout();
    pb->addWidget(scanBtn);
    pb->addWidget(regBtn);
    probeLay->addLayout(pb);

    QVBoxLayout* mgrLay = new QVBoxLayout();
    QLabel* mTitle = new QLabel(T("管理"), this);
    mTitle->setObjectName(QLatin1String("section"));
    mgrLay->addWidget(mTitle);
    mgrLay->addWidget(m_managedList, 1);
    QHBoxLayout* mb = new QHBoxLayout();
    mb->addWidget(addBtn);
    mb->addWidget(delBtn);
    mgrLay->addLayout(mb);

    QVBoxLayout* right = new QVBoxLayout();
    QLabel* aTitle = new QLabel(T("属性"), this);
    aTitle->setObjectName(QLatin1String("section"));
    right->addWidget(aTitle);
    right->addWidget(m_summary);
    right->addWidget(m_preview, 1);
    right->addWidget(m_testBtn);
    right->addWidget(openBtn);

    QHBoxLayout* body = new QHBoxLayout();
    body->addLayout(probeLay, 1);
    body->addLayout(mgrLay, 1);
    body->addLayout(right, 2);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addWidget(m_kindTabs);
    lay->addLayout(body, 1);
    onKindTabChanged(0);
}

// 当前页签对应的设备种类
DeviceKind DeviceMgrDlg::currentKind() const
{
    switch (m_kindTabs->currentIndex()) {
    case 1: return DeviceFolder;
    case 2: return DeviceImage;
    case 3: return DeviceControlCard;
    case 4: return DevicePlc;
    default: return DeviceCamera;
    }
}
void DeviceMgrDlg::onKindTabChanged(int)
{
    const DeviceKind kind = currentKind();
    m_preview->setVisible(kind == DeviceCamera || kind == DeviceFolder || kind == DeviceImage);
    onScan();
    refreshManaged();
}
void DeviceMgrDlg::onScan()
{
    const DeviceKind kind = currentKind();
    if (kind == DeviceCamera || kind == DeviceImage) {
        m_probes.clear();
        refreshProbe();
        return;
    }
    IDeviceControl* ctrl = nullptr;
    if (kind == DeviceFolder) {
        ctrl = &m_camCtrl;
    } else if (kind == DeviceControlCard) {
        ctrl = &m_cardCtrl;
    } else {
        ctrl = &m_plcCtrl;
    }
    m_probes = ctrl->enumerate();
    refreshProbe();
}
void DeviceMgrDlg::refreshProbe()
{
    m_probeList->clear();
    for (size_t i = 0; i < m_probes.size(); ++i) {
        QListWidgetItem* it = new QListWidgetItem(qs(m_probes[i].title), m_probeList);
        it->setData(Qt::UserRole, static_cast<int>(i));
    }
}

// 按当前种类重填台账
void DeviceMgrDlg::refreshManaged()
{
    m_managedList->clear();
    const DeviceKind kind = currentKind();
    for (size_t i = 0; i < m_sol->inventory.size(); ++i) {
        const HardwareDevice& d = m_sol->inventory[i];
        if (d.kind != kind) {
            continue;
        }
        QString label = qs(d.name);
        if (kind == DeviceCamera) {
            label += QLatin1String("  [")
                     + QString::fromUtf8(cameraBackendTitle(d.cameraBackend).c_str())
                     + QLatin1Char(']');
        }
        QListWidgetItem* it = new QListWidgetItem(label, m_managedList);
        it->setData(Qt::UserRole, qs(d.id));
    }
    onManagedSel();
}

// 台账当前选中设备 id
QString DeviceMgrDlg::currentManagedId() const
{
    QListWidgetItem* it = m_managedList->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

// 选中台账项后刷新摘要
void DeviceMgrDlg::onManagedSel()
{
    const HardwareDevice* d = m_sol->findDevice(oid(currentManagedId()));
    if (!d) {
        m_summary->setText(T("请选择台账中的设备。"));
        return;
    }
    QString text = T("设备：") + qs(d->name)
                   + T("\n类型：") + QString::fromUtf8(deviceKindTitle(d->kind).c_str());
    if (d->kind == DeviceCamera) {
        text += T("\n后端：") + QString::fromUtf8(cameraBackendTitle(d->cameraBackend).c_str());
    } else if (d->kind == DeviceFolder) {
        text += QLatin1String("\n") + T("目录") + QString(QChar(0xFF1A)) + qs(d->defaultParams.value("folder").toString());
    } else if (d->kind == DeviceImage) {
        text += QLatin1String("\n") + T("已记录") + QString(QChar(0xFF1A)) + qs(d->defaultParams.value("images").toString());
    } else if (d->kind == DeviceControlCard) {
        text += T("\n厂商：") + qs(d->defaultParams.value("vendor").toString());
        text += T("\n地址：") + qs(d->defaultParams.value("address").toString());
    } else {
        text += T("\n通信通道：") + qs(d->defaultParams.value("commChannelId").toString());
        text += T("\n从站：") + QString::number(d->defaultParams.value("slaveId").toInt(1));
    }
    m_summary->setText(text);
}

// 把遍历项登记进台账；占位项改走手工添加
void DeviceMgrDlg::onRegisterProbe()
{
    QListWidgetItem* it = m_probeList->currentItem();
    if (!it) {
        return;
    }
    const int idx = it->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= static_cast<int>(m_probes.size())) {
        return;
    }
    const DeviceProbeInfo& p = m_probes[static_cast<size_t>(idx)];
    if (p.key.find("stub.") == 0) {
        onAddManual();
        return;
    }
    HardwareDevice d;
    d.id = ObjectId::create();
    d.kind = currentKind();
    d.name = p.title;
    d.defaultParams = p.params;
    if (d.kind == DeviceCamera) {
        d.cameraBackend = CameraBackendSdk;
    } else if (d.kind == DeviceFolder) {
        d.cameraBackend = CameraBackendFile;
    }
    m_sol->inventory.push_back(d);
    m_sol->dirty = true;
    refreshManaged();
}

// 手工加一台并打开配置
void DeviceMgrDlg::onAddManual()
{
    HardwareDevice d;
    d.id = ObjectId::create();
    d.kind = currentKind();
    if (d.kind == DeviceCamera) {
        d.cameraBackend = CameraBackendSdk;
        d.name = Translate::format("相机{0}", std::to_string(static_cast<int>(m_sol->inventory.size()) + 1));
    } else if (d.kind == DeviceFolder) {
        d.cameraBackend = CameraBackendFile;
        d.name = Translate::format("文件夹{0}", std::to_string(static_cast<int>(m_sol->inventory.size()) + 1));
        d.defaultParams.insert("folder", AppPaths::exeDir() + "/samples");
    } else if (d.kind == DeviceImage) {
        d.name = Translate::format("图像{0}", std::to_string(static_cast<int>(m_sol->inventory.size()) + 1));
    } else if (d.kind == DeviceControlCard) {
        d.name = Translate::format("控制卡{0}", std::to_string(static_cast<int>(m_sol->inventory.size()) + 1));
    } else {
        d.name = Translate::format("PLC{0}", std::to_string(static_cast<int>(m_sol->inventory.size()) + 1));
        d.defaultParams.insert("slaveId", 1);
    }
    m_sol->inventory.push_back(d);
    m_sol->dirty = true;
    refreshManaged();
    emit openDeviceRequested(qs(d.id));
}

// 确认后从台账移除当前设备
void DeviceMgrDlg::onRemove()
{
    const QString id = currentManagedId();
    if (id.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this, T("删除"), T("从台账移除该设备？")) != QMessageBox::Yes) {
        return;
    }
    for (size_t i = 0; i < m_sol->inventory.size(); ++i) {
        if (qs(m_sol->inventory[i].id) == id) {
            vpRemoveAt(m_sol->inventory, static_cast<int>(i));
            m_sol->dirty = true;
            break;
        }
    }
    refreshManaged();
}

// 请求打开当前台账设备的配置
void DeviceMgrDlg::onOpen()
{
    const QString id = currentManagedId();
    if (!id.isEmpty()) {
        emit openDeviceRequested(id);
    }
}

// 试打开再关闭，提示成败
void DeviceMgrDlg::onTestOpen()
{
    const HardwareDevice* d = m_sol->findDevice(oid(currentManagedId()));
    if (!d) {
        return;
    }
    if (d->kind == DeviceFolder) {
        const QString folder = qs(d->defaultParams.value("folder").toString());
        if (folder.isEmpty()) {
            QMessageBox::warning(this, T("连接测试"), T("还没有文件夹路径"));
        } else {
            QMessageBox::information(this, T("连接测试"), T("目录") + QString(QChar(0xFF1A)) + folder);
        }
        return;
    }
    if (d->kind == DeviceImage) {
        int count = 0;
        const std::string packed = d->defaultParams.value("images").toString();
        std::string cur;
        for (size_t i = 0; i < packed.size(); ++i) {
            if (packed[i] == '\n' || packed[i] == '\r') {
                if (!cur.empty()) { ++count; cur.clear(); }
            } else {
                cur += packed[i];
            }
        }
        if (!cur.empty()) { ++count; }
        if (count == 0) {
            QMessageBox::warning(this, T("连接测试"), T("还没有记录图像"));
        } else {
            QMessageBox::information(this, T("连接测试"), T("已记录") + QString::number(count) + T("张"));
        }
        return;
    }
    IDeviceControl* ctrl = nullptr;
    if (d->kind == DeviceCamera) {
        ctrl = &m_camCtrl;
    } else if (d->kind == DeviceControlCard) {
        ctrl = &m_cardCtrl;
    } else {
        ctrl = &m_plcCtrl;
    }
    if (ctrl->open(*d)) {
        QMessageBox::information(this, T("连接测试"), T("打开成功。"));
        ctrl->close();
    } else {
        QMessageBox::warning(this, T("连接测试"), qs(ctrl->lastError()));
    }
}

}

#include "moc_DeviceMgrDlg.cpp"
