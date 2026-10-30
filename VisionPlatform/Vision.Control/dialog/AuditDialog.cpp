/**
 * @file AuditDialog.cpp
 * @brief 审计日志窗口
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AuditDialog.h"
#include "RemoteTheme.h"
#include "RemoteWidgets.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QVBoxLayout>
#include <set>

namespace Vision {

AuditDialog::AuditDialog(const AuditLog* audit, QWidget* parent)
    : QDialog(parent)
    , m_audit(audit)
    , m_page(0)
{
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::WindowMinMaxButtonsHint);
    setWindowTitle(QString::fromUtf8("审计日志"));
    setWindowIcon(RemoteTheme::icon("list", QColor(RemoteTheme::kPrimary), 16));
    resize(1280, 720);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(QStringList() << QString::fromUtf8("时间") << QString::fromUtf8("设备")
                                                     << QString::fromUtf8("操作者") << QString::fromUtf8("事件")
                                                     << QString::fromUtf8("来源") << QString::fromUtf8("详情"));
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(38);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->resizeSection(0, 130);
    m_table->horizontalHeader()->resizeSection(1, 150);
    m_table->horizontalHeader()->resizeSection(2, 120);
    m_table->horizontalHeader()->resizeSection(3, 100);
    m_table->horizontalHeader()->resizeSection(4, 200);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);

    QFrame* list = new QFrame(this);
    list->setObjectName("card");
    QVBoxLayout* listCol = new QVBoxLayout(list);
    listCol->setContentsMargins(1, 1, 1, 1);
    listCol->addWidget(m_table);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildFilter());
    root->addWidget(buildStats());
    QHBoxLayout* listRow = new QHBoxLayout();
    listRow->setContentsMargins(12, 0, 12, 0);
    listRow->addWidget(list);
    root->addLayout(listRow, 1);
    root->addWidget(buildPager());
    query();
}

QWidget* AuditDialog::buildFilter()
{
    QWidget* bar = new QWidget(this);
    bar->setObjectName("toolbar");
    bar->setAttribute(Qt::WA_StyledBackground);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 12, 12, 12);
    row->setSpacing(8);
    // 默认区间：本月 1 日到今天
    const QDate today = QDate::currentDate();
    m_from = new QDateEdit(QDate(today.year(), today.month(), 1), bar);
    m_to = new QDateEdit(today, bar);
    QDateEdit* dates[] = { m_from, m_to };
    for (int i = 0; i < 2; ++i) {
        dates[i]->setCalendarPopup(true);
        dates[i]->setDisplayFormat("yyyy-MM-dd");
        dates[i]->setFixedWidth(120);
    }
    QLabel* calendar = new QLabel(bar);
    calendar->setPixmap(RemoteTheme::pixmap("calendar", QColor(RemoteTheme::kMuted), 14, devicePixelRatioF()));
    m_device = new QComboBox(bar);
    m_event = new QComboBox(bar);
    m_op = new QComboBox(bar);
    QComboBox* combos[] = { m_device, m_event, m_op };
    for (int i = 0; i < 3; ++i) {
        combos[i]->setFixedWidth(130);
    }
    m_search = new QLineEdit(bar);
    m_search->setPlaceholderText(QString::fromUtf8("搜索 IP、路径、详情"));
    m_search->addAction(RemoteTheme::icon("search", QColor(RemoteTheme::kMuted), 14), QLineEdit::LeadingPosition);
    m_search->setFixedWidth(220);
    QPushButton* find = RemoteTheme::button(QString::fromUtf8("查询"), "search", "primary", bar);
    QPushButton* csv = RemoteTheme::button(QString::fromUtf8("导出 CSV"), "download", 0, bar);

    row->addWidget(calendar);
    row->addWidget(m_from);
    row->addWidget(new QLabel("~", bar));
    row->addWidget(m_to);
    row->addWidget(m_device);
    row->addWidget(m_event);
    row->addWidget(m_op);
    row->addWidget(m_search);
    row->addWidget(find);
    row->addWidget(csv);
    row->addStretch(1);

    // 「查询」重新读盘；下拉框和回车只在内存里筛选
    QObject::connect(find, &QPushButton::clicked, [this]() { query(); });
    QObject::connect(csv, &QPushButton::clicked, [this]() { exportCsv(); });
    QObject::connect(m_search, &QLineEdit::returnPressed, [this]() { applyFilter(); });
    for (int i = 0; i < 3; ++i) {
        QObject::connect(combos[i], static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int) { applyFilter(); });
    }
    return bar;
}

QLabel* AuditDialog::addCard(QHBoxLayout* row, const QString& title)
{
    QFrame* card = new QFrame(row->parentWidget());
    card->setObjectName("card");
    QVBoxLayout* col = new QVBoxLayout(card);
    col->setContentsMargins(16, 12, 16, 12);
    col->setSpacing(4);
    QLabel* head = new QLabel(title, card);
    head->setProperty("role", "small");
    QLabel* value = new QLabel("0", card);
    value->setStyleSheet("font-size:22px;font-weight:600;");
    col->addWidget(head);
    col->addWidget(value);
    row->addWidget(card, 1);
    return value;
}

QWidget* AuditDialog::buildStats()
{
    QWidget* box = new QWidget(this);
    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(12, 12, 12, 12);
    row->setSpacing(12);
    m_sessions = addCard(row, QString::fromUtf8("会话"));
    m_hours = addCard(row, QString::fromUtf8("累计时长"));
    m_files = addCard(row, QString::fromUtf8("文件传输"));
    m_rejects = addCard(row, QString::fromUtf8("操作被拒"));
    m_shots = addCard(row, QString::fromUtf8("截图"));
    m_files->setText("0");
    m_rejects->setStyleSheet("font-size:22px;font-weight:600;color:#CF1322;");
    return box;
}

QWidget* AuditDialog::buildPager()
{
    QWidget* bar = new QWidget(this);
    QHBoxLayout* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 10, 12, 10);
    row->setSpacing(6);
    m_total = new QLabel(bar);
    m_total->setStyleSheet("color:#595959;font-size:12px;");
    m_pages = new QHBoxLayout();
    m_pages->setSpacing(6);
    m_perPage = new QComboBox(bar);
    m_perPage->addItem(QString::fromUtf8("11 条/页"), 11);
    m_perPage->addItem(QString::fromUtf8("20 条/页"), 20);
    m_perPage->addItem(QString::fromUtf8("50 条/页"), 50);
    m_perPage->addItem(QString::fromUtf8("100 条/页"), 100);
    m_perPage->setCurrentIndex(1);
    m_perPage->setProperty("size", "sm");
    m_perPage->setFixedWidth(100);
    row->addStretch(1);
    row->addWidget(m_total);
    row->addLayout(m_pages);
    row->addWidget(m_perPage);
    QObject::connect(m_perPage, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int) { showPage(0); });
    return bar;
}

void AuditDialog::refillCombo(QComboBox* combo, const QString& all, const QStringList& values)
{
    const QString keep = combo->currentData().toString();
    combo->clear();
    combo->addItem(all, QString());
    for (int i = 0; i < values.size(); ++i) {
        combo->addItem(values.at(i), values.at(i));
    }
    const int at = combo->findData(keep);
    combo->setCurrentIndex(at < 0 ? 0 : at);
}

void AuditDialog::query()
{
    QDate from = m_from->date();
    QDate to = m_to->date();
    if (from > to) {
        std::swap(from, to);
    }
    m_all = m_audit->query(from, to);
    QStringList devices;
    QStringList events;
    QStringList ops;
    for (size_t i = 0; i < m_all.size(); ++i) {
        if (!m_all[i].device.isEmpty() && !devices.contains(m_all[i].device)) {
            devices.append(m_all[i].device);
        }
        if (!events.contains(m_all[i].event)) {
            events.append(m_all[i].event);
        }
        if (!m_all[i].op.isEmpty() && !ops.contains(m_all[i].op)) {
            ops.append(m_all[i].op);
        }
    }
    devices.sort();
    ops.sort();
    refillCombo(m_device, QString::fromUtf8("全部设备"), devices);
    refillCombo(m_op, QString::fromUtf8("全部操作者"), ops);
    const QString keepEvent = m_event->currentData().toString();
    m_event->clear();
    m_event->addItem(QString::fromUtf8("全部事件"), QString());
    for (int i = 0; i < events.size(); ++i) {
        m_event->addItem(AuditLog::title(events.at(i)), events.at(i));
    }
    const int at = m_event->findData(keepEvent);
    m_event->setCurrentIndex(at < 0 ? 0 : at);
    applyFilter();
}

void AuditDialog::applyFilter()
{
    const QString device = m_device->currentData().toString();
    const QString event = m_event->currentData().toString();
    const QString op = m_op->currentData().toString();
    const QString text = m_search->text().trimmed();
    m_rows.clear();
    int sessions = 0;
    int rejects = 0;
    int shots = 0;
    int files = 0;
    qint64 seconds = 0;
    for (size_t i = 0; i < m_all.size(); ++i) {
        const AuditLog::Entry& e = m_all[i];
        if (!device.isEmpty() && e.device != device) {
            continue;
        }
        if (!event.isEmpty() && e.event != event) {
            continue;
        }
        if (!op.isEmpty() && e.op != op) {
            continue;
        }
        if (!text.isEmpty() && !e.source.contains(text, Qt::CaseInsensitive) && !e.detail.contains(text, Qt::CaseInsensitive)
            && !e.device.contains(text, Qt::CaseInsensitive)) {
            continue;
        }
        m_rows.push_back(e);
        if (e.event == "session.start") {
            sessions += 1;
        } else if (e.event == "session.end") {
            seconds += e.seconds;
        } else if (e.event == "operate.reject") {
            rejects += 1;
        } else if (e.event == "screenshot") {
            shots += 1;
        } else if (e.event == "file.upload" || e.event == "file.download") {
            files += 1;
        }
    }
    m_sessions->setText(QLocale(QLocale::English).toString(sessions));
    m_hours->setText(seconds >= 3600 ? QString::fromUtf8("%1 小时").arg(seconds / 3600) : AuditLog::duration(seconds));
    m_rejects->setText(QString::number(rejects));
    m_shots->setText(QString::number(shots));
    m_files->setText(QString::number(files));
    m_files->setStyleSheet("font-size:22px;font-weight:600;color:#262626;");
    showPage(0);
}

void AuditDialog::showPage(int page)
{
    const int per = m_perPage->currentData().toInt() > 0 ? m_perPage->currentData().toInt() : 20;
    const int total = static_cast<int>(m_rows.size());
    const int pages = total == 0 ? 1 : (total + per - 1) / per;
    m_page = page < 0 ? 0 : (page >= pages ? pages - 1 : page);

    // 当前页的行；被拒记录整行浅红底
    m_table->setRowCount(0);
    const int begin = m_page * per;
    for (int i = begin; i < total && i < begin + per; ++i) {
        const AuditLog::Entry& e = m_rows[static_cast<size_t>(i)];
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        QTableWidgetItem* time = new QTableWidgetItem(e.time.toString("MM-dd HH:mm:ss"));
        time->setFont(QFont("Consolas", 9));
        m_table->setItem(row, 0, time);
        m_table->setItem(row, 1, new QTableWidgetItem(e.device.isEmpty() ? QString::fromUtf8("—") : e.device));
        m_table->setItem(row, 2, new QTableWidgetItem(e.op.isEmpty() ? QString::fromUtf8("—") : e.op));
        QWidget* tagBox = new QWidget(m_table);
        QHBoxLayout* tagRow = new QHBoxLayout(tagBox);
        tagRow->setContentsMargins(10, 0, 0, 0);
        tagRow->addWidget(new TagLabel(AuditLog::title(e.event), AuditLog::tone(e.event), tagBox));
        tagRow->addStretch(1);
        m_table->setCellWidget(row, 3, tagBox);
        QTableWidgetItem* source = new QTableWidgetItem(e.source.isEmpty() ? QString::fromUtf8("—") : e.source);
        m_table->setItem(row, 4, source);
        QString detail = e.detail;
        if (e.seconds > 0) {
            detail += (detail.isEmpty() ? QString() : QString::fromUtf8(" · ")) + QString::fromUtf8("时长 ") + AuditLog::duration(e.seconds);
        }
        QTableWidgetItem* detailItem = new QTableWidgetItem(detail);
        detailItem->setToolTip(detail);
        m_table->setItem(row, 5, detailItem);
        if (e.event == "operate.reject") {
            for (int c = 0; c < 6; ++c) {
                if (m_table->item(row, c)) {
                    m_table->item(row, c)->setBackground(QColor("#FFFBFA"));
                }
            }
        }
    }

    // 重建页码按钮：上一页、首页、当前页前后各一页、尾页、下一页，不连续处加省略号
    m_total->setText(QString::fromUtf8("共 %1 条").arg(QLocale(QLocale::English).toString(total)));
    // 常在某个页码按钮的 clicked 里调用，旧按钮只能延后删除
    while (QLayoutItem* item = m_pages->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    QWidget* owner = m_total->parentWidget();
    auto pageButton = [this, owner](const QString& text, const char* icon, int target, bool current, bool enabled) {
        QPushButton* b = RemoteTheme::button(text, icon, current ? "primary" : 0, owner);
        b->setProperty("size", "sm");
        b->setMinimumWidth(26);
        b->setEnabled(enabled);
        QObject::connect(b, &QPushButton::clicked, [this, target]() { showPage(target); });
        m_pages->addWidget(b);
    };
    pageButton(QString(), "left", m_page - 1, false, m_page > 0);
    std::set<int> shown;
    shown.insert(0);
    shown.insert(pages - 1);
    for (int p = m_page - 1; p <= m_page + 1; ++p) {
        if (p >= 0 && p < pages) {
            shown.insert(p);
        }
    }
    int last = -1;
    for (std::set<int>::const_iterator it = shown.begin(); it != shown.end(); ++it) {
        if (last >= 0 && *it > last + 1) {
            QLabel* gap = new QLabel(QString::fromUtf8("…"), owner);
            m_pages->addWidget(gap);
        }
        pageButton(QString::number(*it + 1), 0, *it, *it == m_page, true);
        last = *it;
    }
    pageButton(QString(), "right", m_page + 1, false, m_page + 1 < pages);
}

QString AuditDialog::csvField(const QString& text)
{
    QString out = text;
    out.replace("\"", "\"\"");
    return "\"" + out + "\"";
}

void AuditDialog::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出 CSV"),
        QString("audit_%1.csv").arg(QDate::currentDate().toString("yyyyMMdd")), "CSV (*.csv)");
    if (path.isEmpty()) {
        return;
    }
    QString text = QString::fromUtf8("时间,设备,设备 ID,操作者,事件,来源,详情,时长(秒)\r\n");
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const AuditLog::Entry& e = m_rows[i];
        text += csvField(e.time.toString("yyyy-MM-dd HH:mm:ss")) + "," + csvField(e.device) + "," + csvField(e.deviceId) + ","
            + csvField(e.op) + "," + csvField(AuditLog::title(e.event)) + "," + csvField(e.source) + "," + csvField(e.detail) + ","
            + QString::number(e.seconds) + "\r\n";
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("无法写入 %1").arg(path));
        return;
    }
    file.write("\xEF\xBB\xBF");
    file.write(text.toUtf8());
    if (!file.commit()) {
        QMessageBox::warning(this, windowTitle(), QString::fromUtf8("无法写入 %1").arg(path));
        return;
    }
    QMessageBox::information(this, windowTitle(), QString::fromUtf8("已导出 %1 条到\n%2").arg(m_rows.size()).arg(QDir::toNativeSeparators(path)));
}

}
