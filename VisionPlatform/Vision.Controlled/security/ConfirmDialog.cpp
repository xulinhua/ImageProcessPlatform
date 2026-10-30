/**
 * @file ConfirmDialog.cpp
 * @brief 操作申请确认框
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ConfirmDialog.h"
#include "RemoteTheme.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace Vision {

ConfirmDialog::ConfirmDialog(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_timer(new QTimer(this))
    , m_seq(0)
    , m_deadline(0)
    , m_total(30)
{
    // 透明外层留 16 像素边距给阴影，内容画在白色圆角卡片 box 里
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(420 + 32);

    QFrame* box = new QFrame(this);
    box->setObjectName("confirmBox");
    box->setStyleSheet("QFrame#confirmBox{background:#FFFFFF;border-radius:10px;}");
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(box);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 90));
    box->setGraphicsEffect(shadow);

    // 头部：盾牌图标、标题、申请方说明
    QLabel* icon = new QLabel(box);
    icon->setFixedSize(40, 40);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("background:#FFF7E6;border-radius:10px;");
    icon->setPixmap(RemoteTheme::pixmap("shield", QColor("#FA8C16"), 22, devicePixelRatioF()));
    QLabel* title = new QLabel(QString::fromUtf8("远程操作请求"), box);
    title->setStyleSheet("font-size:16px;font-weight:600;");
    m_who = new QLabel(box);
    m_who->setWordWrap(true);
    m_who->setTextFormat(Qt::RichText);
    m_who->setStyleSheet("color:#595959;font-size:12px;");
    QVBoxLayout* headText = new QVBoxLayout();
    headText->setSpacing(4);
    headText->addWidget(title);
    headText->addWidget(m_who);
    QHBoxLayout* head = new QHBoxLayout();
    head->setContentsMargins(20, 16, 20, 10);
    head->setSpacing(12);
    head->addWidget(icon, 0, Qt::AlignTop);
    head->addLayout(headText, 1);

    // 权限清单只作展示：这一期固定为观看 + 操作，其余为后续分期，全部不可改
    QLabel* allow = new QLabel(QString::fromUtf8("允许对方："), box);
    allow->setProperty("role", "small");
    QFrame* perm = new QFrame(box);
    perm->setStyleSheet("QFrame{background:#FAFAFA;border-radius:6px;}");
    QGridLayout* permGrid = new QGridLayout(perm);
    permGrid->setContentsMargins(12, 10, 12, 10);
    permGrid->setHorizontalSpacing(8);
    permGrid->setVerticalSpacing(8);
    const char* const names[] = { "观看", "操作", "剪贴板", "文件", "电源", "隐私屏" };
    for (int i = 0; i < 6; ++i) {
        QCheckBox* check = new QCheckBox(QString::fromUtf8(names[i]), perm);
        check->setChecked(i < 2);
        check->setEnabled(false);
        check->setToolTip(i < 2 ? QString::fromUtf8("本次申请的权限") : QString::fromUtf8("后续分期提供"));
        permGrid->addWidget(check, i / 3, i % 3);
    }
    m_trust = new QCheckBox(QString::fromUtf8("以后信任这个中控台，不再询问"), box);
    m_trust->setToolTip(QString::fromUtf8("按中控台证书指纹识别；指纹未就绪时退回显示名"));
    QVBoxLayout* body = new QVBoxLayout();
    body->setContentsMargins(20, 6, 20, 14);
    body->setSpacing(10);
    body->addWidget(allow);
    body->addWidget(perm);
    body->addWidget(m_trust);

    // 底部：剩余时间进度条，下方为倒计时文字与拒绝、允许按钮
    m_progress = new QProgressBar(box);
    m_progress->setTextVisible(false);
    m_progress->setProperty("tone", "orange");
    m_progress->setStyleSheet("QProgressBar{border-radius:0;max-height:3px;min-height:3px;}QProgressBar::chunk{border-radius:0;background:#FA8C16;}");
    m_progress->setRange(0, 1000);

    QWidget* foot = new QWidget(box);
    foot->setObjectName("confirmFoot");
    foot->setStyleSheet("QWidget#confirmFoot{border-top:1px solid #F0F0F0;}");
    QHBoxLayout* footRow = new QHBoxLayout(foot);
    footRow->setContentsMargins(20, 12, 20, 12);
    footRow->setSpacing(8);
    m_left = new QLabel(foot);
    m_left->setProperty("role", "small");
    QPushButton* reject = RemoteTheme::button(QString::fromUtf8("拒绝"), 0, 0, foot);
    QPushButton* ok = RemoteTheme::button(QString::fromUtf8("允许"), 0, "primary", foot);
    footRow->addWidget(m_left, 1);
    footRow->addWidget(reject);
    footRow->addWidget(ok);

    // 组装并连接按钮与定时器
    QVBoxLayout* col = new QVBoxLayout(box);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addLayout(head);
    col->addLayout(body);
    col->addWidget(m_progress);
    col->addWidget(foot);

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->addWidget(box);

    QObject::connect(reject, &QPushButton::clicked, [this]() { answer(false); });
    QObject::connect(ok, &QPushButton::clicked, [this]() { answer(true); });
    QObject::connect(m_timer, &QTimer::timeout, [this]() { tick(); });
}

uint64_t ConfirmDialog::seq() const
{
    return m_seq;
}

void ConfirmDialog::setOnAnswer(const std::function<void(uint64_t, bool, bool)>& answer)
{
    m_answer = answer;
}

void ConfirmDialog::showRequest(uint64_t seq, const QString& controller, const QString& host, int secondsLeft, int totalSeconds)
{
    m_seq = seq;
    m_total = totalSeconds > 0 ? totalSeconds : 30;
    m_deadline = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(secondsLeft) * 1000;
    m_trust->setChecked(false);
    const QString who = controller.isEmpty() ? QString::fromUtf8("未具名中控台") : controller;
    m_who->setText(QString::fromUtf8("<b style='color:#262626'>%1</b>（<span style='font-family:Consolas'>%2</span>）请求操作本机<br>"
                                     "认证已启用 · 反连 · TLS")
                       .arg(who.toHtmlEscaped(), host.toHtmlEscaped()));
    adjustSize();
    const QRect area = QApplication::primaryScreen()->availableGeometry();
    move(area.right() - width() - 16, area.bottom() - height() - 24);
    tick();
    show();
    raise();
    m_timer->start(100);
}

void ConfirmDialog::tick()
{
    const qint64 left = m_deadline - QDateTime::currentMSecsSinceEpoch();
    if (left <= 0) {
        m_timer->stop();
        hide();
        return;
    }
    m_progress->setValue(static_cast<int>(left * 1000 / (static_cast<qint64>(m_total) * 1000)));
    m_left->setText(QString::fromUtf8("%1 秒后自动拒绝").arg((left + 999) / 1000));
}

void ConfirmDialog::answer(bool allow)
{
    m_timer->stop();
    hide();
    if (m_answer) {
        m_answer(m_seq, allow, allow && m_trust->isChecked());
    }
}

}
