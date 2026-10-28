/**
 * @file LocateDetectEditors.cpp
 * @brief LocateDetectEditors实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditors.h"
#include "ParamKeys.h"
#include "SolutionDataPaths.h"
#include "ToolParamUi.h"
#include "QtConv.h"
#include "UiText.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Vision {

// 按存盘值选中下拉项，找不到则第一项
static void setComboData(QComboBox* c, const QString& v)
{
    const int i = c->findData(v);
    c->setCurrentIndex(i >= 0 ? i : 0);
}

class TmplEditor : public ToolEditorBase {
public:
    explicit TmplEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_method = mkParamCombo(this);
        m_method->addItem(QLatin1String("NCC"), QLatin1String("ncc"));
        m_method->addItem(T("形状"), QLatin1String("shape"));
        m_score = mkParamDSpin(this, 0.1, 1.0, 2, 0.05);
        m_angle = mkParamDSpin(this, 0, 180, 1, 1.0);
        m_num = mkParamSpin(this, 1, 20);
        m_modelFile = new QLineEdit(this);
        m_rebuild = mkParamCheck(this, T("下次运行强制重建并覆盖模型"));
        QPushButton* teach = new QPushButton(T("记录当前位姿为示教"), this);
        QPushButton* delModel = new QPushButton(T("删除已存模型文件"), this);
        connect(teach, &QPushButton::clicked, this, &TmplEditor::onTeach);
        connect(delModel, &QPushButton::clicked, this, &TmplEditor::onDeleteModel);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamCell(this, T("匹配方式"), m_method));
        addParamStackItem(lay, mkParamRow2(this, T("最低分"), m_score, T("角度范围(度)"), m_angle));
        addParamStackItem(lay, mkParamCell(this, T("匹配个数"), m_num));
        addParamStackItem(lay, mkParamCell(this, T("模型文件(相对vpdata)"), m_modelFile));
        addParamStackItem(lay, m_rebuild);
        addParamStackItem(lay, teach);
        addParamStackItem(lay, delModel);
        QLabel* tip = new QLabel(
            T("输出：x / y / angle / score。请先保存方案；首次运行用 ROI 创建模型并写入 .vpdata。"), this);
        tip->setWordWrap(true);
        tip->setStyleSheet(QStringLiteral("color:#666;"));
        addParamStackItem(lay, tip);
        connect(m_method, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &TmplEditor::onEdited);
        connect(m_method, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &TmplEditor::onMethodChanged);
        connect(m_score, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this, &TmplEditor::onEdited);
        connect(m_angle, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this, &TmplEditor::onEdited);
        connect(m_num, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this, &TmplEditor::onEdited);
        connect(m_modelFile, &QLineEdit::editingFinished, this, &TmplEditor::onEdited);
        connect(m_rebuild, &QCheckBox::toggled, this, &TmplEditor::onEdited);
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        setComboData(m_method, qs(m_tool->params.value("method").toString()));
        m_score->setValue(m_tool->params.value("minScore").toDouble(0.5));
        m_angle->setValue(m_tool->params.value("angleRange").toDouble(15));
        m_num->setValue(m_tool->params.value("numMatches").toInt(1));
        QString mf = qs(m_tool->params.value("modelFile").toString());
        if (mf.isEmpty()) {
            mf = (m_method->currentData().toString() == QLatin1String("shape"))
                ? QLatin1String("model.shm") : QLatin1String("model.ncm");
        }
        m_modelFile->setText(mf);
        m_rebuild->setChecked(m_tool->params.value("rebuildModel").toBool(false));
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("method", ss(m_method->currentData().toString()));
        m_tool->params.insert("minScore", m_score->value());
        m_tool->params.insert("angleRange", m_angle->value());
        m_tool->params.insert("numMatches", m_num->value());
        QString mf = m_modelFile->text().trimmed();
        if (mf.isEmpty()) {
            mf = (m_method->currentData().toString() == QLatin1String("shape"))
                ? QLatin1String("model.shm") : QLatin1String("model.ncm");
        }
        m_tool->params.insert("modelFile", ss(mf));
        m_tool->params.insert("rebuildModel", m_rebuild->isChecked());
    }

private:
    void onMethodChanged()
    {
        if (m_block) {
            return;
        }
        const QString method = m_method->currentData().toString();
        const QString cur = m_modelFile->text().trimmed();
        if (cur.isEmpty() || cur == QLatin1String("model.ncm") || cur == QLatin1String("model.shm")) {
            m_modelFile->setText(method == QLatin1String("shape")
                                     ? QLatin1String("model.shm")
                                     : QLatin1String("model.ncm"));
        }
        onEdited();
    }
    void onTeach()
    {
        if (!m_tool) {
            return;
        }
        teachSet(&m_tool->params, TeachKey::RefX, runtimeGet(m_tool->runtime, RuntimeKey::LastX).toDouble());
        teachSet(&m_tool->params, TeachKey::RefY, runtimeGet(m_tool->runtime, RuntimeKey::LastY).toDouble());
        teachSet(&m_tool->params, TeachKey::RefA, runtimeGet(m_tool->runtime, RuntimeKey::LastA).toDouble());
        onEdited();
    }
    void onDeleteModel()
    {
        if (!m_tool || !m_sol || m_sol->filePath.empty()) {
            return;
        }
        StationCfg* st = nullptr;
        JobCfg* job = m_sol->findJobCfgOfTool(m_tool->id, &st);
        if (!job) {
            return;
        }
        const std::string dir = toolDataDir(m_sol->filePath, st ? st->id : ObjectId(), job->id, m_tool->id);
        std::string rel = ss(m_modelFile->text().trimmed());
        if (rel.empty()) {
            rel = (m_method->currentData().toString() == QLatin1String("shape"))
                ? "model.shm" : "model.ncm";
        }
        const std::string abs = resolveToolAssetPath(dir, rel);
        removeFileIfExists(abs);
        m_rebuild->setChecked(true);
        onEdited();
    }
    QComboBox* m_method;
    QDoubleSpinBox* m_score;
    QDoubleSpinBox* m_angle;
    QSpinBox* m_num;
    QLineEdit* m_modelFile;
    QCheckBox* m_rebuild;
};

class FindLineEditor : public ToolEditorBase {
public:
    explicit FindLineEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_pol = mkParamCombo(this);
        m_pol->addItem(T("亮 -> 暗"), QLatin1String("positive"));
        m_pol->addItem(T("暗 -> 亮"), QLatin1String("negative"));
        m_pol->addItem(T("全部"), QLatin1String("all"));
        m_sel = mkParamCombo(this);
        m_sel->addItem(T("最强"), QLatin1String("best"));
        m_sel->addItem(T("第一条"), QLatin1String("first"));
        m_sel->addItem(T("最后一条"), QLatin1String("last"));
        m_count = mkParamSpin(this, 1, 20);
        m_minLen = mkParamDSpin(this, 1, 10000, 1, 1.0);
        m_minLen->setSuffix(QLatin1String(" px"));
        m_sigma = mkParamDSpin(this, 0.4, 20, 2, 0.1);
        m_sigma->setSuffix(QLatin1String(" px"));
        m_low = mkParamSpin(this, 1, 255);
        m_high = mkParamSpin(this, 1, 255);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamRow2(this, T("极性"), m_pol, T("选择"), m_sel));
        addParamStackItem(lay, mkParamRow2(this, T("最小长度"), m_minLen, T("Sigma"), m_sigma));
        addParamStackItem(lay, mkParamCell(this, T("每框条数"), m_count));
        addParamStackItem(lay, mkParamRow2(this, T("低阈值"), m_low, T("高阈值"), m_high));
        connect(m_pol, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_sel, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_count, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_minLen, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_sigma, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_low, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_high, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        setComboData(m_pol, qs(m_tool->params.value("polarity").toString()));
        setComboData(m_sel, qs(m_tool->params.value("select").toString()));
        m_count->setValue(m_tool->params.value("searchCount").toInt(1));
        m_minLen->setValue(m_tool->params.value("minLength").toDouble(20));
        m_sigma->setValue(m_tool->params.value("sigma").toDouble(1));
        m_low->setValue(m_tool->params.value("low").toInt(20));
        m_high->setValue(m_tool->params.value("high").toInt(40));
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("polarity", ss(m_pol->currentData().toString()));
        m_tool->params.insert("select", ss(m_sel->currentData().toString()));
        m_tool->params.insert("searchCount", m_count->value());
        m_tool->params.insert("minLength", m_minLen->value());
        m_tool->params.insert("sigma", m_sigma->value());
        m_tool->params.insert("low", m_low->value());
        m_tool->params.insert("high", m_high->value());
    }
    QComboBox* m_pol;
    QComboBox* m_sel;
    QSpinBox* m_count;
    QDoubleSpinBox* m_minLen;
    QDoubleSpinBox* m_sigma;
    QSpinBox* m_low;
    QSpinBox* m_high;
};

class FindCircleEditor : public ToolEditorBase {
public:
    explicit FindCircleEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_th = mkParamSpin(this, 0, 255);
        m_minR = mkParamSpin(this, 1, 5000);
        m_maxR = mkParamSpin(this, 1, 5000);
        m_dark = mkParamCheck(this, QString());
        m_sigma = mkParamDSpin(this, 0.4, 20, 2, 0.1);
        m_low = mkParamSpin(this, 1, 255);
        m_high = mkParamSpin(this, 1, 255);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamRow2(this, T("阈值"), m_th, T("最小半径"), m_minR));
        addParamStackItem(lay, mkParamRow2(this, T("最大半径"), m_maxR, T("Sigma"), m_sigma));
        addParamStackItem(lay, mkParamRow2(this, T("低阈值"), m_low, T("高阈值"), m_high));
        addParamStackItem(lay, mkParamCell(this, T("暗目标"), m_dark));
        connect(m_th, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_minR, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_maxR, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_dark, SIGNAL(toggled(bool)), this, SLOT(onEdited()));
        connect(m_sigma, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_low, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_high, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        m_th->setValue(m_tool->params.value("threshold").toInt(80));
        m_minR->setValue(m_tool->params.value("minR").toInt(8));
        m_maxR->setValue(m_tool->params.value("maxR").toInt(400));
        m_dark->setChecked(m_tool->params.value("dark").toBool(true));
        m_sigma->setValue(m_tool->params.value("sigma").toDouble(1));
        m_low->setValue(m_tool->params.value("low").toInt(20));
        m_high->setValue(m_tool->params.value("high").toInt(40));
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("threshold", m_th->value());
        m_tool->params.insert("minR", m_minR->value());
        m_tool->params.insert("maxR", m_maxR->value());
        m_tool->params.insert("dark", m_dark->isChecked());
        m_tool->params.insert("sigma", m_sigma->value());
        m_tool->params.insert("low", m_low->value());
        m_tool->params.insert("high", m_high->value());
    }
    QSpinBox* m_th;
    QSpinBox* m_minR;
    QSpinBox* m_maxR;
    QCheckBox* m_dark;
    QDoubleSpinBox* m_sigma;
    QSpinBox* m_low;
    QSpinBox* m_high;
};

class CaliperEditor : public ToolEditorBase {
public:
    explicit CaliperEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_mode = mkParamCombo(this);
        m_mode->addItem(T("测宽"), QLatin1String("width"));
        m_mode->addItem(T("找边"), QLatin1String("edge"));
        m_sigma = mkParamDSpin(this, 0.4, 20, 2, 0.1);
        m_th = mkParamDSpin(this, 1, 255, 1, 1.0);
        m_trans = mkParamCombo(this);
        m_trans->addItem(T("全部"), QLatin1String("all"));
        m_trans->addItem(T("由暗到亮"), QLatin1String("positive"));
        m_trans->addItem(T("由亮到暗"), QLatin1String("negative"));
        m_sel = mkParamCombo(this);
        m_sel->addItem(T("全部"), QLatin1String("all"));
        m_sel->addItem(T("第一条"), QLatin1String("first"));
        m_sel->addItem(T("最后一条"), QLatin1String("last"));
        m_count = mkParamSpin(this, 1, 20);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamCell(this, T("模式"), m_mode));
        addParamStackItem(lay, mkParamRow2(this, T("Sigma"), m_sigma, T("阈值"), m_th));
        addParamStackItem(lay, mkParamRow2(this, T("极性"), m_trans, T("选择"), m_sel));
        addParamStackItem(lay, mkParamCell(this, T("每框条数"), m_count));
        connect(m_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_sigma, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_th, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_trans, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_sel, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_count, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        setComboData(m_mode, qs(m_tool->params.value("mode").toString()));
        m_sigma->setValue(m_tool->params.value("sigma").toDouble(1));
        m_th->setValue(m_tool->params.value("threshold").toDouble(20));
        setComboData(m_trans, qs(m_tool->params.value("transition").toString()));
        setComboData(m_sel, qs(m_tool->params.value("select").toString()));
        m_count->setValue(m_tool->params.value("searchCount").toInt(1));
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("mode", ss(m_mode->currentData().toString()));
        m_tool->params.insert("sigma", m_sigma->value());
        m_tool->params.insert("threshold", m_th->value());
        m_tool->params.insert("transition", ss(m_trans->currentData().toString()));
        m_tool->params.insert("select", ss(m_sel->currentData().toString()));
        m_tool->params.insert("searchCount", m_count->value());
    }
    QComboBox* m_mode;
    QDoubleSpinBox* m_sigma;
    QDoubleSpinBox* m_th;
    QComboBox* m_trans;
    QComboBox* m_sel;
    QSpinBox* m_count;
};

class BlobEditor : public ToolEditorBase {
public:
    explicit BlobEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_mode = mkParamCombo(this);
        m_mode->addItem(T("绝对阈值"), QLatin1String("absolute"));
        m_mode->addItem(T("相对阈值"), QLatin1String("relative"));
        m_mode->addItem(T("动态阈值"), QLatin1String("dynamic"));
        m_absMin = mkParamSpin(this, 0, 255);
        m_absMax = mkParamSpin(this, 0, 255);
        m_rel = mkParamDSpin(this, 0, 255, 1, 1.0);
        m_dyn = mkParamDSpin(this, 0, 255, 1, 1.0);
        m_kern = mkParamSpin(this, 3, 201);
        m_dark = mkParamCheck(this, T("暗目标"));
        m_open = mkParamDSpin(this, 0, 50, 1, 0.5);
        m_close = mkParamDSpin(this, 0, 50, 1, 0.5);
        m_minA = mkParamSpin(this, 0, 100000000);
        m_maxA = mkParamSpin(this, 0, 100000000);
        m_minC = mkParamDSpin(this, 0, 1, 2, 0.01);
        m_maxC = mkParamDSpin(this, 0, 1, 2, 0.01);
        m_okMinCnt = mkParamSpin(this, 0, 100000);
        m_okMaxCnt = mkParamSpin(this, 0, 100000);
        m_okMinArea = mkParamDSpin(this, 0, 1e12, 1, 1.0);
        m_okMaxArea = mkParamDSpin(this, 0, 1e12, 1, 1.0);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamCell(this, T("阈值方式"), m_mode));
        addParamStackItem(lay, mkParamRow2(this, T("绝对下限"), m_absMin, T("绝对上限"), m_absMax));
        addParamStackItem(lay, mkParamRow2(this, T("相对偏移"), m_rel, T("动态偏移"), m_dyn));
        addParamStackItem(lay, mkParamCell(this, T("动态核"), m_kern));
        addParamStackItem(lay, m_dark);
        addParamStackItem(lay, mkParamRow2(this, T("开运算半径"), m_open, T("闭运算半径"), m_close));
        addParamStackItem(lay, mkParamRow2(this, T("筛选最小面积"), m_minA, T("筛选最大面积(0不限)"), m_maxA));
        addParamStackItem(lay, mkParamRow2(this, T("最小圆度"), m_minC, T("最大圆度"), m_maxC));
        addParamStackItem(lay, mkParamRow2(this, T("判定最少数量(0不判)"), m_okMinCnt, T("判定最多数量(0不限)"), m_okMaxCnt));
        addParamStackItem(lay, mkParamRow2(this, T("判定总面积下限(0不判)"), m_okMinArea, T("判定总面积上限(0不限)"), m_okMaxArea));
        connect(m_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_absMin, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_absMax, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_rel, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_dyn, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_kern, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_dark, SIGNAL(toggled(bool)), this, SLOT(onEdited()));
        connect(m_open, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_close, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_minA, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_maxA, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_minC, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_maxC, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_okMinCnt, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_okMaxCnt, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_okMinArea, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_okMaxArea, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        setComboData(m_mode, qs(m_tool->params.value("threshMode").toString()));
        m_absMin->setValue(m_tool->params.value("absMin").toInt(80));
        m_absMax->setValue(m_tool->params.value("absMax").toInt(255));
        m_rel->setValue(m_tool->params.value("relOffset").toDouble(20));
        m_dyn->setValue(m_tool->params.value("dynOffset").toDouble(15));
        m_kern->setValue(m_tool->params.value("dynKernel").toInt(31));
        m_dark->setChecked(m_tool->params.value("dark").toBool(false));
        m_open->setValue(m_tool->params.value("openR").toDouble(1));
        m_close->setValue(m_tool->params.value("closeR").toDouble(0));
        m_minA->setValue(m_tool->params.value("minArea").toInt(50));
        m_maxA->setValue(m_tool->params.value("maxArea").toInt(0));
        m_minC->setValue(m_tool->params.value("minCirc").toDouble(0));
        m_maxC->setValue(m_tool->params.value("maxCirc").toDouble(1));
        m_okMinCnt->setValue(m_tool->params.value("okMinCount").toInt(0));
        m_okMaxCnt->setValue(m_tool->params.value("okMaxCount").toInt(0));
        m_okMinArea->setValue(m_tool->params.value("okMinArea").toDouble(0));
        m_okMaxArea->setValue(m_tool->params.value("okMaxArea").toDouble(0));
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("threshMode", ss(m_mode->currentData().toString()));
        m_tool->params.insert("absMin", m_absMin->value());
        m_tool->params.insert("absMax", m_absMax->value());
        m_tool->params.insert("relOffset", m_rel->value());
        m_tool->params.insert("dynOffset", m_dyn->value());
        m_tool->params.insert("dynKernel", m_kern->value());
        m_tool->params.insert("dark", m_dark->isChecked());
        m_tool->params.insert("openR", m_open->value());
        m_tool->params.insert("closeR", m_close->value());
        m_tool->params.insert("minArea", m_minA->value());
        m_tool->params.insert("maxArea", m_maxA->value());
        m_tool->params.insert("minCirc", m_minC->value());
        m_tool->params.insert("maxCirc", m_maxC->value());
        m_tool->params.insert("okMinCount", m_okMinCnt->value());
        m_tool->params.insert("okMaxCount", m_okMaxCnt->value());
        m_tool->params.insert("okMinArea", m_okMinArea->value());
        m_tool->params.insert("okMaxArea", m_okMaxArea->value());
    }
    QComboBox* m_mode;
    QSpinBox* m_absMin;
    QSpinBox* m_absMax;
    QDoubleSpinBox* m_rel;
    QDoubleSpinBox* m_dyn;
    QSpinBox* m_kern;
    QCheckBox* m_dark;
    QDoubleSpinBox* m_open;
    QDoubleSpinBox* m_close;
    QSpinBox* m_minA;
    QSpinBox* m_maxA;
    QDoubleSpinBox* m_minC;
    QDoubleSpinBox* m_maxC;
    QSpinBox* m_okMinCnt;
    QSpinBox* m_okMaxCnt;
    QDoubleSpinBox* m_okMinArea;
    QDoubleSpinBox* m_okMaxArea;
};

class TextureEditor : public ToolEditorBase {
public:
    explicit TextureEditor(QWidget* parent)
        : ToolEditorBase(parent)
    {
        m_bg = mkParamCombo(this);
        m_bg->addItem(T("均值背景"), QLatin1String("mean"));
        m_bg->addItem(T("中值背景"), QLatin1String("median"));
        m_bg->addItem(T("灰度闭运算"), QLatin1String("closing"));
        m_kw = mkParamSpin(this, 3, 501);
        m_kh = mkParamSpin(this, 3, 501);
        m_th = mkParamDSpin(this, 1, 255, 1, 1.0);
        m_open = mkParamDSpin(this, 0, 50, 1, 0.5);
        m_minA = mkParamSpin(this, 0, 100000000);
        m_maxA = mkParamSpin(this, 0, 100000000);
        QVBoxLayout* lay = mkParamStack(this, 10);
        addParamStackItem(lay, mkParamCell(this, T("背景"), m_bg));
        addParamStackItem(lay, mkParamRow2(this, T("核宽"), m_kw, T("核高"), m_kh));
        addParamStackItem(lay, mkParamRow2(this, T("残差阈值"), m_th, T("开运算半径"), m_open));
        addParamStackItem(lay, mkParamRow2(this, T("最小面积"), m_minA, T("最大面积(0不限)"), m_maxA));
        connect(m_bg, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        connect(m_kw, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_kh, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_th, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_open, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        connect(m_minA, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        connect(m_maxA, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
    }

protected:
    void loadFromTool()
    {
        if (!m_tool) {
            return;
        }
        setComboData(m_bg, qs(m_tool->params.value("bgMethod").toString()));
        m_kw->setValue(m_tool->params.value("kernelW").toInt(31));
        m_kh->setValue(m_tool->params.value("kernelH").toInt(31));
        m_th->setValue(m_tool->params.value("residThresh").toDouble(18));
        m_open->setValue(m_tool->params.value("openR").toDouble(1.5));
        m_minA->setValue(m_tool->params.value("minArea").toInt(30));
        m_maxA->setValue(m_tool->params.value("maxArea").toInt(0));
    }
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        m_tool->params.insert("bgMethod", ss(m_bg->currentData().toString()));
        m_tool->params.insert("kernelW", m_kw->value());
        m_tool->params.insert("kernelH", m_kh->value());
        m_tool->params.insert("residThresh", m_th->value());
        m_tool->params.insert("openR", m_open->value());
        m_tool->params.insert("minArea", m_minA->value());
        m_tool->params.insert("maxArea", m_maxA->value());
    }
    QComboBox* m_bg;
    QSpinBox* m_kw;
    QSpinBox* m_kh;
    QDoubleSpinBox* m_th;
    QDoubleSpinBox* m_open;
    QSpinBox* m_minA;
    QSpinBox* m_maxA;
};

// 模板定位参数页
ToolEditorBase* createTmplEditor(QWidget* parent) { return new TmplEditor(parent); }
// 找线参数页
ToolEditorBase* createFindLineEditor(QWidget* parent) { return new FindLineEditor(parent); }
// 找圆参数页
ToolEditorBase* createFindCircleEditor(QWidget* parent) { return new FindCircleEditor(parent); }
// 卡尺参数页
ToolEditorBase* createCaliperEditor(QWidget* parent) { return new CaliperEditor(parent); }
// 斑点检测参数页
ToolEditorBase* createBlobEditor(QWidget* parent) { return new BlobEditor(parent); }
// 纹理检测参数页
ToolEditorBase* createTextureEditor(QWidget* parent) { return new TextureEditor(parent); }

}
