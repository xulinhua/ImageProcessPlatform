/**
 * @file AgentWindow.cpp
 * @brief 被控端窗口。上排是设备卡和当前会话，下排是共享范围，预览固定在右侧
 */

#include "AgentWindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleFactory>
#include <QTime>
#include <QVBoxLayout>

namespace Vision {

AgentWindow::AgentWindow(QWidget* parent)
    : QWidget(parent)
    , m_id(0)
    , m_code(0)
    , m_fingerprint(0)
    , m_desk(0)
    , m_state(0)
    , m_dot(0)
    , m_count(0)
    , m_sessionRow(0)
    , m_sessionMeta(0)
    , m_sessionTag(0)
    , m_noSession(0)
    , m_previewInfo(0)
    , m_preview(0)
    , m_desktop(0)
    , m_window(0)
    , m_monitor(0)
    , m_processBox(0)
    , m_titleEdit(0)
    , m_chat(0)
    , m_chatInput(0)
    , m_file(0)
    , m_desktopShare(true)
{
    setWindowTitle(QString::fromUtf8("被控端"));
    applyTheme();
    loadIcon();
    resize(960, 680);

    QFrame* idCard = new QFrame(this);
    idCard->setObjectName("idCard");
    idCard->setFixedWidth(380);
    idCard->setStyleSheet(
        "QFrame#idCard{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #1677FF,stop:1 #4096FF);border-radius:6px;}"
        "QFrame#idCard QLabel{color:#FFFFFF;background:transparent;}"
        "QFrame#idCard QPushButton{background:rgba(255,255,255,46);border:none;}"
        "QFrame#idCard QPushButton:hover{background:rgba(255,255,255,80);}");
    QVBoxLayout* idCol = new QVBoxLayout(idCard);
    idCol->setContentsMargins(20, 18, 20, 18);
    idCol->setSpacing(14);

    QLabel* idKey = new QLabel(QString::fromUtf8("本机设备 ID"), idCard);
    idKey->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    m_id = new QLabel(QString::fromUtf8("—"), idCard);
    m_id->setStyleSheet("font-size:30px;font-weight:600;letter-spacing:3px;font-family:'Segoe UI','Noto Sans CJK SC','Ubuntu';");
    m_id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_id->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    QPushButton* copy = new QPushButton(idCard);
    copy->setIcon(copyIcon());
    copy->setIconSize(QSize(14, 14));
    copy->setFixedSize(24, 24);
    copy->setCursor(Qt::PointingHandCursor);
    copy->setFocusPolicy(Qt::NoFocus);
    copy->setToolTip(QString::fromUtf8("复制设备 ID"));
    QObject::connect(copy, &QPushButton::clicked, this, &AgentWindow::copyId);
    QHBoxLayout* idRow = new QHBoxLayout();
    idRow->setSpacing(10);
    idRow->addWidget(m_id);
    idRow->addWidget(copy, 0, Qt::AlignVCenter);
    idRow->addStretch(1);
    QVBoxLayout* idBlock = new QVBoxLayout();
    idBlock->setSpacing(2);
    idBlock->addWidget(idKey);
    idBlock->addLayout(idRow);
    idCol->addLayout(idBlock);

    QFrame* hr = new QFrame(idCard);
    hr->setFixedHeight(1);
    hr->setStyleSheet("background:rgba(255,255,255,64);");
    idCol->addWidget(hr);

    QLabel* codeKey = new QLabel(QString::fromUtf8("一次性验证码"), idCard);
    codeKey->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    m_code = new QLabel(QString::fromUtf8("— — —"), idCard);
    m_code->setStyleSheet("font-size:24px;font-weight:600;letter-spacing:4px;color:#FFFFFF;");
    QPushButton* again = new QPushButton(idCard);
    again->setIcon(refreshIcon());
    again->setIconSize(QSize(14, 14));
    again->setFixedSize(24, 24);
    again->setCursor(Qt::PointingHandCursor);
    again->setFocusPolicy(Qt::NoFocus);
    again->setToolTip(QString::fromUtf8("刷新验证码"));
    QObject::connect(again, &QPushButton::clicked, this, &AgentWindow::codeRefresh);
    QLabel* clock = new QLabel(idCard);
    clock->setPixmap(clockIcon());
    clock->setFixedSize(12, 12);
    QLabel* codeNote = new QLabel(QString::fromUtf8("10 分钟后或认证成功后自动刷新"), idCard);
    codeNote->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    QHBoxLayout* codeRow = new QHBoxLayout();
    codeRow->setSpacing(10);
    codeRow->addWidget(m_code);
    codeRow->addWidget(again, 0, Qt::AlignVCenter);
    codeRow->addStretch(1);
    QHBoxLayout* noteRow = new QHBoxLayout();
    noteRow->setSpacing(10);
    noteRow->addWidget(clock, 0, Qt::AlignVCenter);
    noteRow->addWidget(codeNote);
    noteRow->addStretch(1);
    QVBoxLayout* codeCol = new QVBoxLayout();
    codeCol->setSpacing(4);
    codeCol->addWidget(codeKey);
    codeCol->addLayout(codeRow);
    codeCol->addLayout(noteRow);
    idCol->addLayout(codeCol);

    QFrame* hr2 = new QFrame(idCard);
    hr2->setFixedHeight(1);
    hr2->setStyleSheet("background:rgba(255,255,255,64);");
    idCol->addWidget(hr2);

    QLabel* fpKey = new QLabel(QString::fromUtf8("本机证书指纹"), idCard);
    fpKey->setStyleSheet("font-size:12px;color:rgba(255,255,255,217);");
    m_fingerprint = new QLabel(QString::fromUtf8("-"), idCard);
    m_fingerprint->setStyleSheet("font-size:12px;font-family:Consolas,'Segoe UI';");
    m_fingerprint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* copyFp = new QPushButton(idCard);
    copyFp->setIcon(copyIcon());
    copyFp->setIconSize(QSize(14, 14));
    copyFp->setFixedSize(24, 24);
    copyFp->setCursor(Qt::PointingHandCursor);
    copyFp->setFocusPolicy(Qt::NoFocus);
    copyFp->setToolTip(QString::fromUtf8("复制证书指纹"));
    QObject::connect(copyFp, &QPushButton::clicked, this, &AgentWindow::copyFingerprint);
    QHBoxLayout* fpRow = new QHBoxLayout();
    fpRow->setSpacing(10);
    fpRow->addWidget(m_fingerprint);
    fpRow->addWidget(copyFp, 0, Qt::AlignVCenter);
    fpRow->addStretch(1);
    QVBoxLayout* fpBlock = new QVBoxLayout();
    fpBlock->setSpacing(2);
    fpBlock->addWidget(fpKey);
    fpBlock->addLayout(fpRow);
    idCol->addLayout(fpBlock);
    idCol->addStretch(1);

    QFrame* session = new QFrame(this);
    session->setObjectName("card");
    QVBoxLayout* sessionCol = new QVBoxLayout(session);
    sessionCol->setContentsMargins(16, 14, 16, 14);
    sessionCol->setSpacing(10);
    QLabel* sessionTitle = new QLabel(QString::fromUtf8("当前会话"), session);
    sessionTitle->setProperty("role", QString::fromUtf8("section"));
    m_count = new QLabel(QString::fromUtf8("0"), session);
    m_count->setProperty("tone", QString::fromUtf8("gray"));
    m_count->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QHBoxLayout* sessionHead = new QHBoxLayout();
    sessionHead->setSpacing(8);
    sessionHead->addWidget(sessionTitle);
    sessionHead->addWidget(m_count);
    sessionHead->addStretch(1);
    sessionCol->addLayout(sessionHead);

    m_dot = new QLabel(session);
    m_dot->setFixedSize(8, 8);
    m_dot->setStyleSheet("background:#BFBFBF;border-radius:4px;");
    m_state = new QLabel(QString::fromUtf8("正在启动"), session);
    m_state->setStyleSheet("font-size:12px;color:#595959;");
    m_desk = new QLabel(QString::fromUtf8("桌面 —"), session);
    m_desk->setStyleSheet("font-size:12px;color:#595959;");
    QLabel* relay = new QLabel(QString::fromUtf8("中继 未启用"), session);
    relay->setStyleSheet("font-size:12px;color:#595959;");
    QHBoxLayout* svc = new QHBoxLayout();
    svc->setSpacing(6);
    svc->addWidget(m_dot, 0, Qt::AlignVCenter);
    svc->addWidget(m_state);
    svc->addSpacing(12);
    svc->addWidget(m_desk);
    svc->addSpacing(12);
    svc->addWidget(relay);
    svc->addStretch(1);
    sessionCol->addLayout(svc);

    m_sessionRow = new QFrame(session);
    m_sessionRow->setObjectName("sessionRow");
    m_sessionRow->hide();
    QHBoxLayout* row = new QHBoxLayout(m_sessionRow);
    row->setContentsMargins(10, 8, 10, 8);
    row->setSpacing(10);
    QLabel* avatar = new QLabel(QString::fromUtf8("控"), m_sessionRow);
    avatar->setFixedSize(30, 30);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet("background:#1890FF;color:#FFFFFF;border-radius:15px;font-size:12px;");
    QLabel* who = new QLabel(QString::fromUtf8("中控台"), m_sessionRow);
    m_sessionMeta = new QLabel(m_sessionRow);
    m_sessionMeta->setProperty("role", QString::fromUtf8("small"));
    QVBoxLayout* info = new QVBoxLayout();
    info->setSpacing(2);
    info->addWidget(who);
    info->addWidget(m_sessionMeta);
    m_sessionTag = new QLabel(QString::fromUtf8("观看"), m_sessionRow);
    m_sessionTag->setProperty("tone", QString::fromUtf8("gray"));
    m_sessionTag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->addWidget(avatar);
    row->addLayout(info, 1);
    row->addWidget(m_sessionTag);
    sessionCol->addWidget(m_sessionRow);

    m_noSession = new QLabel(QString::fromUtf8("还没有会话"), session);
    m_noSession->setProperty("role", QString::fromUtf8("muted"));
    m_noSession->setAlignment(Qt::AlignCenter);
    m_noSession->setMinimumHeight(48);
    sessionCol->addWidget(m_noSession);

    QLabel* noteTitle = new QLabel(QString::fromUtf8("中控台消息"), session);
    noteTitle->setProperty("role", QString::fromUtf8("small"));
    m_chat = new QPlainTextEdit(session);
    m_chat->setReadOnly(true);
    m_chat->setMaximumHeight(96);
    m_chat->setPlaceholderText(QString::fromUtf8("中控台发来的聊天会记在这里"));
    m_chatInput = new QLineEdit(session);
    m_chatInput->setPlaceholderText(QString::fromUtf8("发给中控台"));
    QPushButton* send = new QPushButton(QString::fromUtf8("发送"), session);
    send->setProperty("kind", QString::fromUtf8("primary"));
    send->setProperty("size", QString::fromUtf8("sm"));
    send->setCursor(Qt::PointingHandCursor);
    send->setFocusPolicy(Qt::NoFocus);
    QObject::connect(send, &QPushButton::clicked, this, &AgentWindow::sendChat);
    QObject::connect(m_chatInput, &QLineEdit::returnPressed, this, &AgentWindow::sendChat);
    QHBoxLayout* chatRow = new QHBoxLayout();
    chatRow->setSpacing(8);
    chatRow->addWidget(m_chatInput, 1);
    chatRow->addWidget(send);
    m_file = new QLabel(QString::fromUtf8("文件  空闲"), session);
    m_file->setProperty("role", QString::fromUtf8("small"));
    sessionCol->addWidget(noteTitle);
    sessionCol->addWidget(m_chat);
    sessionCol->addLayout(chatRow);
    sessionCol->addWidget(m_file);

    QFrame* share = new QFrame(this);
    share->setObjectName("card");
    QVBoxLayout* shareCol = new QVBoxLayout(share);
    shareCol->setContentsMargins(16, 14, 16, 14);
    shareCol->setSpacing(12);
    QLabel* shareTitle = new QLabel(QString::fromUtf8("共享范围"), share);
    shareTitle->setProperty("role", QString::fromUtf8("section"));
    shareCol->addWidget(shareTitle);

    m_desktop = new QRadioButton(QString::fromUtf8("整个桌面"), share);
    m_window = new QRadioButton(QString::fromUtf8("进程画面"), share);
    m_desktop->setChecked(true);
    QButtonGroup* kinds = new QButtonGroup(share);
    kinds->addButton(m_desktop);
    kinds->addButton(m_window);
    m_monitor = new QComboBox(share);
    m_monitor->setFixedWidth(150);
    m_monitor->addItem(QString::fromUtf8("显示器 1"));
    m_processBox = new QComboBox(share);
    m_processBox->setEditable(true);
    m_titleEdit = new QLineEdit(share);
    m_titleEdit->setPlaceholderText(QString::fromUtf8("留空则匹配该进程的主窗口"));

    QHBoxLayout* deskRow = new QHBoxLayout();
    deskRow->setSpacing(10);
    deskRow->addWidget(m_desktop);
    deskRow->addWidget(m_monitor);
    deskRow->addStretch(1);

    const char* keys[] = { "进程", "标题" };
    QWidget* values[] = { m_processBox, m_titleEdit };
    QVBoxLayout* fields = new QVBoxLayout();
    fields->setSpacing(12);
    fields->addLayout(deskRow);
    fields->addWidget(m_window);
    for (int i = 0; i < 2; ++i) {
        QLabel* key = new QLabel(QString::fromUtf8(keys[i]), share);
        key->setProperty("role", QString::fromUtf8("key"));
        key->setFixedWidth(52);
        QHBoxLayout* row = new QHBoxLayout();
        row->setContentsMargins(22, 0, 0, 0);
        row->setSpacing(10);
        row->addWidget(key);
        row->addWidget(values[i], 1);
        fields->addLayout(row);
    }

    m_previewInfo = new QLabel(QString::fromUtf8("送出预览"), share);
    m_previewInfo->setProperty("role", QString::fromUtf8("small"));
    m_preview = new QLabel(QString::fromUtf8("还没有送出画面"), share);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFixedSize(300, 169);
    m_preview->setStyleSheet("background:#15181D;border:1px solid #E8E8E8;border-radius:4px;color:#8C8C8C;");
    QVBoxLayout* prev = new QVBoxLayout();
    prev->setSpacing(8);
    prev->addWidget(m_previewInfo);
    prev->addWidget(m_preview);
    prev->addStretch(1);

    QHBoxLayout* shareRow = new QHBoxLayout();
    shareRow->setSpacing(16);
    shareRow->addLayout(fields, 1);
    shareRow->addLayout(prev);
    shareCol->addLayout(shareRow);
    shareCol->addStretch(1);

    QObject::connect(m_desktop, &QRadioButton::toggled, this, &AgentWindow::syncShare);
    QObject::connect(m_desktop, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            commitShare();
        }
    });
    QObject::connect(m_window, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            commitShare();
        }
    });
    QObject::connect(m_processBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_window->isChecked()) {
            commitShare();
        }
    });
    QObject::connect(m_processBox->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        if (m_window->isChecked()) {
            commitShare();
        }
    });
    QObject::connect(m_titleEdit, &QLineEdit::editingFinished, this, [this]() {
        if (m_window->isChecked()) {
            commitShare();
        }
    });
    syncShare();

    QHBoxLayout* top = new QHBoxLayout();
    top->setSpacing(12);
    top->addWidget(idCard);
    top->addWidget(session, 1);

    QVBoxLayout* main = new QVBoxLayout(this);
    main->setContentsMargins(14, 14, 14, 14);
    main->setSpacing(12);
    main->addLayout(top);
    main->addWidget(share, 1);
}

void AgentWindow::applyTheme()
{
    QFont font(QString::fromUtf8("Noto Sans CJK SC"));
    font.setPixelSize(13);
    if (!font.exactMatch()) {
        font.setFamily(QString::fromUtf8("Ubuntu"));
        font.setPixelSize(13);
    }
    QApplication::setFont(font);
    QApplication::setStyle(QStyleFactory::create(QString::fromUtf8("Fusion")));

    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Window, QColor("#F5F7FA"));
    pal.setColor(QPalette::Base, QColor("#FFFFFF"));
    pal.setColor(QPalette::AlternateBase, QColor("#FAFAFA"));
    pal.setColor(QPalette::Text, QColor("#262626"));
    pal.setColor(QPalette::WindowText, QColor("#262626"));
    pal.setColor(QPalette::ButtonText, QColor("#262626"));
    pal.setColor(QPalette::Button, QColor("#FFFFFF"));
    pal.setColor(QPalette::Highlight, QColor("#E6F4FF"));
    pal.setColor(QPalette::HighlightedText, QColor("#262626"));
    pal.setColor(QPalette::PlaceholderText, QColor("#BFBFBF"));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#BFBFBF"));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#BFBFBF"));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#BFBFBF"));
    QApplication::setPalette(pal);

    QImage arrow(12, 12, QImage::Format_ARGB32);
    arrow.fill(Qt::transparent);
    QPainter painter(&arrow);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#8C8C8C"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(2, 4), QPointF(6, 8));
    painter.drawLine(QPointF(6, 8), QPointF(10, 4));
    painter.end();
    const QString arrowPath = QDir::tempPath() + QString::fromUtf8("/remoteclient-down.png");
    arrow.save(arrowPath, "PNG");

    QString qss = QString::fromUtf8(R"(
QWidget { color:#262626; }
QLabel, QCheckBox, QRadioButton { background:transparent; }
QToolTip { background:#262626; color:#FFFFFF; border:none; padding:4px 8px; }
QFrame#card { background:#FFFFFF; border:1px solid #E8E8E8; border-radius:6px; }
QFrame#sessionRow { background:#FFFFFF; border:1px solid #F0F0F0; border-radius:6px; }
QPushButton { background:#FFFFFF; border:1px solid #D9D9D9; border-radius:4px; padding:0 12px; min-height:28px; color:#262626; }
QPushButton:hover { border-color:#4096FF; color:#4096FF; }
QPushButton:pressed { border-color:#0958D9; color:#0958D9; }
QPushButton:disabled { background:#F5F5F5; border-color:#D9D9D9; color:#BFBFBF; }
QPushButton[kind="primary"] { background:#1890FF; border-color:#1890FF; color:#FFFFFF; }
QPushButton[kind="primary"]:hover { background:#4096FF; border-color:#4096FF; color:#FFFFFF; }
QPushButton[kind="primary"]:pressed { background:#0958D9; border-color:#0958D9; }
QPushButton[kind="icon"] { padding:0; min-width:28px; max-width:28px; }
QPushButton[size="sm"] { min-height:24px; padding:0 10px; font-size:12px; }
QPushButton[kind="icon"][size="sm"] { min-width:24px; max-width:24px; padding:0; }
QLineEdit, QComboBox { background:#FFFFFF; border:1px solid #D9D9D9; border-radius:4px; padding:0 8px; min-height:28px; selection-background-color:#91CAFF; selection-color:#262626; }
QLineEdit:hover, QComboBox:hover { border-color:#4096FF; }
QLineEdit:focus, QComboBox:focus { border-color:#1890FF; }
QLineEdit:disabled, QComboBox:disabled { background:#F5F5F5; color:#BFBFBF; border-color:#D9D9D9; }
QComboBox::drop-down { border:none; width:24px; }
QComboBox::down-arrow { image:url(ARROW); width:12px; height:12px; }
QComboBox QAbstractItemView { background:#FFFFFF; border:1px solid #E8E8E8; outline:0; padding:4px; selection-background-color:#E6F4FF; selection-color:#1677FF; }
QPlainTextEdit { background:#FFFFFF; border:1px solid #D9D9D9; border-radius:4px; padding:4px 8px; selection-background-color:#91CAFF; selection-color:#262626; }
QPlainTextEdit:focus { border-color:#1890FF; }
QScrollBar:vertical { background:transparent; width:8px; margin:0; }
QScrollBar::handle:vertical { background:#D9D9D9; border-radius:4px; min-height:30px; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; }
QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }
QLabel[role="muted"] { color:#8C8C8C; }
QLabel[role="small"] { color:#8C8C8C; font-size:12px; }
QLabel[role="section"] { font-weight:600; font-size:14px; }
QLabel[role="key"] { color:#595959; }
QLabel[tone] { border-radius:3px; padding:1px 6px; font-size:11px; }
QLabel[tone="gray"] { background:#F0F0F0; color:#595959; }
QLabel[tone="blue"] { background:#E6F4FF; color:#1677FF; }
QLabel[tone="green"] { background:#F6FFED; color:#389E0D; }
)");
    qss.replace(QString::fromUtf8("ARROW"), arrowPath);
    qApp->setStyleSheet(qss);
}

void AgentWindow::setTone(QWidget* widget, const char* tone)
{
    if (!widget) {
        return;
    }
    widget->setProperty("tone", QString::fromLatin1(tone));
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QIcon AgentWindow::copyIcon() const
{
    // 和 Windows 被控端同一支线条图标：后面一张纸只露出左上，前面一张完整
    QPainterPath back;
    back.moveTo(5, 15);
    back.lineTo(4, 15);
    back.arcTo(QRectF(2, 11, 4, 4), -90, -90);
    back.lineTo(2, 4);
    back.arcTo(QRectF(2, 2, 4, 4), 180, -90);
    back.lineTo(13, 2);
    back.arcTo(QRectF(11, 2, 4, 4), 90, -90);
    back.lineTo(15, 5);
    QPainterPath front;
    front.addRoundedRect(QRectF(9, 9, 13, 13), 2, 2);

    QIcon icon;
    const qreal scales[] = { 1.0, 2.0 };
    for (int i = 0; i < 2; ++i) {
        const int side = static_cast<int>(14 * scales[i] + 0.5);
        QPixmap pix(side, side);
        pix.fill(Qt::transparent);
        pix.setDevicePixelRatio(scales[i]);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(14.0 / 24.0, 14.0 / 24.0);
        painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(back);
        painter.drawPath(front);
        painter.end();
        icon.addPixmap(pix);
    }
    return icon;
}

QIcon AgentWindow::refreshIcon() const
{
    QIcon icon;
    const qreal scales[] = { 1.0, 2.0 };
    for (int i = 0; i < 2; ++i) {
        const int side = static_cast<int>(14 * scales[i] + 0.5);
        QPixmap pix(side, side);
        pix.fill(Qt::transparent);
        pix.setDevicePixelRatio(scales[i]);
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(14.0 / 24.0, 14.0 / 24.0);
        painter.setPen(QPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(3, 3, 18, 18), 30 * 16, 230 * 16);
        painter.drawArc(QRectF(3, 3, 18, 18), (30 + 180) * 16, 230 * 16);
        painter.drawPolyline(QPolygonF() << QPointF(23, 4) << QPointF(23, 10) << QPointF(17, 10));
        painter.drawPolyline(QPolygonF() << QPointF(1, 20) << QPointF(1, 14) << QPointF(7, 14));
        painter.end();
        icon.addPixmap(pix);
    }
    return icon;
}

QPixmap AgentWindow::clockIcon() const
{
    const qreal dpr = 2.0;
    const int side = static_cast<int>(12 * dpr + 0.5);
    QPixmap pix(side, side);
    pix.fill(Qt::transparent);
    pix.setDevicePixelRatio(dpr);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(12.0 / 24.0, 12.0 / 24.0);
    painter.setPen(QPen(QColor(255, 255, 255, 217), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(2, 2, 20, 20));
    painter.drawPolyline(QPolygonF() << QPointF(12, 6) << QPointF(12, 12) << QPointF(16, 14));
    painter.end();
    return pix;
}

void AgentWindow::loadIcon()
{
    const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };
    QIcon icon;
    for (int i = 0; i < 7; ++i) {
        icon.addFile(QString::fromUtf8(":/icon/controlled-%1.png").arg(sizes[i]));
    }
    setWindowIcon(icon);
    QApplication::setWindowIcon(icon);
    QApplication::setDesktopFileName(QString::fromUtf8("RemoteClient"));
}

void AgentWindow::copyId()
{
    if (!m_idText.isEmpty()) {
        QApplication::clipboard()->setText(m_idText);
    }
}

void AgentWindow::copyFingerprint()
{
    if (!m_fpText.isEmpty()) {
        QApplication::clipboard()->setText(m_fpText);
    }
}

void AgentWindow::applyStatus(const QString& deviceId, const QString& fingerprint, const QString& desk, const QString& state)
{
    m_idText = deviceId;
    m_fpText = fingerprint;
    m_id->setText(deviceId.isEmpty() ? QString::fromUtf8("—") : deviceId);
    m_fingerprint->setText(fingerprint.isEmpty() ? QString::fromUtf8("-") : (fingerprint.left(16) + QString::fromUtf8("…")));
    m_fingerprint->setToolTip(fingerprint);
    m_state->setText(state);
    m_desk->setText(desk.isEmpty() ? QString::fromUtf8("桌面 —") : desk);
    const bool on = state.contains(QString::fromUtf8("已连接"));
    const bool operating = state.contains(QString::fromUtf8("操作"));
    m_count->setText(on ? QString::fromUtf8("1") : QString::fromUtf8("0"));
    setTone(m_count, on ? "blue" : "gray");
    m_dot->setStyleSheet(on ? "background:#52C41A;border-radius:4px;" : "background:#BFBFBF;border-radius:4px;");
    m_sessionRow->setVisible(on);
    m_sessionMeta->setText(state);
    m_sessionTag->setText(operating ? QString::fromUtf8("操作") : QString::fromUtf8("观看"));
    setTone(m_sessionTag, operating ? "blue" : "gray");
    m_noSession->setVisible(!on);
    if (!deviceId.isEmpty()) {
        setWindowTitle(QString::fromUtf8("被控端 — %1").arg(deviceId));
    }
}

void AgentWindow::applyPreview(const QByteArray& jpeg, int w, int h)
{
    if (jpeg.isEmpty() || w <= 0 || h <= 0) {
        m_frame = QPixmap();
        m_preview->setPixmap(QPixmap());
        m_preview->setText(QString::fromUtf8("还没有送出画面"));
        m_previewInfo->setText(QString::fromUtf8("送出预览"));
        return;
    }
    QPixmap pix;
    if (!pix.loadFromData(jpeg, "JPEG") || pix.isNull()) {
        m_frame = QPixmap();
        m_preview->setPixmap(QPixmap());
        m_preview->setText(QString::fromUtf8("这一帧解不开"));
        m_previewInfo->setText(QString::fromUtf8("送出预览 · %1×%2 · JPEG").arg(w).arg(h));
        return;
    }
    m_frame = pix;
    m_preview->setText(QString());
    m_previewInfo->setText(QString::fromUtf8("送出预览 · %1×%2 · JPEG").arg(w).arg(h));
    fitPreview();
}

void AgentWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitPreview();
}

void AgentWindow::fitPreview()
{
    if (!m_preview || m_frame.isNull()) {
        return;
    }
    const QSize box = m_preview->size() - QSize(2, 2);
    if (box.width() < 2 || box.height() < 2) {
        return;
    }
    m_preview->setPixmap(m_frame.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void AgentWindow::syncShare()
{
    const bool desktop = m_desktop->isChecked();
    m_desktopShare = desktop;
    m_monitor->setEnabled(desktop);
    m_processBox->setEnabled(!desktop);
    m_titleEdit->setEnabled(!desktop);
}

void AgentWindow::commitShare()
{
    if (m_desktop->isChecked()) {
        m_desktopShare = true;
        m_process.clear();
        m_title.clear();
        emit shareChosen(true, QString(), QString());
        return;
    }
    m_desktopShare = false;
    m_process = m_processBox->currentText().trimmed();
    m_title = m_titleEdit->text().trimmed();
    emit shareChosen(false, m_process, m_title);
}

void AgentWindow::selectProcess()
{
    m_processBox->blockSignals(true);
    int pick = m_processBox->findText(m_process);
    if (pick < 0 && !m_process.isEmpty()) {
        m_processBox->addItem(m_process);
        pick = m_processBox->count() - 1;
    }
    if (pick >= 0) {
        m_processBox->setCurrentIndex(pick);
    }
    m_processBox->blockSignals(false);
    m_titleEdit->blockSignals(true);
    m_titleEdit->setText(m_title);
    m_titleEdit->blockSignals(false);
}

void AgentWindow::applyWindows(const QStringList& rows)
{
    const QString current = m_processBox->currentText();
    m_processBox->blockSignals(true);
    m_processBox->clear();
    for (int i = 0; i < rows.size(); ++i) {
        const QString row = rows.at(i);
        const int tab = row.indexOf(QLatin1Char('\t'));
        const QString process = tab < 0 ? row : row.left(tab);
        if (process.isEmpty() || m_processBox->findText(process) >= 0) {
            continue;
        }
        m_processBox->addItem(process);
    }
    m_processBox->blockSignals(false);
    if (!m_process.isEmpty()) {
        selectProcess();
    } else if (!current.isEmpty()) {
        const int pick = m_processBox->findText(current);
        if (pick >= 0) {
            m_processBox->blockSignals(true);
            m_processBox->setCurrentIndex(pick);
            m_processBox->blockSignals(false);
        }
    }
}

void AgentWindow::applyShare(bool desktop, const QString& process, const QString& title)
{
    m_desktopShare = desktop;
    m_process = process;
    m_title = title;
    m_desktop->blockSignals(true);
    m_window->blockSignals(true);
    m_desktop->setChecked(desktop);
    m_window->setChecked(!desktop);
    m_desktop->blockSignals(false);
    m_window->blockSignals(false);
    syncShare();
    selectProcess();
}

void AgentWindow::showNotice(const QString& name, const QString& text)
{
    QDialog* box = new QDialog(0, Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowTitle(QString::fromUtf8("中控台提示"));
    box->setMinimumWidth(360);
    QVBoxLayout* col = new QVBoxLayout(box);
    QLabel* who = new QLabel(name.isEmpty() ? QString::fromUtf8("中控台") : name, box);
    who->setStyleSheet("font-weight:600;");
    QLabel* body = new QLabel(text, box);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QPushButton* ok = new QPushButton(QString::fromUtf8("知道了"), box);
    ok->setProperty("kind", QString::fromUtf8("primary"));
    ok->setProperty("size", QString::fromUtf8("sm"));
    ok->setCursor(Qt::PointingHandCursor);
    ok->setFocusPolicy(Qt::NoFocus);
    QObject::connect(ok, &QPushButton::clicked, box, &QDialog::close);
    col->addWidget(who);
    col->addWidget(body);
    col->addWidget(ok, 0, Qt::AlignRight);
    box->show();
    box->raise();
    box->activateWindow();
}

void AgentWindow::applyChat(const QString& name, const QString& text, int kind)
{
    if (!m_chat) {
        return;
    }
    const QString who = name.isEmpty() ? QString::fromUtf8("中控台") : name;
    m_chat->appendPlainText(QTime::currentTime().toString(QString::fromUtf8("HH:mm"))
        + (kind == 1 ? QString::fromUtf8("  弹窗  ") : QString::fromUtf8("  "))
        + who + QString::fromUtf8("：") + text);
    if (kind == 1) {
        showNotice(who, text);
    }
}

void AgentWindow::sendChat()
{
    if (!m_chatInput) {
        return;
    }
    const QString text = m_chatInput->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    m_chatInput->clear();
    emit chatSend(text);
}

void AgentWindow::applyCode(const QString& code)
{
    if (!m_code) {
        return;
    }
    QString spaced;
    for (int i = 0; i < code.size(); ++i) {
        if (i > 0 && i % 3 == 0) {
            spaced += QLatin1Char(' ');
        }
        spaced += code.at(i);
    }
    m_code->setText(spaced.isEmpty() ? QString::fromUtf8("— — —") : spaced);
}

void AgentWindow::applyFile(const QString& note)
{
    if (m_file) {
        m_file->setText(QString::fromUtf8("文件  ") + note);
    }
}

}
