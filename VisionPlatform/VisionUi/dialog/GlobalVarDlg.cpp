/**
 * @file GlobalVarDlg.cpp
 * @brief GlobalVarDlg 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "GlobalVarDlg.h"
#include "QtConv.h"
#include "Theme.h"
#include "UiText.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace Vision {

QString GlobalVarDlg::typeTitle(GlobalVarType t)
{
    switch (t) {
    case GlobalVarInt: return QString::fromLatin1("Int");
    case GlobalVarFloat: return QString::fromLatin1("Float");
    case GlobalVarBool: return QString::fromLatin1("Bool");
    case GlobalVarString: return QString::fromLatin1("String");
    default: return QString::fromLatin1("Int");
    }
}

GlobalVarType GlobalVarDlg::typeFromCombo(int idx)
{
    if (idx < 0 || idx > 3) {
        return GlobalVarInt;
    }
    return static_cast<GlobalVarType>(idx);
}

QComboBox* GlobalVarDlg::makeTypeCombo(QWidget* parent, GlobalVarType t)
{
    QComboBox* cb = new QComboBox(parent);
    cb->addItem(typeTitle(GlobalVarInt));
    cb->addItem(typeTitle(GlobalVarFloat));
    cb->addItem(typeTitle(GlobalVarBool));
    cb->addItem(typeTitle(GlobalVarString));
    cb->setCurrentIndex(static_cast<int>(t));
    return cb;
}

GlobalVarDlg::GlobalVarDlg(Solution* sol, QWidget* parent)
    : QDialog(parent)
    , m_sol(sol)
    , m_table(0)
{
    setWindowTitle(T("全局变量"));
    setModal(true);
    resize(720, 420);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    QStringList headers;
    headers << T("名称") << T("类型") << T("值") << T("注释");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    lay->addWidget(m_table, 1);

    QHBoxLayout* ops = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton(T("添加"), this);
    QPushButton* remBtn = new QPushButton(T("删除"), this);
    QPushButton* impBtn = new QPushButton(T("导入..."), this);
    QPushButton* expBtn = new QPushButton(T("导出..."), this);
    ops->addWidget(addBtn);
    ops->addWidget(remBtn);
    ops->addStretch();
    ops->addWidget(impBtn);
    ops->addWidget(expBtn);
    lay->addLayout(ops);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(box);

    connect(addBtn, SIGNAL(clicked()), this, SLOT(onAdd()));
    connect(remBtn, SIGNAL(clicked()), this, SLOT(onRemove()));
    connect(impBtn, SIGNAL(clicked()), this, SLOT(onImport()));
    connect(expBtn, SIGNAL(clicked()), this, SLOT(onExport()));
    connect(box, SIGNAL(accepted()), this, SLOT(onSave()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    loadTable();
    enableDialogResize(this);
}

void GlobalVarDlg::loadTable()
{
    m_table->setRowCount(0);
    if (!m_sol) {
        return;
    }
    const std::vector<GlobalVar> items = m_sol->globalVars.items();
    for (size_t i = 0; i < items.size(); ++i) {
        const GlobalVar& g = items[i];
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(qs(g.name)));
        m_table->setCellWidget(r, 1, makeTypeCombo(m_table, g.type));
        m_table->setItem(r, 2, new QTableWidgetItem(qs(g.value.toString())));
        m_table->setItem(r, 3, new QTableWidgetItem(qs(g.comment)));
    }
}

bool GlobalVarDlg::collectItems(std::vector<GlobalVar>* out, QString* err) const
{
    if (!out) {
        return false;
    }
    out->clear();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* nameItem = m_table->item(r, 0);
        QTableWidgetItem* valItem = m_table->item(r, 2);
        QTableWidgetItem* cmtItem = m_table->item(r, 3);
        QComboBox* typeCb = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        GlobalVar g;
        g.name = ss(nameItem ? nameItem->text().trimmed() : QString());
        g.type = typeFromCombo(typeCb ? typeCb->currentIndex() : 0);
        g.comment = ss(cmtItem ? cmtItem->text() : QString());
        g.value = VarStore::valueFromText(g.type, ss(valItem ? valItem->text() : QString()));
        if (g.name.empty()) {
            if (err) {
                *err = T("第 %1 行名称为空").arg(r + 1);
            }
            return false;
        }
        for (size_t i = 0; i < out->size(); ++i) {
            if ((*out)[i].name == g.name) {
                if (err) {
                    *err = T("变量名重复：") + qs(g.name);
                }
                return false;
            }
        }
        out->push_back(g);
    }
    return true;
}

void GlobalVarDlg::onAdd()
{
    const int r = m_table->rowCount();
    m_table->insertRow(r);
    QString name = QString::fromLatin1("Var%1").arg(r + 1);
    m_table->setItem(r, 0, new QTableWidgetItem(name));
    m_table->setCellWidget(r, 1, makeTypeCombo(m_table, GlobalVarInt));
    m_table->setItem(r, 2, new QTableWidgetItem(QString::fromLatin1("0")));
    m_table->setItem(r, 3, new QTableWidgetItem(QString()));
    m_table->selectRow(r);
}

void GlobalVarDlg::onRemove()
{
    const int r = m_table->currentRow();
    if (r >= 0) {
        m_table->removeRow(r);
    }
}

void GlobalVarDlg::onImport()
{
    const QString path = QFileDialog::getOpenFileName(this, T("导入全局变量"),
                                                      QString(),
                                                      T("JSON (*.json);;CSV (*.csv);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, T("导入"), T("无法打开文件"));
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    std::vector<GlobalVar> loaded;
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        const QStringList lines = QString::fromUtf8(raw).split(QRegExp(QString::fromLatin1("[\r\n]+")), QString::SkipEmptyParts);
        for (int i = 0; i < lines.size(); ++i) {
            const QStringList cols = lines[i].split(QLatin1Char(','));
            if (cols.size() < 3) {
                continue;
            }
            if (i == 0 && cols[0].trimmed().compare(QLatin1String("name"), Qt::CaseInsensitive) == 0) {
                continue;
            }
            GlobalVar g;
            g.name = ss(cols[0].trimmed());
            g.type = typeFromCombo(cols[1].trimmed().toInt());
            const QString tyName = cols[1].trimmed();
            if (tyName.compare(QLatin1String("Float"), Qt::CaseInsensitive) == 0) {
                g.type = GlobalVarFloat;
            } else if (tyName.compare(QLatin1String("Bool"), Qt::CaseInsensitive) == 0) {
                g.type = GlobalVarBool;
            } else if (tyName.compare(QLatin1String("String"), Qt::CaseInsensitive) == 0) {
                g.type = GlobalVarString;
            } else if (tyName.compare(QLatin1String("Int"), Qt::CaseInsensitive) == 0) {
                g.type = GlobalVarInt;
            }
            g.value = VarStore::valueFromText(g.type, ss(cols[2].trimmed()));
            if (cols.size() > 3) {
                g.comment = ss(cols[3].trimmed());
            }
            if (!g.name.empty()) {
                loaded.push_back(g);
            }
        }
    } else {
        QJsonParseError jerr;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &jerr);
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject() && doc.object().contains(QLatin1String("globalVars"))) {
            arr = doc.object().value(QLatin1String("globalVars")).toArray();
        } else {
            QMessageBox::warning(this, T("导入"), T("JSON 格式不正确"));
            return;
        }
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonObject o = arr.at(i).toObject();
            GlobalVar g;
            g.name = ss(o.value(QLatin1String("name")).toString());
            g.type = static_cast<GlobalVarType>(o.value(QLatin1String("type")).toInt(0));
            g.comment = ss(o.value(QLatin1String("comment")).toString());
            g.value = VarStore::valueFromText(g.type, ss(o.value(QLatin1String("value")).toString()));
            if (!g.name.empty()) {
                loaded.push_back(g);
            }
        }
    }
    m_table->setRowCount(0);
    for (size_t i = 0; i < loaded.size(); ++i) {
        const GlobalVar& g = loaded[i];
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(qs(g.name)));
        m_table->setCellWidget(r, 1, makeTypeCombo(m_table, g.type));
        m_table->setItem(r, 2, new QTableWidgetItem(qs(g.value.toString())));
        m_table->setItem(r, 3, new QTableWidgetItem(qs(g.comment)));
    }
}

void GlobalVarDlg::onExport()
{
    std::vector<GlobalVar> items;
    QString err;
    if (!collectItems(&items, &err)) {
        QMessageBox::warning(this, T("导出"), err);
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, T("导出全局变量"),
                                                      QString::fromLatin1("globalVars.json"),
                                                      T("JSON (*.json);;CSV (*.csv);;所有文件 (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, T("导出"), T("无法写入文件"));
        return;
    }
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        QTextStream ts(&f);
        ts.setCodec("UTF-8");
        ts << "name,type,value,comment\n";
        for (size_t i = 0; i < items.size(); ++i) {
            const GlobalVar& g = items[i];
            ts << qs(g.name) << QLatin1Char(',')
               << static_cast<int>(g.type) << QLatin1Char(',')
               << qs(g.value.toString()) << QLatin1Char(',')
               << qs(g.comment) << QLatin1Char('\n');
        }
    } else {
        QJsonArray arr;
        for (size_t i = 0; i < items.size(); ++i) {
            const GlobalVar& g = items[i];
            QJsonObject o;
            o.insert(QLatin1String("name"), qs(g.name));
            o.insert(QLatin1String("type"), static_cast<int>(g.type));
            o.insert(QLatin1String("value"), qs(g.value.toString()));
            o.insert(QLatin1String("comment"), qs(g.comment));
            arr.append(o);
        }
        QJsonObject root;
        root.insert(QLatin1String("globalVars"), arr);
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
    f.close();
}

void GlobalVarDlg::onSave()
{
    if (!m_sol) {
        reject();
        return;
    }
    std::vector<GlobalVar> items;
    QString err;
    if (!collectItems(&items, &err)) {
        QMessageBox::warning(this, T("全局变量"), err);
        return;
    }
    m_sol->globalVars.setItems(items);
    m_sol->dirty = true;
    accept();
}

}

#include "moc_GlobalVarDlg.cpp"