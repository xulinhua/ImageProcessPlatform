/**
 * @file CamPipelineEditors.cpp
 * @brief CamPipelineEditors实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditors.h"
#include "Translate.h"
#include "ToolParamUi.h"
#include "QtConv.h"
#include "UiText.h"
#include "Session.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace Vision {
// 第i步参数键
static std::string stepKey(int i, const char* suffix)
{
    return std::string("pl_") + std::to_string(static_cast<long long>(i)) + "_" + suffix;
}

// 增强算子
enum class EnhanceOp {
    EquHisto = 0,
    Emphasize,
    Illuminate,
    Scale,
    Invert
};

// 增强管线一步
struct EnhanceStep {
    EnhanceOp op;
    int maskW;
    int maskH;
    double coeff;
    double coeff2;
    bool scaleMax;
    EnhanceStep()
        : op(EnhanceOp::EquHisto)
        , maskW(7)
        , maskH(7)
        , coeff(1.0)
        , coeff2(0.0)
        , scaleMax(false)
    {}
};

// 整型转增强算子
static EnhanceOp enhanceOpFromInt(int v)
{
    switch (v) {
    case 0: return EnhanceOp::EquHisto;
    case 1: return EnhanceOp::Emphasize;
    case 2: return EnhanceOp::Illuminate;
    case 3: return EnhanceOp::Scale;
    case 4: return EnhanceOp::Invert;
    default: return EnhanceOp::EquHisto;
    }
}

// 从参数解开增强步骤
static std::vector<EnhanceStep> enhanceStepsFromParams(const ParamMap& p)
{
    std::vector<EnhanceStep> out;
    const int n = p.value("pl_n").toInt(0);
    if (n <= 0) {
        return out;
    }
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        EnhanceStep st;
        st.op = enhanceOpFromInt(p.value(stepKey(i, "op")).toInt());
        st.maskW = p.value(stepKey(i, "w")).toInt(st.maskW);
        st.maskH = p.value(stepKey(i, "h")).toInt(st.maskH);
        st.coeff = p.value(stepKey(i, "c0")).toDouble(st.coeff);
        st.coeff2 = p.value(stepKey(i, "c1")).toDouble(st.coeff2);
        st.scaleMax = p.value(stepKey(i, "f")).toInt(0) != 0;
        out.push_back(st);
    }
    return out;
}

// 平滑算子
enum class SmoothOp {
    Mean = 0,
    Median,
    Gauss,
    Bilateral
};

// 平滑管线一步
struct SmoothStep {
    SmoothOp op;
    int maskW;
    int maskH;
    double sigmaRange;
    double sigmaSpace;
    SmoothStep()
        : op(SmoothOp::Median)
        , maskW(3)
        , maskH(3)
        , sigmaRange(20.0)
        , sigmaSpace(3.0)
    {}
};

// 整型转平滑算子
static SmoothOp smoothOpFromInt(int v)
{
    switch (v) {
    case 0: return SmoothOp::Mean;
    case 1: return SmoothOp::Median;
    case 2: return SmoothOp::Gauss;
    case 3: return SmoothOp::Bilateral;
    default: return SmoothOp::Median;
    }
}

// 从参数解开平滑步骤
static std::vector<SmoothStep> smoothStepsFromParams(const ParamMap& p)
{
    std::vector<SmoothStep> out;
    const int n = p.value("pl_n").toInt(0);
    if (n <= 0) {
        return out;
    }
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        SmoothStep st;
        st.op = smoothOpFromInt(p.value(stepKey(i, "op")).toInt());
        st.maskW = p.value(stepKey(i, "w")).toInt(st.maskW);
        st.maskH = p.value(stepKey(i, "h")).toInt(st.maskH);
        st.sigmaRange = p.value(stepKey(i, "c0")).toDouble(st.sigmaRange);
        st.sigmaSpace = p.value(stepKey(i, "c1")).toDouble(st.sigmaSpace);
        out.push_back(st);
    }
    return out;
}

// 平滑一步

// 算子写成整型
static int enhanceOpToInt(EnhanceOp op)
{
    return static_cast<int>(op);
}

// 算子写成整型
static int smoothOpToInt(SmoothOp op)
{
    return static_cast<int>(op);
}

// 算子显示名
static std::string enhanceOpTitle(EnhanceOp op)
{
    switch (op) {
    case EnhanceOp::EquHisto: return Translate::tr("直方图均衡");
    case EnhanceOp::Emphasize: return Translate::tr("强调");
    case EnhanceOp::Illuminate: return Translate::tr("照度");
    case EnhanceOp::Scale: return Translate::tr("灰度缩放");
    case EnhanceOp::Invert: return Translate::tr("反色");
    default: return Translate::tr("直方图均衡");
    }
}

// 算子显示名
static std::string smoothOpTitle(SmoothOp op)
{
    switch (op) {
    case SmoothOp::Mean: return Translate::tr("均值");
    case SmoothOp::Median: return Translate::tr("中值");
    case SmoothOp::Gauss: return Translate::tr("高斯");
    case SmoothOp::Bilateral: return Translate::tr("双边");
    default: return Translate::tr("中值");
    }
}

// 把步骤写回参数
static void clearPipelineParams(ParamMap* p)
{
    if (!p) {
        return;
    }
    const std::vector<std::string> keys = p->keys();
    for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
        const std::string& key = keys[static_cast<size_t>(i)];
        if (key == "pl_n" || key == "pipeline" || (key.size() >= 3 && key.compare(0, 3, "pl_") == 0)) {
            p->raw().erase(key);
        }
    }
}

// 把步骤写回参数
static void enhanceStepsToParams(const std::vector<EnhanceStep>& steps, ParamMap* p)
{
    if (!p) {
        return;
    }
    clearPipelineParams(p);
    p->insert("pl_n", static_cast<int>(steps.size()));
    for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
        const EnhanceStep& st = steps[static_cast<size_t>(i)];
        p->insert(stepKey(i, "op"), enhanceOpToInt(st.op));
        p->insert(stepKey(i, "w"), st.maskW);
        p->insert(stepKey(i, "h"), st.maskH);
        p->insert(stepKey(i, "c0"), st.coeff);
        p->insert(stepKey(i, "c1"), st.coeff2);
        p->insert(stepKey(i, "f"), st.scaleMax ? 1 : 0);
    }
}

// 把步骤写回参数
static void smoothStepsToParams(const std::vector<SmoothStep>& steps, ParamMap* p)
{
    if (!p) {
        return;
    }
    clearPipelineParams(p);
    p->insert("pl_n", static_cast<int>(steps.size()));
    for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
        const SmoothStep& st = steps[static_cast<size_t>(i)];
        p->insert(stepKey(i, "op"), smoothOpToInt(st.op));
        p->insert(stepKey(i, "w"), st.maskW);
        p->insert(stepKey(i, "h"), st.maskH);
        p->insert(stepKey(i, "c0"), st.sigmaRange);
        p->insert(stepKey(i, "c1"), st.sigmaSpace);
    }
}

// 建一个两项下拉，显示文案与存盘值成对
static QComboBox* combo(QWidget* parent, const char* a, const char* av, const char* b, const char* bv)
{
    QComboBox* c = mkParamCombo(parent);
    c->addItem(T(a), QLatin1String(av));
    c->addItem(T(b), QLatin1String(bv));
    return c;
}

// 按存盘值选中下拉项，找不到则第一项
static void setCombo(QComboBox* c, const QString& v)
{
    const int i = c->findData(v);
    c->setCurrentIndex(i >= 0 ? i : 0);
}

// 当前下拉项的存盘值
static QString comboVal(QComboBox* c)
{
    return c->currentData().toString();
}

// 建参数用浮点旋钮
static QDoubleSpinBox* dspin(QWidget* parent, double lo, double hi, int dec, double step)
{
    return mkParamDSpin(parent, lo, hi, dec, step);
}

// ---------- 相机 ----------
class CameraEditor : public ToolEditorBase {
public:
    CameraEditor(QWidget* parent, bool fileMode)
        : ToolEditorBase(parent)
        , m_file(fileMode)
    {
        m_dev = mkParamCombo(this);
        m_path = new QLineEdit(this);
        m_path->setObjectName(QLatin1String("toolDebugParamLine"));
        m_mode = combo(this, "文件夹循环", "folder", "单个图像", "file");
        m_exp = mkParamSpin(this, 1, 10000000);
        m_gain = dspin(this, 0, 48, 1, 0.5);
        QPushButton* browse = new QPushButton(T("浏览…"), this);
        browse->setFixedHeight(28);
        connect(browse, &QPushButton::clicked, this, &CameraEditor::onBrowse);
        QWidget* pathWrap = new QWidget(this);
        QHBoxLayout* pathLay = new QHBoxLayout(pathWrap);
        pathLay->setContentsMargins(0, 0, 0, 0);
        pathLay->setSpacing(8);
        pathLay->addWidget(m_path, 1);
        pathLay->addWidget(browse);
        pathWrap->setFixedHeight(28);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamCell(this, T("引用设备"), m_dev));
        if (fileMode) {
            addParamStackItem(lay, mkParamCell(this, T("来源"), m_mode));
        }
        addParamStackItem(lay, mkParamCell(this, T("路径覆盖"), pathWrap));
        addParamStackItem(lay, mkParamRow2(this, T("曝光 us"), m_exp, T("增益 dB"), m_gain));
        m_mode->setVisible(fileMode);
        connect(m_dev, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_path, SIGNAL(editingFinished()), this, SLOT(onEdited()));
        connect(m_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_exp, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_gain, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        m_dev->clear();
        if (m_sol) {
            for (int i = 0; i < static_cast<int>(m_sol->inventory.size()); ++i) {
                const HardwareDevice& d = m_sol->inventory[static_cast<size_t>(i)];
                if (d.kind != DeviceCamera) {
                    continue;
                }
                QString label = qs(d.name);
                if (d.cameraBackend == CameraBackendFile) {
                    label += T(" [文件源]");
                }
                m_dev->addItem(label, qs(d.id));
            }
        }
        if (!m_tool) {
            return;
        }
        const int ix = m_dev->findData(qs(m_tool->deviceId));
        m_dev->setCurrentIndex(ix >= 0 ? ix : 0);
        m_path->setText(qs(m_tool->params.value("path").toString()));
        setCombo(m_mode, qs(m_tool->params.value("sourceMode").toString()));
        m_exp->setValue(m_tool->params.value("exposureUs").toInt(10000));
        m_gain->setValue(m_tool->params.value("gainDb").toDouble());
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        const std::string oldPath = m_tool->params.value("path").toString();
        const std::string oldMode = m_tool->params.value("sourceMode").toString();
        const std::string newPath = ss(m_path->text().trimmed());
        const std::string newMode = m_file ? ss(comboVal(m_mode)) : oldMode;
        m_tool->deviceId = oid(m_dev->currentData().toString());
        m_tool->params.insert("path", newPath);
        if (m_file) {
            m_tool->params.insert("sourceMode", newMode);
        }
        // 路径或来源方式变了，从第一张重新循环
        if (newPath != oldPath || newMode != oldMode) {
            m_tool->params.insert("index", 0);
            m_tool->params.raw().erase("grabPath");
            m_tool->params.insert("pathChanged", 1);
        }
        m_tool->params.insert("exposureUs", m_exp->value());
        m_tool->params.insert("gainDb", m_gain->value());
    }

private:
    void onBrowse()
    {
        QString p;
        const bool single = m_file && comboVal(m_mode) == QLatin1String("file");
        if (single) {
            p = QFileDialog::getOpenFileName(this, T("单个图像"), QString(),
                                             T("图像 (*.png *.bmp *.jpg *.jpeg *.tif);;所有文件 (*.*)"));
        } else {
            p = QFileDialog::getExistingDirectory(this, T("图像文件夹"));
        }
        if (!p.isEmpty()) {
            m_path->setText(p);
            onEdited();
        }
    }
    bool m_file;
    QComboBox* m_dev;
    QLineEdit* m_path;
    QComboBox* m_mode;
    QSpinBox* m_exp;
    QDoubleSpinBox* m_gain;
};

// 相机设备采集参数页
ToolEditorBase* createCamDeviceEditor(QWidget* parent)
{
    return new CameraEditor(parent, false);
}
// 文件源采集参数页
ToolEditorBase* createCameraFileEditor(QWidget* parent)
{
    return new CameraEditor(parent, true);
}

// ---------- 增强/平滑管线 ----------
namespace {

enum StepVarRole {
    StepVarOp = 0,
    StepVarMaskW,
    StepVarMaskH,
    StepVarCoeff,
    StepVarCoeff2,
    StepVarFlag
};

// 增强步骤写入 QVariantList
QVariantList enhanceStepToVariant(const EnhanceStep& st)
{
    QVariantList v;
    v << enhanceOpToInt(st.op) << st.maskW << st.maskH << st.coeff << st.coeff2 << (st.scaleMax ? 1 : 0);
    return v;
}

// QVariantList 读增强步骤
EnhanceStep enhanceStepFromVariant(const QVariant& data)
{
    const QVariantList v = data.toList();
    EnhanceStep st;
    if (v.size() >= 6) {
        st.op = enhanceOpFromInt(v[StepVarOp].toInt());
        st.maskW = v[StepVarMaskW].toInt();
        st.maskH = v[StepVarMaskH].toInt();
        st.coeff = v[StepVarCoeff].toDouble();
        st.coeff2 = v[StepVarCoeff2].toDouble();
        st.scaleMax = v[StepVarFlag].toInt() != 0;
    }
    return st;
}

// 平滑步骤写入 QVariantList
QVariantList smoothStepToVariant(const SmoothStep& st)
{
    QVariantList v;
    v << smoothOpToInt(st.op) << st.maskW << st.maskH << st.sigmaRange << st.sigmaSpace << 0;
    return v;
}

// QVariantList 读平滑步骤
SmoothStep smoothStepFromVariant(const QVariant& data)
{
    const QVariantList v = data.toList();
    SmoothStep st;
    if (v.size() >= 5) {
        st.op = smoothOpFromInt(v[StepVarOp].toInt());
        st.maskW = v[StepVarMaskW].toInt();
        st.maskH = v[StepVarMaskH].toInt();
        st.sigmaRange = v[StepVarCoeff].toDouble();
        st.sigmaSpace = v[StepVarCoeff2].toDouble();
    }
    return st;
}

}

class PipelineEditor : public ToolEditorBase {
public:
    PipelineEditor(QWidget* parent, bool enhance)
        : ToolEditorBase(parent)
        , m_enhance(enhance)
    {
        m_list = new QListWidget(this);
        m_op = new QComboBox(this);
        if (enhance) {
            m_op->addItem(T("直方图均衡"), enhanceOpToInt(EnhanceOp::EquHisto));
            m_op->addItem(T("强调"), enhanceOpToInt(EnhanceOp::Emphasize));
            m_op->addItem(T("照度"), enhanceOpToInt(EnhanceOp::Illuminate));
            m_op->addItem(T("灰度缩放"), enhanceOpToInt(EnhanceOp::Scale));
            m_op->addItem(T("反色"), enhanceOpToInt(EnhanceOp::Invert));
        } else {
            m_op->addItem(T("均值"), smoothOpToInt(SmoothOp::Mean));
            m_op->addItem(T("中值"), smoothOpToInt(SmoothOp::Median));
            m_op->addItem(T("高斯"), smoothOpToInt(SmoothOp::Gauss));
            m_op->addItem(T("双边"), smoothOpToInt(SmoothOp::Bilateral));
        }
        QPushButton* add = new QPushButton(T("添加步骤"), this);
        QPushButton* del = new QPushButton(T("删除步骤"), this);
        QPushButton* up = new QPushButton(T("上移"), this);
        QPushButton* dn = new QPushButton(T("下移"), this);
        m_w = new QSpinBox(this);
        m_w->setRange(1, 501);
        m_h = new QSpinBox(this);
        m_h->setRange(1, 501);
        m_factor = dspin(this, 0.01, 20, 2, 0.1);
        QFormLayout* args = new QFormLayout();
        args->addRow(T("宽/核"), m_w);
        args->addRow(T("高"), m_h);
        args->addRow(T("系数"), m_factor);
        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->addWidget(new QLabel(T("可叠加步骤，顺序即执行顺序"), this));
        lay->addWidget(m_list, 1);
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(m_op);
        row->addWidget(add);
        lay->addLayout(row);
        QHBoxLayout* row2 = new QHBoxLayout();
        row2->addWidget(up);
        row2->addWidget(dn);
        row2->addWidget(del);
        lay->addLayout(row2);
        lay->addLayout(args);
        connect(add, &QPushButton::clicked, this, &PipelineEditor::onAdd);
        connect(del, &QPushButton::clicked, this, &PipelineEditor::onDel);
        connect(up, &QPushButton::clicked, this, &PipelineEditor::onUp);
        connect(dn, &QPushButton::clicked, this, &PipelineEditor::onDown);
        connect(m_list, &QListWidget::currentItemChanged, this, &PipelineEditor::onItemChanged);
        connect(m_w, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_h, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_factor, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        m_block = true;
        m_list->clear();
        if (m_tool) {
            if (m_enhance) {
                const std::vector<EnhanceStep> steps = enhanceStepsFromParams(m_tool->params);
                for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
                    addEnhanceItem(steps[static_cast<size_t>(i)]);
                }
            } else {
                const std::vector<SmoothStep> steps = smoothStepsFromParams(m_tool->params);
                for (int i = 0; i < static_cast<int>(steps.size()); ++i) {
                    addSmoothItem(steps[static_cast<size_t>(i)]);
                }
            }
        }
        if (m_list->count() > 0) {
            m_list->setCurrentRow(0);
            loadArgsFromItem(m_list->currentItem());
        } else {
            updateArgWidgets();
        }
        m_block = false;
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        writeCurrentArgs();
        if (m_enhance) {
            std::vector<EnhanceStep> steps;
            for (int i = 0; i < m_list->count(); ++i) {
                steps.push_back(enhanceItemStep(i));
            }
            enhanceStepsToParams(steps, &m_tool->params);
        } else {
            std::vector<SmoothStep> steps;
            for (int i = 0; i < m_list->count(); ++i) {
                steps.push_back(smoothItemStep(i));
            }
            smoothStepsToParams(steps, &m_tool->params);
        }
    }

private:
    void addEnhanceItem(const EnhanceStep& st)
    {
        QListWidgetItem* it = new QListWidgetItem(QString::fromUtf8(enhanceOpTitle(st.op).c_str()), m_list);
        it->setData(Qt::UserRole, enhanceStepToVariant(st));
    }
    void addSmoothItem(const SmoothStep& st)
    {
        QListWidgetItem* it = new QListWidgetItem(QString::fromUtf8(smoothOpTitle(st.op).c_str()), m_list);
        it->setData(Qt::UserRole, smoothStepToVariant(st));
    }
    EnhanceStep enhanceItemStep(int row)
    {
        if (row < 0 || row >= m_list->count()) {
            return EnhanceStep();
        }
        return enhanceStepFromVariant(m_list->item(row)->data(Qt::UserRole));
    }
    SmoothStep smoothItemStep(int row)
    {
        if (row < 0 || row >= m_list->count()) {
            return SmoothStep();
        }
        return smoothStepFromVariant(m_list->item(row)->data(Qt::UserRole));
    }
    void writeArgsToItem(QListWidgetItem* it)
    {
        if (!it) {
            return;
        }
        if (m_enhance) {
            EnhanceStep st = enhanceStepFromVariant(it->data(Qt::UserRole));
            switch (st.op) {
            case EnhanceOp::Emphasize:
            case EnhanceOp::Illuminate:
                st.maskW = m_w->value();
                st.maskH = m_h->value();
                st.coeff = m_factor->value();
                break;
            case EnhanceOp::Scale:
                st.coeff = m_factor->value();
                st.coeff2 = 0.0;
                st.scaleMax = false;
                break;
            default:
                break;
            }
            it->setText(QString::fromUtf8(enhanceOpTitle(st.op).c_str()));
            it->setData(Qt::UserRole, enhanceStepToVariant(st));
        } else {
            SmoothStep st = smoothStepFromVariant(it->data(Qt::UserRole));
            switch (st.op) {
            case SmoothOp::Mean:
                st.maskW = m_w->value();
                st.maskH = m_h->value();
                break;
            case SmoothOp::Median:
            case SmoothOp::Gauss:
                st.maskW = m_w->value();
                break;
            case SmoothOp::Bilateral:
                st.sigmaRange = m_factor->value();
                st.sigmaSpace = static_cast<double>(m_h->value());
                break;
            default:
                break;
            }
            it->setText(QString::fromUtf8(smoothOpTitle(st.op).c_str()));
            it->setData(Qt::UserRole, smoothStepToVariant(st));
        }
    }
    void loadArgsFromItem(QListWidgetItem* it)
    {
        m_block = true;
        if (!it) {
            updateArgWidgets();
            m_block = false;
            return;
        }
        if (m_enhance) {
            const EnhanceStep st = enhanceStepFromVariant(it->data(Qt::UserRole));
            switch (st.op) {
            case EnhanceOp::Emphasize:
            case EnhanceOp::Illuminate:
                m_w->setValue(st.maskW);
                m_h->setValue(st.maskH);
                m_factor->setValue(st.coeff);
                break;
            case EnhanceOp::Scale:
                m_w->setValue(st.maskW);
                m_h->setValue(st.maskH);
                m_factor->setValue(st.coeff);
                break;
            default:
                break;
            }
        } else {
            const SmoothStep st = smoothStepFromVariant(it->data(Qt::UserRole));
            switch (st.op) {
            case SmoothOp::Mean:
                m_w->setValue(st.maskW);
                m_h->setValue(st.maskH);
                break;
            case SmoothOp::Median:
            case SmoothOp::Gauss:
                m_w->setValue(st.maskW);
                m_h->setValue(st.maskH);
                break;
            case SmoothOp::Bilateral:
                m_w->setValue(st.maskW);
                m_h->setValue(static_cast<int>(st.sigmaSpace));
                m_factor->setValue(st.sigmaRange);
                break;
            default:
                break;
            }
        }
        updateArgWidgets();
        m_block = false;
    }
    void updateArgWidgets()
    {
        bool showW = false;
        bool showH = false;
        bool showF = false;
        if (m_enhance) {
            const EnhanceStep st = enhanceItemStep(m_list->currentRow());
            switch (st.op) {
            case EnhanceOp::Emphasize:
            case EnhanceOp::Illuminate:
                showW = true;
                showH = true;
                showF = true;
                break;
            case EnhanceOp::Scale:
                showF = true;
                break;
            default:
                break;
            }
        } else {
            const SmoothStep st = smoothItemStep(m_list->currentRow());
            switch (st.op) {
            case SmoothOp::Mean:
                showW = true;
                showH = true;
                break;
            case SmoothOp::Median:
            case SmoothOp::Gauss:
                showW = true;
                break;
            case SmoothOp::Bilateral:
                showH = true;
                showF = true;
                break;
            default:
                break;
            }
        }
        m_w->setVisible(showW);
        m_h->setVisible(showH);
        m_factor->setVisible(showF);
    }
    void writeCurrentArgs()
    {
        writeArgsToItem(m_list->currentItem());
    }
    void onItemChanged(QListWidgetItem* cur, QListWidgetItem* prev)
    {
        if (m_block) {
            return;
        }
        writeArgsToItem(prev);
        loadArgsFromItem(cur);
    }
    void onAdd()
    {
        if (m_enhance) {
            EnhanceStep st;
            st.op = enhanceOpFromInt(m_op->currentData().toInt());
            if (st.op == EnhanceOp::Illuminate) {
                st.maskW = 101;
                st.maskH = 101;
                st.coeff = 0.7;
            }
            addEnhanceItem(st);
        } else {
            SmoothStep st;
            st.op = smoothOpFromInt(m_op->currentData().toInt());
            addSmoothItem(st);
        }
        m_list->setCurrentRow(m_list->count() - 1);
        onEdited();
    }
    void onDel()
    {
        const int row = m_list->currentRow();
        if (row >= 0) {
            QListWidgetItem* item = m_list->takeItem(row);
            if (item) {
                delete item;
                item = nullptr;
            }
            onEdited();
        }
    }
    void onUp()
    {
        const int row = m_list->currentRow();
        if (row <= 0) {
            return;
        }
        QListWidgetItem* it = m_list->takeItem(row);
        m_list->insertItem(row - 1, it);
        m_list->setCurrentRow(row - 1);
        onEdited();
    }
    void onDown()
    {
        const int row = m_list->currentRow();
        if (row < 0 || row >= m_list->count() - 1) {
            return;
        }
        QListWidgetItem* it = m_list->takeItem(row);
        m_list->insertItem(row + 1, it);
        m_list->setCurrentRow(row + 1);
        onEdited();
    }
    bool m_enhance;
    QListWidget* m_list;
    QComboBox* m_op;
    QSpinBox* m_w;
    QSpinBox* m_h;
    QDoubleSpinBox* m_factor;
};

// 增强预处理参数页
ToolEditorBase* createEnhanceEditor(QWidget* parent)
{
    return new PipelineEditor(parent, true);
}
// 平滑预处理参数页
ToolEditorBase* createSmoothEditor(QWidget* parent)
{
    return new PipelineEditor(parent, false);
}

} // namespace Vision
