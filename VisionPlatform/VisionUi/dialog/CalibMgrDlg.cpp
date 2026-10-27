/**
 * @file CalibMgrDlg.cpp
 * @brief CalibMgrDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibMgrDlg.h"
#include "CalibMgr.h"
#include "HardwareDevice.h"
#include "QtConv.h"
#include "Solution.h"
#include "StationEnums.h"
#include "Theme.h"
#include "UiText.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Vision {

namespace {

QString statusName(CalibStatus s)
{
    if (s == CalibValid) {
        return T("生效");
    }
    if (s == CalibExpired) {
        return T("失效");
    }
    return T("草稿");
}

QString fitName(CalibPlaneFit f)
{
    if (f == CalibFitScale) {
        return T("比例尺");
    }
    if (f == CalibFitHomography) {
        return T("透视");
    }
    return T("仿射");
}

QDoubleSpinBox* makeDouble(double minV, double maxV, int decimals)
{
    QDoubleSpinBox* box = new QDoubleSpinBox();
    box->setRange(minV, maxV);
    box->setDecimals(decimals);
    return box;
}

}

// 打开标定管理，左边选相机，右边改参数并看结果
CalibMgrDlg::CalibMgrDlg(Solution* solution, QWidget* parent)
    : QDialog(parent)
    , m_solution(solution)
    , m_loading(false)
    , m_list(nullptr)
    , m_bindTarget(nullptr)
    , m_bindBtn(nullptr)
    , m_kind(nullptr)
    , m_pages(nullptr)
    , m_tableBtn(nullptr)
    , m_plate(nullptr)
    , m_rows(nullptr)
    , m_cols(nullptr)
    , m_spacing(nullptr)
    , m_thickness(nullptr)
    , m_fit(nullptr)
    , m_resPx(nullptr)
    , m_resMm(nullptr)
    , m_angle(nullptr)
    , m_minImages(nullptr)
    , m_minAngles(nullptr)
    , m_width(nullptr)
    , m_height(nullptr)
    , m_result(nullptr)
{
    setWindowTitle(T("标定管理"));
    setModal(true);
    resize(920, 560);

    m_list = new QListWidget(this);
    connect(m_list, SIGNAL(currentRowChanged(int)), this, SLOT(onCameraChanged()));

    QPushButton* addBtn = new QPushButton(T("添加临时相机"), this);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddCamera()));
    m_bindTarget = new QComboBox(this);
    m_bindBtn = new QPushButton(T("绑定"), this);
    connect(m_bindBtn, SIGNAL(clicked()), this, SLOT(onBindCamera()));

    m_plate = new QComboBox(this);
    m_plate->addItem(T("棋盘格"), CalibPlateCheckerboard);
    m_plate->addItem(T("圆点阵"), CalibPlateCircleGrid);
    m_plate->addItem(T("Halcon 描述文件"), CalibPlateHalconCaltab);
    m_rows = new QSpinBox(this);
    m_rows->setRange(0, 200);
    m_cols = new QSpinBox(this);
    m_cols->setRange(0, 200);
    m_spacing = makeDouble(0.0, 10000.0, 4);
    m_thickness = makeDouble(0.0, 1000.0, 3);
    m_fit = new QComboBox(this);
    m_fit->addItem(T("比例尺"), CalibFitScale);
    m_fit->addItem(T("仿射"), CalibFitAffine);
    m_fit->addItem(T("透视"), CalibFitHomography);
    m_resPx = makeDouble(0.0, 1000.0, 3);
    m_resMm = makeDouble(0.0, 1000.0, 4);
    m_angle = makeDouble(-360.0, 360.0, 3);
    m_minImages = new QSpinBox(this);
    m_minImages->setRange(1, 100);
    m_minAngles = new QSpinBox(this);
    m_minAngles->setRange(3, 36);
    m_width = new QSpinBox(this);
    m_width->setRange(0, 20000);
    m_height = new QSpinBox(this);
    m_height->setRange(0, 20000);

    m_kind = new QComboBox(this);
    m_kind->addItem(T("标定板"), 0);
    m_kind->addItem(T("九点标定"), 1);
    m_kind->addItem(T("旋转标定"), 2);
    connect(m_kind, SIGNAL(currentIndexChanged(int)), this, SLOT(onKindChanged()));

    QFormLayout* plateForm = new QFormLayout();
    plateForm->addRow(T("板型"), m_plate);
    plateForm->addRow(T("行"), m_rows);
    plateForm->addRow(T("列"), m_cols);
    plateForm->addRow(T("间距 mm"), m_spacing);
    plateForm->addRow(T("板厚 mm"), m_thickness);
    plateForm->addRow(T("像素残差上限"), m_resPx);
    plateForm->addRow(T("标定板最少张数"), m_minImages);
    QWidget* platePage = new QWidget(this);
    platePage->setLayout(plateForm);

    QFormLayout* pointForm = new QFormLayout();
    pointForm->addRow(T("拟合"), m_fit);
    pointForm->addRow(T("毫米残差上限"), m_resMm);
    QWidget* pointPage = new QWidget(this);
    pointPage->setLayout(pointForm);

    QFormLayout* rotForm = new QFormLayout();
    rotForm->addRow(T("示教角度"), m_angle);
    rotForm->addRow(T("旋转最少角度"), m_minAngles);
    QWidget* rotPage = new QWidget(this);
    rotPage->setLayout(rotForm);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(platePage);
    m_pages->addWidget(pointPage);
    m_pages->addWidget(rotPage);

    QFormLayout* form = new QFormLayout();
    form->addRow(T("方式"), m_kind);
    form->addRow(m_pages);
    form->addRow(T("图像宽"), m_width);
    form->addRow(T("图像高"), m_height);
    QPushButton* saveBtn = new QPushButton(T("保存参数"), this);
    m_tableBtn = new QPushButton(T("导入点表"), this);
    QPushButton* resultBtn = new QPushButton(T("导入结果"), this);
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSaveParam()));
    connect(m_tableBtn, SIGNAL(clicked()), this, SLOT(onImportTable()));
    connect(resultBtn, SIGNAL(clicked()), this, SLOT(onImportResult()));
    QHBoxLayout* btns = new QHBoxLayout();
    btns->addWidget(saveBtn);
    btns->addWidget(m_tableBtn);
    btns->addWidget(resultBtn);
    btns->addStretch();
    form->addRow(btns);

    QGroupBox* paramBox = new QGroupBox(T("参数"), this);
    paramBox->setLayout(form);
    onKindChanged();
    m_result = new QPlainTextEdit(this);
    m_result->setReadOnly(true);
    QGroupBox* resultBox = new QGroupBox(T("生效结果"), this);
    QVBoxLayout* resultLay = new QVBoxLayout(resultBox);
    resultLay->addWidget(m_result);

    QVBoxLayout* left = new QVBoxLayout();
    left->addWidget(new QLabel(T("相机"), this));
    left->addWidget(m_list, 1);
    left->addWidget(addBtn);
    left->addWidget(new QLabel(T("绑定到台账相机"), this));
    left->addWidget(m_bindTarget);
    left->addWidget(m_bindBtn);
    QWidget* leftW = new QWidget(this);
    leftW->setLayout(left);
    leftW->setMinimumWidth(220);

    QVBoxLayout* right = new QVBoxLayout();
    right->addWidget(paramBox);
    right->addWidget(resultBox, 1);
    QWidget* rightW = new QWidget(this);
    rightW->setLayout(right);

    QHBoxLayout* body = new QHBoxLayout();
    body->addWidget(leftW);
    body->addWidget(rightW, 1);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(body, 1);

    reloadCameras(QString());
}

// 当前列表选中的相机 id
QString CalibMgrDlg::currentId() const
{
    QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return QString();
    }
    return item->data(Qt::UserRole).toString();
}

// 台账相机在前，还没绑定的标定接在后面
void CalibMgrDlg::reloadCameras(const QString& selectId)
{
    m_loading = true;
    m_list->clear();
    m_bindTarget->clear();
    QStringList seen;
    if (m_solution) {
        for (size_t i = 0; i < m_solution->inventory.size(); ++i) {
            const HardwareDevice& dev = m_solution->inventory[i];
            if (dev.kind != DeviceCamera) {
                continue;
            }
            const QString id = qs(dev.id);
            seen.append(id);
            const QString title = qs(dev.name).isEmpty() ? id : qs(dev.name);
            QListWidgetItem* item = new QListWidgetItem(title, m_list);
            item->setData(Qt::UserRole, id);
            m_bindTarget->addItem(title, id);
        }
    }
    const std::vector<ObjectId> extra = CalibMgr::instance().cameras();
    for (size_t i = 0; i < extra.size(); ++i) {
        const QString id = qs(extra[i]);
        if (seen.contains(id)) {
            continue;
        }
        QString title = qs(CalibMgr::instance().cameraName(extra[i]));
        if (title.isEmpty()) {
            title = id;
        }
        title = T("临时·") + title;
        QListWidgetItem* item = new QListWidgetItem(title, m_list);
        item->setData(Qt::UserRole, id);
    }
    int pick = 0;
    if (!selectId.isEmpty()) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(Qt::UserRole).toString() == selectId) {
                pick = i;
                break;
            }
        }
    }
    m_loading = false;
    if (m_list->count() > 0) {
        m_list->setCurrentRow(pick);
    } else {
        m_currentId.clear();
        showResult();
    }
}

// 把这路相机的参数填入表单
void CalibMgrDlg::loadForm()
{
    const QString id = currentId();
    m_currentId = id;
    const bool ok = !id.isEmpty();
    m_plate->setEnabled(ok);
    if (!ok) {
        showResult();
        return;
    }
    const ObjectId oid(ss(id));
    const CalibParam p = CalibMgr::instance().param(oid);
    m_loading = true;
    int plateIndex = m_plate->findData(static_cast<int>(p.plateKind));
    m_plate->setCurrentIndex(plateIndex < 0 ? 0 : plateIndex);
    m_rows->setValue(p.rows);
    m_cols->setValue(p.cols);
    m_spacing->setValue(p.spacingMm);
    m_thickness->setValue(p.thicknessMm);
    int fitIndex = m_fit->findData(static_cast<int>(p.planeFit));
    m_fit->setCurrentIndex(fitIndex < 0 ? 1 : fitIndex);
    m_resPx->setValue(p.residualMaxPx);
    m_resMm->setValue(p.residualMaxMm);
    m_angle->setValue(p.angleRefDeg);
    m_minImages->setValue(p.minPlateImages);
    m_minAngles->setValue(p.minRotateAngles);
    int iw = 0;
    int ih = 0;
    CalibMgr::instance().imageSize(oid, &iw, &ih);
    m_width->setValue(iw);
    m_height->setValue(ih);
    m_loading = false;
    showResult();
    bool inLedger = false;
    if (m_solution && m_solution->findDevice(oid) && m_solution->findDevice(oid)->kind == DeviceCamera) {
        inLedger = true;
    }
    m_bindBtn->setEnabled(!inLedger && m_bindTarget->count() > 0);
}

// 显示三份生效结果
void CalibMgrDlg::showResult()
{
    if (m_currentId.isEmpty()) {
        m_result->setPlainText(T("请先选择相机"));
        return;
    }
    const ObjectId oid(ss(m_currentId));
    const CalibRst r = CalibMgr::instance().active(oid);
    QString text;
    text += T("相机模型：");
    if (!r.camera.present) {
        text += T("未标定");
    } else {
        text += statusName(r.camera.status);
        text += QString("  rms=%1  max=%2").arg(r.camera.rms, 0, 'f', 3).arg(r.camera.maxResidual, 0, 'f', 3);
    }
    text += QLatin1Char('\n');
    text += T("平面映射：");
    if (!r.plane.present) {
        text += T("未标定");
    } else {
        text += fitName(r.plane.fit) + QLatin1Char(' ') + statusName(r.plane.status);
        text += QString("  rms=%1 mm").arg(r.plane.rms, 0, 'f', 4);
        if (r.plane.fit == CalibFitScale) {
            text += QString("  %1 mm/px").arg(r.plane.mmPerPixel, 0, 'f', 5);
        }
    }
    text += QLatin1Char('\n');
    text += T("旋转中心：");
    if (!r.rotate.present) {
        text += T("未标定");
    } else {
        text += statusName(r.rotate.status);
        text += QString("  (%1, %2)  r=%3").arg(r.rotate.imageX, 0, 'f', 2).arg(r.rotate.imageY, 0, 'f', 2).arg(r.rotate.radius, 0, 'f', 2);
        if (r.rotate.worldValid) {
            text += QString("  world (%1, %2)").arg(r.rotate.worldX, 0, 'f', 3).arg(r.rotate.worldY, 0, 'f', 3);
        }
    }
    const CalibRst draft = CalibMgr::instance().draft(oid);
    if (draft.camera.present || draft.plane.present || draft.rotate.present) {
        text += QLatin1String("\ndraft");
        if (draft.camera.present) {
            text += QString(" cam=%1").arg(draft.camera.rms, 0, 'f', 3);
        }
        if (draft.plane.present) {
            text += QString(" plane=%1").arg(draft.plane.rms, 0, 'f', 4);
        }
        if (draft.rotate.present) {
            text += QString(" rot=%1").arg(draft.rotate.rms, 0, 'f', 3);
        }
    }
    const CalibKind kinds[3] = { CalibKindPlate, CalibKindPlane, CalibKindRotate };
    for (int k = 0; k < 3; ++k) {
        const std::vector<CalibHistItem> hist = CalibMgr::instance().history(oid, kinds[k]);
        for (size_t i = 0; i < hist.size(); ++i) {
            text += QLatin1String("\n");
            text += QString::fromUtf8(hist[i].fileName.c_str());
        }
    }
    const std::string offline = CalibMgr::instance().offlineDir(oid);
    if (!offline.empty()) {
        text += QLatin1String("\n");
        text += QString::fromUtf8(offline.c_str());
    }
    m_result->setPlainText(text);
}


// 只显示当前标定方式的参数。点表用于九点和旋转
void CalibMgrDlg::onKindChanged()
{
    if (!m_kind || !m_pages) {
        return;
    }
    const int k = m_kind->currentIndex();
    if (k >= 0 && k < m_pages->count()) {
        m_pages->setCurrentIndex(k);
    }
    if (m_tableBtn) {
        m_tableBtn->setVisible(k != 0);
    }
    QWidget* host = (k == 2) ? m_pages->widget(2) : m_pages->widget(0);
    QFormLayout* form = host ? qobject_cast<QFormLayout*>(host->layout()) : nullptr;
    if (!form || !m_resPx || k == 1) {
        return;
    }
    if (form->labelForField(m_resPx)) {
        return;
    }
    QWidget* otherPage = (k == 2) ? m_pages->widget(0) : m_pages->widget(2);
    QFormLayout* other = otherPage ? qobject_cast<QFormLayout*>(otherPage->layout()) : nullptr;
    QLabel* lab = nullptr;
    if (other) {
        lab = qobject_cast<QLabel*>(other->labelForField(m_resPx));
        other->removeWidget(m_resPx);
        if (lab) {
            other->removeWidget(lab);
        }
    }
    if (!lab) {
        lab = new QLabel(T("像素残差上限"), this);
    }
    form->addRow(lab, m_resPx);
}
// 换一路相机
void CalibMgrDlg::onCameraChanged()
{
    if (m_loading) {
        return;
    }
    loadForm();
}

// 保存当前相机的标定参数
void CalibMgrDlg::onSaveParam()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), T("请先选择相机"));
        return;
    }
    CalibParam p;
    p.plateKind = static_cast<CalibPlateKind>(m_plate->currentData().toInt());
    p.rows = m_rows->value();
    p.cols = m_cols->value();
    p.spacingMm = m_spacing->value();
    p.thicknessMm = m_thickness->value();
    p.planeFit = static_cast<CalibPlaneFit>(m_fit->currentData().toInt());
    p.residualMaxPx = m_resPx->value();
    p.residualMaxMm = m_resMm->value();
    p.angleRefDeg = m_angle->value();
    p.minPlateImages = m_minImages->value();
    p.minRotateAngles = m_minAngles->value();
    const ObjectId oid(ss(id));
    std::string err;
    if (!CalibMgr::instance().setParam(oid, p, &err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    CalibMgr::instance().setImageSize(oid, m_width->value(), m_height->value(), &err);
    if (!CalibMgr::instance().save(&err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    showResult();
}

// 导入点表并按当前拟合方式提交
void CalibMgrDlg::onImportTable()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), T("请先选择相机"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, T("导入点表"), QString(), T("点表 (*.csv *.txt);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    std::string err;
    if (!CalibMgr::instance().importPointTable(ObjectId(ss(id)), ss(path), &err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    if (!CalibMgr::instance().save(&err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    showResult();
}

// 导入已算好的相机模型、平面映射或旋转中心
void CalibMgrDlg::onImportResult()
{
    const QString id = currentId();
    if (id.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), T("请先选择相机"));
        return;
    }
    const QString path = QFileDialog::getOpenFileName(this, T("导入结果"), QString(), T("json (*.json);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    std::string err;
    if (!CalibMgr::instance().importResult(ObjectId(ss(id)), ss(path), &err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    if (!CalibMgr::instance().save(&err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    showResult();
}

// 添加一路还没进台账的临时相机
void CalibMgrDlg::onAddCamera()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, windowTitle(), T("名称"), QLineEdit::Normal, T("临时相机"), &ok);
    if (!ok) {
        return;
    }
    const ObjectId id = ObjectId::create();
    std::string err;
    if (!CalibMgr::instance().setParam(id, CalibParam(), &err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    CalibMgr::instance().setCameraName(id, ss(name), &err);
    if (!CalibMgr::instance().save(&err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
    }
    reloadCameras(qs(id));
}

// 把当前临时标定绑到选中的台账相机
void CalibMgrDlg::onBindCamera()
{
    const QString from = currentId();
    if (from.isEmpty() || m_bindTarget->currentIndex() < 0) {
        return;
    }
    const QString to = m_bindTarget->currentData().toString();
    std::string err;
    if (!CalibMgr::instance().bindCamera(ObjectId(ss(from)), ObjectId(ss(to)), &err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
        return;
    }
    if (!CalibMgr::instance().save(&err)) {
        QMessageBox::warning(this, windowTitle(), qs(err));
    }
    reloadCameras(to);
}

}

#include "moc_CalibMgrDlg.cpp"