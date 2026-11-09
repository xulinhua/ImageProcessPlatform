/**
 * @file LicenseDlg.cpp
 * @brief 软件注册页
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "LicenseDlg.h"
#include "Theme.h"
#include "UiText.h"
#include "VisionLicense.h"
#include "VisionVersion.h"
#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

namespace {

/** @brief 状态胶囊的颜色。ok 为绿，bad 为红，其余为灰 */
void paintPill(QLabel* pill, const char* kind)
{
    if (!pill) {
        return;
    }
    QString style = QString::fromLatin1(
        "border-radius: 11px; padding: 3px 10px; font-weight: bold;");
    if (qstrcmp(kind, "ok") == 0) {
        style += QString::fromLatin1("color: #237804; background: #F6FFED; border: 1px solid #B7EB8F;");
    } else if (qstrcmp(kind, "bad") == 0) {
        style += QString::fromLatin1("color: #A8071A; background: #FFF1F0; border: 1px solid #FFA39E;");
    } else {
        style += QString::fromLatin1("color: #595959; background: #FAFAFA; border: 1px solid #D9D9D9;");
    }
    pill->setStyleSheet(style);
}

}

LicenseDlg::LicenseDlg(QWidget* parent)
    : QDialog(parent)
    , m_pill(0)
    , m_state(0)
    , m_id(0)
    , m_term(0)
    , m_serial(0)
{
    setWindowTitle(T("软件注册"));
    setModal(true);
    resize(520, 360);

    QLabel* logo = new QLabel(QLatin1String("V"), this);
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(56, 56);
    logo->setStyleSheet(QLatin1String(
        "QLabel { font-size: 28px; font-weight: bold; color: #1677FF;"
        " border: 2px solid #1677FF; border-radius: 12px; background: #E6F4FF; }"));
    QLabel* product = new QLabel(QString::fromUtf8(appProductName()), this);
    product->setStyleSheet(QLatin1String("font-size: 20px; font-weight: bold; color: #1F1F1F;"));
    m_pill = new QLabel(this);
    m_pill->setAlignment(Qt::AlignCenter);
    QVBoxLayout* nameCol = new QVBoxLayout();
    nameCol->setSpacing(4);
    nameCol->addWidget(product, 0, Qt::AlignVCenter);
    QHBoxLayout* hero = new QHBoxLayout();
    hero->setSpacing(16);
    hero->addWidget(logo, 0, Qt::AlignVCenter);
    hero->addLayout(nameCol, 1);
    hero->addWidget(m_pill, 0, Qt::AlignVCenter);

    QFrame* card = new QFrame(this);
    card->setStyleSheet(QLatin1String(
        "QFrame { background: #FFFFFF; border: 1px solid #E6E8EB; border-radius: 8px; }"
        "QLabel { background: transparent; border: none; }"));
    QVBoxLayout* facts = new QVBoxLayout(card);
    facts->setContentsMargins(16, 12, 16, 12);
    facts->setSpacing(8);
    m_state = addFact(facts, T("许可状态"));
    m_id = addFact(facts, T("编号"));
    m_term = addFact(facts, T("期限"));
    m_serial = addFact(facts, T("序号"));

    QPushButton* reg = new QPushButton(T("注册…"), this);
    setButtonRole(reg, "primary");
    QPushButton* refresh = new QPushButton(T("刷新"), this);
    QPushButton* closeBtn = new QPushButton(T("关闭"), this);
    connect(reg, &QPushButton::clicked, this, [this]() { registerProduct(); });
    connect(refresh, &QPushButton::clicked, this, [this]() { reload(); });
    connect(closeBtn, &QPushButton::clicked, this, [this]() { accept(); });
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(reg);
    buttons->addWidget(refresh);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->setSpacing(14);
    lay->addLayout(hero);
    lay->addWidget(card);
    lay->addStretch(1);
    lay->addLayout(buttons);
    reload();
}

QLabel* LicenseDlg::addFact(QVBoxLayout* layout, const QString& name)
{
    QLabel* key = new QLabel(name, this);
    key->setObjectName(QLatin1String("hint"));
    key->setMinimumWidth(72);
    QLabel* value = new QLabel(QString::fromUtf8("—"), this);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setStyleSheet(QLatin1String("color: #1F1F1F;"));
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(key);
    row->addWidget(value, 1);
    layout->addLayout(row);
    return value;
}

void LicenseDlg::reload()
{
    License license;
    const LicenseReason reason = license.open();
    const LicenseView info = license.view();
    const bool known = !info.licenseId.empty();
    if (reason == LicenseOk) {
        m_pill->setText(T("已注册"));
        paintPill(m_pill, "ok");
        m_state->setText(T("已注册"));
    } else if (reason == LicenseExpired) {
        m_pill->setText(T("已过期"));
        paintPill(m_pill, "bad");
        m_state->setText(T("已过期"));
    } else if (reason == LicenseClock || reason == LicenseRecordsCleared || reason == LicenseDamaged) {
        m_pill->setText(T("需处理"));
        paintPill(m_pill, "bad");
        m_state->setText(QString::fromUtf8(license.reasonText(reason).c_str()));
    } else {
        m_pill->setText(T("未注册"));
        paintPill(m_pill, "idle");
        m_state->setText(QString::fromUtf8(license.reasonText(reason).c_str()));
    }
    m_id->setText(known ? QString::fromUtf8(info.licenseId.c_str()) : QString::fromUtf8("—"));
    if (!known) {
        m_term->setText(QString::fromUtf8("—"));
        m_serial->setText(QString::fromUtf8("—"));
    } else if (info.perpetual) {
        m_term->setText(T("永久"));
        m_serial->setText(QString::number(info.serial));
    } else {
        m_term->setText(T("到期 ") + QString::fromUtf8(info.until.c_str()));
        m_serial->setText(QString::number(info.serial));
    }
}

void LicenseDlg::registerProduct()
{
    const QString path = QApplication::applicationDirPath() + QLatin1String("/Vision.LicenseRequest.exe");
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, T("软件注册"), T("找不到 Vision.LicenseRequest.exe"));
        return;
    }
    QProcess::execute(path, QStringList());
    reload();
}

}
