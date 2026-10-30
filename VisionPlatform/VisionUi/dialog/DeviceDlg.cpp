/**
 * @file DeviceDlg.cpp
 * @brief DeviceDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "DeviceDlg.h"
#include "Solution.h"
#include "QtConv.h"
#include "RecipeDraft.h"
#include "Session.h"
#include "UiText.h"
#include "Theme.h"
#include "Perm.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace Vision {

namespace {

// 把图像路径拼成一行一条
std::string joinImageLines(QListWidget* list)
{
    std::string out;
    if (!list) {
        return out;
    }
    for (int i = 0; i < list->count(); ++i) {
        const QString one = list->item(i)->text().trimmed();
        if (one.isEmpty()) {
            continue;
        }
        if (!out.empty()) {
            out += "\n";
        }
        out += ss(one);
    }
    return out;
}

// 把已记录路径填进列表
void fillImageList(QListWidget* list, const std::string& packed)
{
    if (!list) {
        return;
    }
    list->clear();
    std::string cur;
    for (size_t i = 0; i < packed.size(); ++i) {
        const char c = packed[i];
        if (c == '\n' || c == '\r') {
            if (!cur.empty()) {
                list->addItem(qs(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        list->addItem(qs(cur));
    }
}

} // namespace
// 单设备配置
DeviceDlg::DeviceDlg(Session* session, Solution* solution, const QString& deviceId, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_sol(solution)
    , m_id(deviceId)
    , m_images(nullptr)
    , m_imageRow(nullptr)
{
    setWindowTitle(T("单设备配置"));
    resize(520, 460);
    enableDialogResize(this);
    // 控件：名称、种类，以及按种类显示的路径、图像列表、厂商地址、PLC 通道与从站号
    const bool can = session->has(PermEditDevice);
    m_name = new QLineEdit(this);
    m_kind = new QComboBox(this);
    m_kind->addItem(T("相机"), DeviceCamera);
    m_kind->addItem(T("控制卡"), DeviceControlCard);
    m_kind->addItem(T("PLC"), DevicePlc);
    m_kind->addItem(T("文件夹"), DeviceFolder);
    m_kind->addItem(T("图像"), DeviceImage);
    m_backend = new QComboBox(this);
    m_backend->addItem(T("文件源"), CameraBackendFile);
    m_backend->addItem(T("真相机"), CameraBackendSdk);
    m_path = new QLineEdit(this);
    QPushButton* browse = new QPushButton(T("浏览…"), this);
    connect(browse, SIGNAL(clicked()), this, SLOT(onBrowse()));
    m_pathRow = new QWidget(this);
    QHBoxLayout* pathLay = new QHBoxLayout(m_pathRow);
    pathLay->setContentsMargins(0, 0, 0, 0);
    pathLay->addWidget(m_path);
        pathLay->addWidget(browse);
    m_images = new QListWidget(this);
    m_images->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_images->setMinimumHeight(96);
    QPushButton* addImg = new QPushButton(T("添加"), this);
    QPushButton* delImg = new QPushButton(T("移除"), this);
    connect(addImg, SIGNAL(clicked()), this, SLOT(onAddImages()));
    connect(delImg, SIGNAL(clicked()), this, SLOT(onRemoveImage()));
    m_imageRow = new QWidget(this);
    QVBoxLayout* imgLay = new QVBoxLayout(m_imageRow);
    imgLay->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout* imgBtns = new QHBoxLayout();
    imgBtns->addWidget(addImg);
    imgBtns->addWidget(delImg);
    imgBtns->addStretch();
    imgLay->addWidget(m_images);
    imgLay->addLayout(imgBtns);
    m_vendor = new QLineEdit(this);
    m_address = new QLineEdit(this);
    m_comm = new QComboBox(this);
    m_slave = new QSpinBox(this);
    m_slave->setRange(1, 247);

    // 在 draft 上编辑，另存一份打开时快照用于判断是否改过
    HardwareDevice* formal = m_sol->findDevice(oid(m_id));
    if (formal) {
        m_draft.loadFrom(*formal);
        m_formalSnapshot.loadFrom(*formal);
    }
    m_name->setText(qs(m_draft.name));
    m_kind->setCurrentIndex(m_kind->findData(static_cast<int>(m_draft.kind)));
    m_backend->setCurrentIndex(m_backend->findData(static_cast<int>(m_draft.cameraBackend)));
    m_path->setText(qs(m_draft.defaultParams.value("folder").toString()));
    fillImageList(m_images, m_draft.defaultParams.value("images").toString());
    m_vendor->setText(qs(m_draft.defaultParams.value("vendor").toString()));
    m_address->setText(qs(m_draft.defaultParams.value("address").toString()));
    m_slave->setValue(m_draft.defaultParams.value("slaveId").toInt(1));
    reloadCommCombo();

    connect(m_name, SIGNAL(textEdited(QString)), this, SLOT(onEdited()));
    connect(m_kind, SIGNAL(currentIndexChanged(int)), this, SLOT(onKindChanged(int)));
    connect(m_backend, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
    connect(m_path, SIGNAL(textEdited(QString)), this, SLOT(onEdited()));
    connect(m_vendor, SIGNAL(textEdited(QString)), this, SLOT(onEdited()));
    connect(m_address, SIGNAL(textEdited(QString)), this, SLOT(onEdited()));
    connect(m_comm, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
    connect(m_slave, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));

    // 无编辑设备权限时全部只读
    m_name->setReadOnly(!can);
    m_kind->setEnabled(can);
    m_backend->setEnabled(can);
    m_path->setReadOnly(!can);
    browse->setEnabled(can);
    addImg->setEnabled(can);
    delImg->setEnabled(can);
    m_vendor->setReadOnly(!can);
    m_address->setReadOnly(!can);
    m_comm->setEnabled(can);
    m_slave->setEnabled(can);

    QFormLayout* form = new QFormLayout();
    form->addRow(T("名称"), m_name);
    form->addRow(T("类型"), m_kind);
        form->addRow(T("默认文件夹"), m_pathRow);
    form->addRow(T("图像列表"), m_imageRow);
    form->addRow(T("厂商"), m_vendor);
    form->addRow(T("地址"), m_address);
    form->addRow(T("通信通道"), m_comm);
    form->addRow(T("从站号"), m_slave);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, SIGNAL(accepted()), this, SLOT(onOk()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(form);
    lay->addWidget(box);
    syncFieldVisibility();
}

// 用方案通信通道填充 PLC 绑定下拉
void DeviceDlg::reloadCommCombo()
{
    const QString keep = qs(m_draft.defaultParams.value("commChannelId").toString());
    m_comm->clear();
    m_comm->addItem(T("（未绑定）"), QString());
    for (size_t i = 0; i < m_sol->commChannels.size(); ++i) {
        const CommChannel& c = m_sol->commChannels[i];
        m_comm->addItem(qs(c.name) + QLatin1String(" [")
                            + QString::fromUtf8(commChannelTypeTitle(c.type).c_str()) + QLatin1Char(']'),
                        qs(c.id));
    }
    const int idx = m_comm->findData(keep);
    m_comm->setCurrentIndex(idx >= 0 ? idx : 0);
}

// 按相机、控制卡、PLC 显隐字段
void DeviceDlg::syncFieldVisibility()
{
    const DeviceKind kind = static_cast<DeviceKind>(m_kind->currentData().toInt());
    const bool cam = kind == DeviceCamera;
    const bool card = kind == DeviceControlCard;
    const bool plc = kind == DevicePlc;
    const bool folder = kind == DeviceFolder;
    const bool images = kind == DeviceImage;
    m_backend->setVisible(false);
    m_pathRow->setVisible(folder);
    m_imageRow->setVisible(images);
    m_vendor->setVisible(card);
    m_address->setVisible(card);
    m_comm->setVisible(plc);
    m_slave->setVisible(plc);
}

// 种类变化后刷新可见字段并记入草稿
void DeviceDlg::onKindChanged(int)
{
    syncFieldVisibility();
    onEdited();
}

// 选择文件源目录
void DeviceDlg::onBrowse()
{
    const QString p = QFileDialog::getExistingDirectory(this, T("文件夹源"));
    if (!p.isEmpty()) {
        m_path->setText(p);
        onEdited();
    }
}

// 把界面字段写入设备草稿
void DeviceDlg::flushUiToDraft()
{
    if (!m_session->has(PermEditDevice)) {
        return;
    }
    m_draft.name = ss(m_name->text().trimmed());
    m_draft.kind = static_cast<DeviceKind>(m_kind->currentData().toInt());
    if (m_draft.kind == DeviceCamera) {
        m_draft.cameraBackend = CameraBackendSdk;
    } else if (m_draft.kind == DeviceFolder) {
        m_draft.cameraBackend = CameraBackendFile;
    }
    m_draft.defaultParams.insert("folder", ss(m_path->text()));
    m_draft.defaultParams.insert("images", joinImageLines(m_images));
    m_draft.defaultParams.insert("vendor", ss(m_vendor->text()));
    m_draft.defaultParams.insert("address", ss(m_address->text()));
    m_draft.defaultParams.insert("commChannelId", ss(m_comm->currentData().toString()));
    m_draft.defaultParams.insert("slaveId", m_slave->value());
}

// 控件改动后同步草稿与字段显隐
void DeviceDlg::onEdited()
{
    flushUiToDraft();
    syncFieldVisibility();
}

// 未保存时询问取消、丢弃或保存
int DeviceDlg::askCloseAction()
{
    flushUiToDraft();
    if (m_draft.equals(m_formalSnapshot)) {
        return 1;
    }
    const int q = askUnsavedCloseQuestion(this);
    if (q == 0) {
        return 0;
    }
    if (q == 2) {
        if (!trySave()) {
            return 0;
        }
        return 2;
    }
    return 1;
}

// 提交草稿到方案，失败则弹窗
bool DeviceDlg::trySave()
{
    if (!m_session->has(PermEditDevice)) {
        return true;
    }
    flushUiToDraft();
    if (m_draft.equals(m_formalSnapshot)) {
        return true;
    }
    HardwareDevice dev;
    dev.id = oid(m_id);
    m_draft.applyTo(&dev);
    if (!m_sol->commitDevice(dev)) {
        QMessageBox::warning(this, T("保存"), T("保存失败。"));
        return false;
    }
    m_formalSnapshot = m_draft;
    m_sol->dirty = true;
    return true;
}

// 关闭前确认是否保存
void DeviceDlg::reject()
{
    const int action = askCloseAction();
    if (action == 0) {
        return;
    }
    if (action == 2) {
        QDialog::accept();
        return;
    }
    QDialog::reject();
}

// 标题栏关闭时同样确认是否保存
void DeviceDlg::closeEvent(QCloseEvent* event)
{
    const int action = askCloseAction();
    if (action == 0) {
        event->ignore();
        return;
    }
    event->accept();
    if (action == 2) {
        QDialog::accept();
    } else {
        QDialog::reject();
    }
}

// 保存成功后关闭
void DeviceDlg::onOk()
{
    if (!trySave()) {
        return;
    }
    accept();
}


// 选择要加载的图像并记住路径
void DeviceDlg::onAddImages()
{
    if (!m_session->has(PermEditDevice) || !m_images) {
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(this, T("选择图像"), QString(),
        QLatin1String("Images (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)"));
    for (int i = 0; i < files.size(); ++i) {
        bool dup = false;
        for (int r = 0; r < m_images->count(); ++r) {
            if (m_images->item(r)->text() == files.at(i)) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            m_images->addItem(files.at(i));
        }
    }
    if (!files.isEmpty()) {
        onEdited();
    }
}

// 从列表移除选中的图像
void DeviceDlg::onRemoveImage()
{
    if (!m_session->has(PermEditDevice) || !m_images) {
        return;
    }
    const QList<QListWidgetItem*> picked = m_images->selectedItems();
    for (int i = 0; i < picked.size(); ++i) {
        const int row = m_images->row(picked.at(i));
        QListWidgetItem* it = m_images->takeItem(row);
        if (it) {
            delete it;
            it = nullptr;
        }
    }
    if (!picked.isEmpty()) {
        onEdited();
    }
}
}

#include "moc_DeviceDlg.cpp"
