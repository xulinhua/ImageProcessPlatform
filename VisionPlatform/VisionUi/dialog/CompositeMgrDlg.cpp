/**
 * @file CompositeMgrDlg.cpp
 * @brief CompositeMgrDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CompositeMgrDlg.h"
#include "CompositeMgr.h"
#include "QtConv.h"
#include "TextEncoding.h"
#include "ToolBase.h"
#include "AlgToolBase.h"
#include "AlgLibKind.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace Vision {

namespace {
QString scopeText(CompositeScope scope)
{
    return scope == CompositeLibrary ? T("独立") : T("项目");
}

// 列表上显示名称和当前算法库
QString toolRowText(const ToolCfg& tool)
{
    QString name = qs(tool.name);
    if (name.isEmpty()) {
        name = qs(tool.toolName);
    }
    const char* lib = algLibKindTitle(tool.algLib);
    if (lib && lib[0]) {
        name += QLatin1String("  [");
        name += QString::fromLatin1(lib);
        name += QLatin1Char(']');
    }
    return name;
}
}

CompositeMgrDlg::CompositeMgrDlg(Solution* sol, QWidget* parent)
    : QDialog(parent)
    , m_sol(sol)
    , m_table(nullptr)
{
    setWindowTitle(T("组合工具"));
    resize(720, 420);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList() << T("名称") << T("范围") << T("版本") << T("说明"));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QPushButton* addBtn = new QPushButton(T("新建"), this);
    QPushButton* editBtn = new QPushButton(T("修改"), this);
    QPushButton* delBtn = new QPushButton(T("删除"), this);
    QPushButton* closeBtn = new QPushButton(T("关闭"), this);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onNew()));
    connect(editBtn, SIGNAL(clicked()), this, SLOT(onEdit()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onDelete()));
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(addBtn);
    buttons->addWidget(editBtn);
    buttons->addWidget(delBtn);
    buttons->addStretch();
    buttons->addWidget(closeBtn);
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->addWidget(m_table, 1);
    lay->addLayout(buttons);
    reload();
}

void CompositeMgrDlg::reload()
{
    m_table->setRowCount(0);
    const std::vector<CompositeDef*> rows = CompositeMgr::instance().all();
    for (size_t i = 0; i < rows.size(); ++i) {
        const CompositeDef* d = rows[i];
        if (!d) {
            continue;
        }
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        QTableWidgetItem* name = new QTableWidgetItem(qs(d->name));
        name->setData(Qt::UserRole, qs(d->id.toString()));
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, new QTableWidgetItem(scopeText(d->scope)));
        m_table->setItem(row, 2, new QTableWidgetItem(qs(d->version)));
        m_table->setItem(row, 3, new QTableWidgetItem(qs(d->description)));
    }
    m_table->resizeColumnsToContents();
}

void CompositeMgrDlg::onNew()
{
    QDialog box(this);
    box.setWindowTitle(T("新建组合工具"));
    QLineEdit* name = new QLineEdit(&box);
    QRadioButton* project = new QRadioButton(T("项目组合工具"), &box);
    QRadioButton* library = new QRadioButton(T("独立组合工具"), &box);
    library->setChecked(true);
    project->setEnabled(m_sol != 0);
    QPushButton* ok = new QPushButton(T("确定"), &box);
    QPushButton* cancel = new QPushButton(T("取消"), &box);
    connect(ok, SIGNAL(clicked()), &box, SLOT(accept()));
    connect(cancel, SIGNAL(clicked()), &box, SLOT(reject()));
    QVBoxLayout* lay = new QVBoxLayout(&box);
    lay->addWidget(new QLabel(T("名称"), &box));
    lay->addWidget(name);
    lay->addWidget(project);
    lay->addWidget(library);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    lay->addLayout(buttons);
    if (box.exec() != QDialog::Accepted) {
        return;
    }
    std::string err;
    const CompositeScope scope = project->isChecked() ? CompositeSolution : CompositeLibrary;
    const ObjectId id = CompositeMgr::instance().create(scope, ss(name->text()), &err);
    if (id.isNull()) {
        QMessageBox::warning(this, T("组合工具"), qs(err));
        return;
    }
    if (scope == CompositeSolution && m_sol) {
        m_sol->dirty = true;
    }
    reload();
}

void CompositeMgrDlg::onEdit()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return;
    }
    CompositeEditDlg dlg(m_table->item(row, 0)->data(Qt::UserRole).toString(), this);
    if (dlg.exec() == QDialog::Accepted && m_sol) {
        const CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_table->item(row, 0)->data(Qt::UserRole).toString())));
        if (d && d->scope == CompositeSolution) {
            m_sol->dirty = true;
        }
    }
    reload();
}

void CompositeMgrDlg::onDelete()
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, 0)) {
        return;
    }
    if (QMessageBox::question(this, T("删除"), T("删除此组合工具？")) != QMessageBox::Yes) {
        return;
    }
    std::string err;
    if (!CompositeMgr::instance().remove(ObjectId::fromString(ss(m_table->item(row, 0)->data(Qt::UserRole).toString())), &err)) {
        QMessageBox::warning(this, T("组合工具"), err.empty() ? T("无法删除") : qs(err));
        return;
    }
    reload();
}

CompositeEditDlg::CompositeEditDlg(const QString& id, QWidget* parent)
    : QDialog(parent)
    , m_id(id)
    , m_shownId()
    , m_filling(false)
    , m_name(nullptr)
    , m_desc(nullptr)
    , m_tools(nullptr)
    , m_algLib(nullptr)
    , m_ports(nullptr)
    , m_paras(nullptr)
{
    setWindowTitle(T("修改组合工具"));
    resize(860, 520);
    m_name = new QLineEdit(this);
    m_desc = new QLineEdit(this);
    m_tools = new QListWidget(this);
    m_algLib = new QComboBox(this);
    m_ports = new QListWidget(this);
    m_paras = new QListWidget(this);
    QPushButton* addBtn = new QPushButton(T("添加工具"), this);
    QPushButton* delBtn = new QPushButton(T("删除工具"), this);
    QPushButton* upBtn = new QPushButton(T("上移"), this);
    QPushButton* downBtn = new QPushButton(T("下移"), this);
    QPushButton* saveBtn = new QPushButton(T("保存"), this);
    QPushButton* closeBtn = new QPushButton(T("关闭"), this);
    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAddTool()));
    connect(delBtn, SIGNAL(clicked()), this, SLOT(onDelTool()));
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSave()));
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(reject()));
    connect(m_tools, SIGNAL(currentRowChanged(int)), this, SLOT(onToolChanged()));
    connect(m_algLib, SIGNAL(currentIndexChanged(int)), this, SLOT(onAlgLibChanged(int)));
    connect(upBtn, SIGNAL(clicked()), this, SLOT(onMoveUp()));
    connect(downBtn, SIGNAL(clicked()), this, SLOT(onMoveDown()));
    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(T("名称"), this));
    top->addWidget(m_name, 1);
    top->addWidget(new QLabel(T("说明"), this));
    top->addWidget(m_desc, 1);
    QHBoxLayout* mid = new QHBoxLayout();
    QVBoxLayout* left = new QVBoxLayout();
    left->addWidget(new QLabel(T("内部工具"), this));
    left->addWidget(m_tools, 1);
    QHBoxLayout* libRow = new QHBoxLayout();
    libRow->addWidget(new QLabel(T("算法库"), this));
    libRow->addWidget(m_algLib, 1);
    left->addLayout(libRow);
    QHBoxLayout* leftBtns = new QHBoxLayout();
    leftBtns->addWidget(addBtn);
    leftBtns->addWidget(delBtn);
    leftBtns->addWidget(upBtn);
    leftBtns->addWidget(downBtn);
    left->addLayout(leftBtns);
    QVBoxLayout* midCol = new QVBoxLayout();
    midCol->addWidget(new QLabel(T("公开接口"), this));
    midCol->addWidget(m_ports, 1);
    QVBoxLayout* right = new QVBoxLayout();
    right->addWidget(new QLabel(T("公开参数"), this));
    right->addWidget(m_paras, 1);
    mid->addLayout(left, 1);
    mid->addLayout(midCol, 1);
    mid->addLayout(right, 1);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(saveBtn);
    buttons->addWidget(closeBtn);
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->addLayout(top);
    lay->addLayout(mid, 1);
    lay->addLayout(buttons);
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    if (!d) {
        return;
    }
    m_name->setText(qs(d->name));
    m_desc->setText(qs(d->description));
    for (size_t i = 0; i < d->inner.tools.size(); ++i) {
        QListWidgetItem* it = new QListWidgetItem(toolRowText(d->inner.tools[i]));
        it->setData(Qt::UserRole, qs(d->inner.tools[i].id.toString()));
        m_tools->addItem(it);
    }
    if (m_tools->count() > 0) {
        m_tools->setCurrentRow(0);
    }
}

void CompositeEditDlg::onToolChanged()
{
    applyAlgLib();
    QListWidgetItem* cur = m_tools->currentItem();
    const QString now = cur ? cur->data(Qt::UserRole).toString() : QString();
    if (!m_shownId.isEmpty() && m_shownId != now) {
        storePubs();
    }
    loadPubs();
}

// 把当前算法库写回选中的内部工具
void CompositeEditDlg::applyAlgLib()
{
    if (m_filling || !m_algLib) {
        return;
    }
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    if (!d || m_shownId.isEmpty()) {
        return;
    }
    ToolCfg* tool = nullptr;
    const ObjectId toolId = ObjectId::fromString(ss(m_shownId));
    for (size_t i = 0; i < d->inner.tools.size(); ++i) {
        if (d->inner.tools[i].id == toolId) {
            tool = &d->inner.tools[i];
            break;
        }
    }
    if (!tool) {
        return;
    }
    const int index = m_algLib->currentIndex();
    if (index < 0 || !m_algLib->itemData(index).isValid()) {
        return;
    }
    tool->algLib = static_cast<AlgLibKind>(m_algLib->itemData(index).toInt());
    QListWidgetItem* cur = m_tools->currentItem();
    if (cur) {
        cur->setText(toolRowText(*tool));
    }
}

// 按列表顺序重排内部工具
void CompositeEditDlg::syncToolOrder()
{
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    if (!d) {
        return;
    }
    std::vector<ToolCfg> ordered;
    for (int i = 0; i < m_tools->count(); ++i) {
        QListWidgetItem* it = m_tools->item(i);
        if (!it) {
            continue;
        }
        const ObjectId id = ObjectId::fromString(ss(it->data(Qt::UserRole).toString()));
        for (size_t k = 0; k < d->inner.tools.size(); ++k) {
            if (d->inner.tools[k].id == id) {
                ordered.push_back(d->inner.tools[k]);
                break;
            }
        }
    }
    if (ordered.size() == d->inner.tools.size()) {
        d->inner.tools.swap(ordered);
    }
}

// 切换算法库
void CompositeEditDlg::onAlgLibChanged(int)
{
    applyAlgLib();
}

// 上移内部工具
void CompositeEditDlg::onMoveUp()
{
    const int row = m_tools->currentRow();
    if (row <= 0) {
        return;
    }
    storePubs();
    QListWidgetItem* it = m_tools->takeItem(row);
    m_tools->insertItem(row - 1, it);
    m_tools->setCurrentItem(it);
    syncToolOrder();
}

// 下移内部工具
void CompositeEditDlg::onMoveDown()
{
    const int row = m_tools->currentRow();
    if (row < 0 || row >= m_tools->count() - 1) {
        return;
    }
    storePubs();
    QListWidgetItem* it = m_tools->takeItem(row);
    m_tools->insertItem(row + 1, it);
    m_tools->setCurrentItem(it);
    syncToolOrder();
}

void CompositeEditDlg::loadPubs()
{
    m_ports->clear();
    m_paras->clear();
    m_algLib->blockSignals(true);
    m_algLib->clear();
    m_filling = true;
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    QListWidgetItem* cur = m_tools->currentItem();
    if (!d || !cur) {
        m_shownId.clear();
        m_algLib->setEnabled(false);
        m_filling = false;
        m_algLib->blockSignals(false);
        return;
    }
    m_shownId = cur->data(Qt::UserRole).toString();
    const ToolCfg* tool = d->inner.findTool(ObjectId::fromString(ss(m_shownId)));
    if (!tool) {
        m_algLib->setEnabled(false);
        m_filling = false;
        m_algLib->blockSignals(false);
        return;
    }
    const std::vector<AlgLibKind> libs = AlgToolBaseRegistry::instance().libs(tool->toolName);
    int pick = -1;
    for (size_t i = 0; i < libs.size(); ++i) {
        const char* title = algLibKindTitle(libs[i]);
        m_algLib->addItem(QString::fromLatin1(title && title[0] ? title : "?"), static_cast<int>(libs[i]));
        if (libs[i] == tool->algLib) {
            pick = static_cast<int>(i);
        }
    }
    if (libs.empty()) {
        m_algLib->addItem(T("无"));
        m_algLib->setEnabled(false);
    } else {
        if (pick < 0) {
            const char* title = algLibKindTitle(tool->algLib);
            m_algLib->addItem(QString::fromLatin1(title && title[0] ? title : "?"), static_cast<int>(tool->algLib));
            pick = m_algLib->count() - 1;
        }
        m_algLib->setCurrentIndex(pick);
        m_algLib->setEnabled(true);
    }
    m_filling = false;
    m_algLib->blockSignals(false);
    ToolBase* proto = ToolRegistry::instance().find(tool->toolName);
    std::vector<PortDesc> ports = proto ? proto->ports() : defaultPortsForType(tool->type);
    for (size_t i = 0; i < ports.size(); ++i) {
        const QString label = QString::fromUtf8(portDataTitle(ports[i].data()).c_str())
            + (ports[i].dir() == PortDir::In ? T(" 输入") : T(" 输出"));
        QListWidgetItem* it = new QListWidgetItem(label, m_ports);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        bool on = false;
        for (size_t k = 0; k < d->ports.size(); ++k) {
            if (d->ports[k].innerToolId == tool->id && d->ports[k].data == ports[i].data() && d->ports[k].dir == ports[i].dir()) {
                on = true;
            }
        }
        it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        it->setData(Qt::UserRole, static_cast<int>(ports[i].dir()));
        it->setData(Qt::UserRole + 1, static_cast<int>(ports[i].data()));
    }
    if (!proto) {
        return;
    }
    const std::vector<ParamField> fields = proto->paramFields();
    for (size_t i = 0; i < fields.size(); ++i) {
        QListWidgetItem* it = new QListWidgetItem(QString::fromUtf8(acpToUtf8(fields[i].title).c_str()), m_paras);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        bool on = false;
        for (size_t k = 0; k < d->paras.size(); ++k) {
            if (d->paras[k].innerToolId == tool->id && d->paras[k].key == fields[i].key) {
                on = true;
            }
        }
        it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        it->setData(Qt::UserRole, qs(fields[i].key));
    }
}

void CompositeEditDlg::storePubs()
{
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    if (!d || m_shownId.isEmpty()) {
        return;
    }
    const ObjectId toolId = ObjectId::fromString(ss(m_shownId));
    std::vector<CompositePort> ports;
    for (size_t i = 0; i < d->ports.size(); ++i) {
        if (d->ports[i].innerToolId != toolId) {
            ports.push_back(d->ports[i]);
        }
    }
    for (int i = 0; i < m_ports->count(); ++i) {
        QListWidgetItem* it = m_ports->item(i);
        if (!it || it->checkState() != Qt::Checked) {
            continue;
        }
        CompositePort p;
        p.name = ss(it->text());
        p.dir = static_cast<PortDir>(it->data(Qt::UserRole).toInt());
        p.data = static_cast<PortData>(it->data(Qt::UserRole + 1).toInt());
        p.innerToolId = toolId;
        p.innerPort = PortSlot(p.dir, p.data);
        ports.push_back(p);
    }
    d->ports.swap(ports);
    std::vector<CompositeParaPub> paras;
    for (size_t i = 0; i < d->paras.size(); ++i) {
        if (d->paras[i].innerToolId != toolId) {
            paras.push_back(d->paras[i]);
        }
    }
    for (int i = 0; i < m_paras->count(); ++i) {
        QListWidgetItem* it = m_paras->item(i);
        if (!it || it->checkState() != Qt::Checked) {
            continue;
        }
        CompositeParaPub p;
        p.name = ss(it->text());
        p.key = ss(it->data(Qt::UserRole).toString());
        p.innerToolId = toolId;
        p.learn = false;
        paras.push_back(p);
    }
    d->paras.swap(paras);
}

void CompositeEditDlg::onAddTool()
{
    storePubs();
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    if (!d) {
        return;
    }
    const std::vector<ToolBase*> tools = ToolRegistry::instance().all();
    QStringList names;
    for (size_t i = 0; i < tools.size(); ++i) {
        if (tools[i]) {
            names << qs(tools[i]->toolName());
        }
    }
    const std::vector<CompositeDef*> defs = CompositeMgr::instance().all();
    for (size_t i = 0; i < defs.size(); ++i) {
        if (!defs[i] || defs[i]->id == d->id) {
            continue;
        }
        if (d->scope == CompositeLibrary && defs[i]->scope == CompositeSolution) {
            continue;
        }
        names << (QLatin1String("composite:") + qs(defs[i]->id.toString()));
    }
    bool ok = false;
    const QString pick = QInputDialog::getItem(this, T("添加工具"), T("注册名"), names, 0, false, &ok);
    if (!ok || pick.isEmpty()) {
        return;
    }
    if (pick == QLatin1String("composite")) {
        return;
    }
    if (pick.startsWith(QLatin1String("composite:"))) {
        const CompositeDef* child = CompositeMgr::instance().find(ObjectId::fromString(ss(pick.mid(10))));
        if (!child) {
            return;
        }
        ToolCfg tool = child->makeNode();
        if (!d->inner.insertTool(tool, ObjectId())) {
            d->inner.tools.push_back(tool);
            d->inner.ensureTree();
        }
        QListWidgetItem* it = new QListWidgetItem(toolRowText(tool));
        it->setData(Qt::UserRole, qs(tool.id.toString()));
        m_tools->addItem(it);
        m_tools->setCurrentItem(it);
        return;
    }
    ToolCfg tool = ToolCfg::make(ss(pick), ObjectId());
    if (tool.id.isNull() && tool.toolName.empty()) {
        tool.id = ObjectId::create();
        tool.toolName = ss(pick);
        tool.name = ss(pick);
    }
    if (!d->inner.insertTool(tool, ObjectId())) {
        d->inner.tools.push_back(tool);
        d->inner.ensureTree();
    }
    QListWidgetItem* it = new QListWidgetItem(toolRowText(tool));
    it->setData(Qt::UserRole, qs(tool.id.toString()));
    m_tools->addItem(it);
    m_tools->setCurrentItem(it);
}

void CompositeEditDlg::onDelTool()
{
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    QListWidgetItem* cur = m_tools->currentItem();
    if (!d || !cur) {
        return;
    }
    const ObjectId id = ObjectId::fromString(ss(cur->data(Qt::UserRole).toString()));
    d->inner.removeTool(id);
    delete cur;
    loadPubs();
}

void CompositeEditDlg::onSave()
{
    applyAlgLib();
    storePubs();
    CompositeDef* d = CompositeMgr::instance().find(ObjectId::fromString(ss(m_id)));
    if (!d) {
        return;
    }
    d->name = ss(m_name->text());
    d->description = ss(m_desc->text());
    std::string err;
    if (d->scope == CompositeLibrary && !CompositeMgr::instance().saveOne(d->id, &err)) {
        QMessageBox::warning(this, T("组合工具"), qs(err));
        return;
    }
    accept();
}

}
#include "moc_CompositeMgrDlg.cpp"
