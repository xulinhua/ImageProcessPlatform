/**
 * @file StepEditors.cpp
 * @brief 滤波和斑点的多步骤参数页，每步可选绑定区域
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditors.h"
#include "QtConv.h"
#include "UiText.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cstdlib>
#include <map>
#include <string>

namespace Vision {

namespace {

// 第 index 步的参数键，形如 step_0_op
std::string stepKey(int index, const char* suffix)
{
    return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
}

// 一步的参数，按值类型分表，键不含 step_N_ 前缀
struct StepRow {
    std::map<std::string, std::string> text;    ///< 文本参数
    std::map<std::string, int> ints;            ///< 整数参数
    std::map<std::string, double> nums;         ///< 浮点参数
    std::map<std::string, bool> flags;          ///< 开关参数
};

// 分步参数页：preprocess.filter 编辑滤波步骤，其余工具编辑斑点步骤并带 OK 判定
class StepEditor : public ToolEditorBase {
public:
    // 按工具名决定建哪一套表单；两套共用步骤列表和绑定区域
    StepEditor(const std::string& toolName, QWidget* parent)
        : ToolEditorBase(parent)
        , m_filter(toolName == "preprocess.filter")
        , m_row(-1)
        , m_list(0)
        , m_op(0)
        , m_type(0)
        , m_mode(0)
        , m_morph(0)
        , m_feat(0)
        , m_shape(0)
        , m_w(0)
        , m_h(0)
        , m_kernel(0)
        , m_roi(0)
        , m_absMin(0)
        , m_absMax(0)
        , m_c0(0)
        , m_c1(0)
        , m_rel(0)
        , m_dyn(0)
        , m_radius(0)
        , m_min(0)
        , m_max(0)
        , m_scale(0)
        , m_dark(0)
        , m_okMinCount(0)
        , m_okMaxCount(0)
        , m_okMinArea(0)
        , m_okMaxArea(0)
    {
        QVBoxLayout* lay = new QVBoxLayout(this);
        m_list = new QListWidget(this);
        lay->addWidget(m_list, 1);
        QHBoxLayout* buttons = new QHBoxLayout();
        QPushButton* add = new QPushButton(T("添加步骤"), this);
        QPushButton* remove = new QPushButton(T("删除步骤"), this);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        lay->addLayout(buttons);

        QFormLayout* form = new QFormLayout();
        if (m_filter) {
            m_op = addCombo(form, "算子", filterOps(), 15);
            m_w = addSpin(form, "掩膜宽", 1, 101);
            m_h = addSpin(form, "掩膜高", 1, 101);
            m_c0 = addDSpin(form, "参数1");
            m_c1 = addDSpin(form, "参数2");
            m_scale = new QCheckBox(T("拉伸到最大"), this);
            form->addRow(QString(), m_scale);
            connect(m_scale, SIGNAL(stateChanged(int)), this, SLOT(onEdited()));
            m_shape = addCombo(form, "掩膜形状", shapeOps(), 3);
        } else {
            m_type = addCombo(form, "步骤类型", blobTypes(), 3);
            m_mode = addCombo(form, "阈值方式", threshModes(), 3);
            m_absMin = addSpin(form, "绝对下限", 0, 255);
            m_absMax = addSpin(form, "绝对上限", 0, 255);
            m_rel = addDSpin(form, "相对偏移");
            m_dyn = addDSpin(form, "动态偏移");
            m_kernel = addSpin(form, "动态核", 3, 255);
            m_dark = new QCheckBox(T("暗目标"), this);
            form->addRow(QString(), m_dark);
            connect(m_dark, SIGNAL(stateChanged(int)), this, SLOT(onEdited()));
            m_morph = addCombo(form, "形态学", morphOps(), 4);
            m_radius = addDSpin(form, "半径");
            m_feat = addCombo(form, "筛选特征", featOps(), 2);
            m_min = addDSpin(form, "特征下限");
            m_max = addDSpin(form, "特征上限");
        }
        m_roi = addSpin(form, "绑定区域", -1, 49);
        lay->addLayout(form);
        if (!m_filter) {
            QFormLayout* judge = new QFormLayout();
            m_okMinCount = addSpin(judge, "数量下限", 0, 1000000000);
            m_okMaxCount = addSpin(judge, "数量上限", 0, 1000000000);
            m_okMinArea = addDSpin(judge, "面积下限");
            m_okMaxArea = addDSpin(judge, "面积上限");
            lay->addLayout(judge);
        }

        // 切行先存表单到旧行再载新行；删除时至少保留一步
        connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
            if (m_block || row < 0 || row == m_row) {
                return;
            }
            pullForm();
            m_row = row;
            m_block = true;
            pushForm();
            m_block = false;
        });
        connect(add, &QPushButton::clicked, this, [this]() {
            pullForm();
            m_steps.push_back(defaultStep());
            refillList(static_cast<int>(m_steps.size()) - 1);
            onEdited();
        });
        connect(remove, &QPushButton::clicked, this, [this]() {
            pullForm();
            const int row = m_list->currentRow();
            if (row < 0 || m_steps.size() <= 1) {
                return;
            }
            m_steps.erase(m_steps.begin() + row);
            const int next = row >= static_cast<int>(m_steps.size()) ? static_cast<int>(m_steps.size()) - 1 : row;
            refillList(next);
            onEdited();
        });
    }

protected:
    // 按 stepCount 读步骤，个数限制在 1~16
    void loadFromTool()
    {
        m_steps.clear();
        const int n = m_tool ? m_tool->params.value("stepCount").toInt(1) : 1;
        const int count = n < 1 ? 1 : (n > 16 ? 16 : n);
        for (int i = 0; i < count; ++i) {
            m_steps.push_back(readStep(i));
        }
        if (m_steps.empty()) {
            m_steps.push_back(defaultStep());
        }
        if (!m_filter && m_tool) {
            setSpin(m_okMinCount, m_tool->params.value("okMinCount").toInt(0));
            setSpin(m_okMaxCount, m_tool->params.value("okMaxCount").toInt(0));
            setDSpin(m_okMinArea, m_tool->params.value("okMinArea").toDouble(0.0));
            setDSpin(m_okMaxArea, m_tool->params.value("okMaxArea").toDouble(0.0));
        }
        refillList(0);
    }

    // 写全部步骤，并删掉下标超出当前步数的旧 step_ 键
    void saveToTool()
    {
        if (!m_tool) {
            return;
        }
        pullForm();
        const int count = static_cast<int>(m_steps.size());
        m_tool->params.insert("stepCount", count);
        for (int i = 0; i < count; ++i) {
            writeStep(i, m_steps[static_cast<size_t>(i)]);
        }
        std::vector<std::string> drop;
        const std::vector<std::string> keys = m_tool->params.keys();
        for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i].compare(0, 5, "step_") != 0) {
                continue;
            }
            const int index = stepIndexOf(keys[i]);
            if (index >= count) {
                drop.push_back(keys[i]);
            }
        }
        for (size_t i = 0; i < drop.size(); ++i) {
            m_tool->params.raw().erase(drop[i]);
        }
        if (!m_filter) {
            if (m_okMinCount) m_tool->params.insert("okMinCount", m_okMinCount->value());
            if (m_okMaxCount) m_tool->params.insert("okMaxCount", m_okMaxCount->value());
            if (m_okMinArea) m_tool->params.insert("okMinArea", m_okMinArea->value());
            if (m_okMaxArea) m_tool->params.insert("okMaxArea", m_okMaxArea->value());
        }
    }

private:
    // 各下拉的候选值，显示文本与存盘值相同；调用处传入的个数须与数组长度一致
    static const char** filterOps()
    {
        static const char* v[] = {
            "EquHisto", "Emphasize", "Illuminate", "Scale", "Invert",
            "Mean", "Median", "Gauss", "Bilateral",
            "GrayOpen", "GrayClose", "GrayErode", "GrayDilate", "TopHat", "BottomHat"
        };
        return v;
    }
    static const char** shapeOps()
    {
        static const char* v[] = { "rectangle", "octagon", "rhombus" };
        return v;
    }
    static const char** blobTypes()
    {
        static const char* v[] = { "threshold", "morph", "select" };
        return v;
    }
    static const char** threshModes()
    {
        static const char* v[] = { "absolute", "relative", "dynamic" };
        return v;
    }
    static const char** morphOps()
    {
        static const char* v[] = { "open", "close", "erode", "dilate" };
        return v;
    }
    static const char** featOps()
    {
        static const char* v[] = { "area", "circularity" };
        return v;
    }

    // 建表单控件并接到 onEdited
    QComboBox* addCombo(QFormLayout* form, const char* title, const char** items, int count)
    {
        QComboBox* box = new QComboBox(this);
        for (int i = 0; i < count; ++i) {
            box->addItem(QString::fromLatin1(items[i]), QString::fromLatin1(items[i]));
        }
        form->addRow(T(title), box);
        connect(box, SIGNAL(currentIndexChanged(int)), this, SLOT(onEdited()));
        return box;
    }

    QSpinBox* addSpin(QFormLayout* form, const char* title, int lo, int hi)
    {
        QSpinBox* box = new QSpinBox(this);
        box->setRange(lo, hi);
        form->addRow(T(title), box);
        connect(box, SIGNAL(valueChanged(int)), this, SLOT(onEdited()));
        return box;
    }

    QDoubleSpinBox* addDSpin(QFormLayout* form, const char* title)
    {
        QDoubleSpinBox* box = new QDoubleSpinBox(this);
        box->setRange(-1e9, 1e9);
        box->setDecimals(3);
        form->addRow(T(title), box);
        connect(box, SIGNAL(valueChanged(double)), this, SLOT(onEdited()));
        return box;
    }

    // 新步骤的默认值；roiIndex 为 -1 表示不绑定区域
    StepRow defaultStep() const
    {
        StepRow st;
        if (m_filter) {
            st.text["op"] = "EquHisto";
            st.ints["w"] = 7;
            st.ints["h"] = 7;
            st.nums["c0"] = 1.0;
            st.nums["c1"] = 3.0;
            st.flags["f"] = false;
            st.text["shape"] = "rectangle";
        } else {
            st.text["type"] = "threshold";
            st.text["threshMode"] = "absolute";
            st.ints["absMin"] = 80;
            st.ints["absMax"] = 255;
            st.nums["relOffset"] = 20.0;
            st.nums["dynOffset"] = 15.0;
            st.ints["dynKernel"] = 31;
            st.flags["dark"] = false;
            st.text["morphOp"] = "open";
            st.nums["radius"] = 1.0;
            st.text["feature"] = "area";
            st.nums["min"] = 50.0;
            st.nums["max"] = 0.0;
        }
        st.ints["roiIndex"] = -1;
        return st;
    }

    // 读第 index 步，缺的键保留默认值
    StepRow readStep(int index) const
    {
        StepRow st = defaultStep();
        if (!m_tool) {
            return st;
        }
        const ParamMap& m = m_tool->params;
        const std::string p = stepKey(index, "");
        readText(&st, m, p, "op");
        readText(&st, m, p, "shape");
        readText(&st, m, p, "type");
        readText(&st, m, p, "threshMode");
        readText(&st, m, p, "morphOp");
        readText(&st, m, p, "feature");
        readInt(&st, m, p, "w");
        readInt(&st, m, p, "h");
        readInt(&st, m, p, "absMin");
        readInt(&st, m, p, "absMax");
        readInt(&st, m, p, "dynKernel");
        readInt(&st, m, p, "roiIndex");
        readNum(&st, m, p, "c0");
        readNum(&st, m, p, "c1");
        readNum(&st, m, p, "relOffset");
        readNum(&st, m, p, "dynOffset");
        readNum(&st, m, p, "radius");
        readNum(&st, m, p, "min");
        readNum(&st, m, p, "max");
        if (m.contains(p + "f")) {
            st.flags["f"] = m.value(p + "f").toBool(false);
        }
        if (m.contains(p + "dark")) {
            st.flags["dark"] = m.value(p + "dark").toBool(false);
        }
        return st;
    }

    static void readText(StepRow* st, const ParamMap& m, const std::string& p, const char* key)
    {
        if (m.contains(p + key)) {
            st->text[key] = m.value(p + key).toString();
        }
    }
    static void readInt(StepRow* st, const ParamMap& m, const std::string& p, const char* key)
    {
        if (m.contains(p + key)) {
            st->ints[key] = m.value(p + key).toInt(st->ints[key]);
        }
    }
    static void readNum(StepRow* st, const ParamMap& m, const std::string& p, const char* key)
    {
        if (m.contains(p + key)) {
            st->nums[key] = m.value(p + key).toDouble(st->nums[key]);
        }
    }

    // 把一步的全部键写成 step_N_键
    void writeStep(int index, const StepRow& st)
    {
        const std::string p = stepKey(index, "");
        for (std::map<std::string, std::string>::const_iterator it = st.text.begin(); it != st.text.end(); ++it) {
            m_tool->params.insert(p + it->first, it->second);
        }
        for (std::map<std::string, int>::const_iterator it = st.ints.begin(); it != st.ints.end(); ++it) {
            m_tool->params.insert(p + it->first, it->second);
        }
        for (std::map<std::string, double>::const_iterator it = st.nums.begin(); it != st.nums.end(); ++it) {
            m_tool->params.insert(p + it->first, it->second);
        }
        for (std::map<std::string, bool>::const_iterator it = st.flags.begin(); it != st.flags.end(); ++it) {
            m_tool->params.insert(p + it->first, it->second);
        }
    }

    // 从 step_N_xxx 取出 N，格式不对返回 -1
    static int stepIndexOf(const std::string& key)
    {
        if (key.size() < 6 || key.compare(0, 5, "step_") != 0) {
            return -1;
        }
        const size_t cut = key.find('_', 5);
        if (cut == std::string::npos) {
            return -1;
        }
        return atoi(key.substr(5, cut - 5).c_str());
    }

    // 重建列表并选中 select 行（越界时夹到首尾），表单跟着刷新
    void refillList(int select)
    {
        const bool prev = m_block;
        m_block = true;
        m_list->clear();
        for (int i = 0; i < static_cast<int>(m_steps.size()); ++i) {
            m_list->addItem(T("步骤") + QString::number(i + 1) + QLatin1String("  ") + qs(stepTitle(m_steps[static_cast<size_t>(i)])));
        }
        if (select < 0) {
            select = 0;
        }
        if (select >= m_list->count()) {
            select = m_list->count() - 1;
        }
        m_row = select;
        m_list->setCurrentRow(select);
        pushForm();
        m_block = prev;
    }

    // 列表显示名：滤波用算子名，斑点用步骤类型
    std::string stepTitle(const StepRow& st) const
    {
        if (m_filter) {
            return st.text.count("op") ? st.text.find("op")->second : std::string();
        }
        return st.text.count("type") ? st.text.find("type")->second : std::string();
    }

    // 表单写回当前行；本套表单没有的控件为空，跳过
    void pullForm()
    {
        if (m_row < 0 || m_row >= static_cast<int>(m_steps.size())) {
            return;
        }
        StepRow& st = m_steps[static_cast<size_t>(m_row)];
        takeCombo(m_op, &st, "op");
        takeCombo(m_shape, &st, "shape");
        takeCombo(m_type, &st, "type");
        takeCombo(m_mode, &st, "threshMode");
        takeCombo(m_morph, &st, "morphOp");
        takeCombo(m_feat, &st, "feature");
        takeSpin(m_w, &st, "w");
        takeSpin(m_h, &st, "h");
        takeSpin(m_absMin, &st, "absMin");
        takeSpin(m_absMax, &st, "absMax");
        takeSpin(m_kernel, &st, "dynKernel");
        takeSpin(m_roi, &st, "roiIndex");
        takeDSpin(m_c0, &st, "c0");
        takeDSpin(m_c1, &st, "c1");
        takeDSpin(m_rel, &st, "relOffset");
        takeDSpin(m_dyn, &st, "dynOffset");
        takeDSpin(m_radius, &st, "radius");
        takeDSpin(m_min, &st, "min");
        takeDSpin(m_max, &st, "max");
        if (m_scale) st.flags["f"] = m_scale->isChecked();
        if (m_dark) st.flags["dark"] = m_dark->isChecked();
    }

    // 当前行填进表单
    void pushForm()
    {
        if (m_row < 0 || m_row >= static_cast<int>(m_steps.size())) {
            return;
        }
        const StepRow& st = m_steps[static_cast<size_t>(m_row)];
        putCombo(m_op, st, "op");
        putCombo(m_shape, st, "shape");
        putCombo(m_type, st, "type");
        putCombo(m_mode, st, "threshMode");
        putCombo(m_morph, st, "morphOp");
        putCombo(m_feat, st, "feature");
        putSpin(m_w, st, "w");
        putSpin(m_h, st, "h");
        putSpin(m_absMin, st, "absMin");
        putSpin(m_absMax, st, "absMax");
        putSpin(m_kernel, st, "dynKernel");
        putSpin(m_roi, st, "roiIndex");
        putDSpin(m_c0, st, "c0");
        putDSpin(m_c1, st, "c1");
        putDSpin(m_rel, st, "relOffset");
        putDSpin(m_dyn, st, "dynOffset");
        putDSpin(m_radius, st, "radius");
        putDSpin(m_min, st, "min");
        putDSpin(m_max, st, "max");
        if (m_scale) m_scale->setChecked(st.flags.count("f") ? st.flags.find("f")->second : false);
        if (m_dark) m_dark->setChecked(st.flags.count("dark") ? st.flags.find("dark")->second : false);
    }

    // 控件与 StepRow 互取，控件为空时不动
    static void takeCombo(QComboBox* box, StepRow* st, const char* key)
    {
        if (box) st->text[key] = ss(box->currentData().toString());
    }
    static void takeSpin(QSpinBox* box, StepRow* st, const char* key)
    {
        if (box) st->ints[key] = box->value();
    }
    static void takeDSpin(QDoubleSpinBox* box, StepRow* st, const char* key)
    {
        if (box) st->nums[key] = box->value();
    }
    static void putCombo(QComboBox* box, const StepRow& st, const char* key)
    {
        if (!box) return;
        const std::string v = st.text.count(key) ? st.text.find(key)->second : std::string();
        const int at = box->findData(qs(v));
        box->setCurrentIndex(at < 0 ? 0 : at);
    }
    static void putSpin(QSpinBox* box, const StepRow& st, const char* key)
    {
        if (box) box->setValue(st.ints.count(key) ? st.ints.find(key)->second : 0);
    }
    static void putDSpin(QDoubleSpinBox* box, const StepRow& st, const char* key)
    {
        if (box) box->setValue(st.nums.count(key) ? st.nums.find(key)->second : 0.0);
    }
    static void setSpin(QSpinBox* box, int v)
    {
        if (box) box->setValue(v);
    }
    static void setDSpin(QDoubleSpinBox* box, double v)
    {
        if (box) box->setValue(v);
    }

    bool m_filter;                  ///< true 为滤波步骤，false 为斑点步骤
    int m_row;                      ///< 表单当前对应的行，-1 为无
    std::vector<StepRow> m_steps;   ///< 编辑中的步骤，保存时整体写回
    QListWidget* m_list;
    QComboBox* m_op;
    QComboBox* m_type;
    QComboBox* m_mode;
    QComboBox* m_morph;
    QComboBox* m_feat;
    QComboBox* m_shape;
    QSpinBox* m_w;
    QSpinBox* m_h;
    QSpinBox* m_kernel;
    QSpinBox* m_roi;
    QSpinBox* m_absMin;
    QSpinBox* m_absMax;
    QDoubleSpinBox* m_c0;
    QDoubleSpinBox* m_c1;
    QDoubleSpinBox* m_rel;
    QDoubleSpinBox* m_dyn;
    QDoubleSpinBox* m_radius;
    QDoubleSpinBox* m_min;
    QDoubleSpinBox* m_max;
    QCheckBox* m_scale;
    QCheckBox* m_dark;
    QSpinBox* m_okMinCount;
    QSpinBox* m_okMaxCount;
    QDoubleSpinBox* m_okMinArea;
    QDoubleSpinBox* m_okMaxArea;
};

}

// 分步滤波或分步斑点参数页
ToolEditorBase* createStepEditor(const std::string& toolName, QWidget* parent)
{
    return new StepEditor(toolName, parent);
}

ToolEditorBase* createFilterStepEditor(QWidget* parent)
{
    return createStepEditor("preprocess.filter", parent);
}

ToolEditorBase* createBlobStepEditor(QWidget* parent)
{
    return createStepEditor("detect.blob", parent);
}

}
