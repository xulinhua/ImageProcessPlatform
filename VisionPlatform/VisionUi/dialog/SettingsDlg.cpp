/**
 * @file SettingsDlg.cpp
 * @brief SettingsDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "SettingsDlg.h"
#include "AppPaths.h"
#include "AppSettings.h"
#include "LogDlg.h"
#include "Perm.h"
#include "QtConv.h"
#include "Session.h"
#include "Theme.h"
#include "Translate.h"
#include "UiText.h"
#include "VisionCfgMgr.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QUrl>
#include <QVBoxLayout>

namespace Vision {

namespace {

// 只读路径行：编辑框 + 打开按钮
QWidget* makePathRow(QWidget* parent, QLineEdit** editOut, const QString& path,
                     QObject* receiver, const char* openSlot)
{
    QWidget* row = new QWidget(parent);
    QHBoxLayout* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    QLineEdit* edit = new QLineEdit(path, row);
    edit->setReadOnly(true);
    QPushButton* btn = new QPushButton(T("打开"), row);
    bindTrKey(btn, "打开");
    QObject::connect(btn, SIGNAL(clicked()), receiver, openSlot);
    lay->addWidget(edit, 1);
    lay->addWidget(btn);
    if (editOut) {
        *editOut = edit;
    }
    return row;
}

} // namespace

// 设置中心：左导航 + 五页
SettingsDlg::SettingsDlg(Session* session, QWidget* parent)
    : QDialog(parent)
    , m_session(session)
    , m_titleLabel(nullptr)
    , m_uiHint(nullptr)
    , m_closeHint(nullptr)
    , m_pathHint(nullptr)
    , m_runHint(nullptr)
    , m_popupBox(nullptr)
    , m_popupJobOk(nullptr)
    , m_popupJobNg(nullptr)
    , m_popupJobEx(nullptr)
    , m_generalBox(nullptr)
    , m_btnOk(nullptr)
    , m_btnCancel(nullptr)
    , m_btnApply(nullptr)
    , m_btnOpenLogsDlg(nullptr)
    , m_btnOpenLogsFolder(nullptr)
    , m_canEdit(session && session->has(PermEditAppSettings))
{
    setWindowTitle(T("设置"));
    setModal(true);
    resize(860, 560);
    enableDialogResize(this);

    m_nav = new QListWidget(this);
    m_nav->setFixedWidth(180);
    m_nav->setObjectName(QLatin1String("settingsNav"));
    m_nav->addItem(T("常规"));
    m_nav->addItem(T("界面外观"));
    m_nav->addItem(T("路径与存储"));
    m_nav->addItem(T("运行与日志"));
    m_nav->setCurrentRow(0);
    connect(m_nav, SIGNAL(currentRowChanged(int)), this, SLOT(onCategoryChanged(int)));

    // —— 常规 ——
    m_autoOp = new QCheckBox(T("开机自动登录操作员（仅 operator，直接进运行态）"), this);
    m_startupMode = new QComboBox(this);
    m_startupMode->addItem(QString::fromUtf8(startupAppModeTitle(StartupModeDesign).c_str()),
                           static_cast<int>(StartupModeDesign));
    m_startupMode->addItem(QString::fromUtf8(startupAppModeTitle(StartupModeRuntime).c_str()),
                           static_cast<int>(StartupModeRuntime));
    m_closeAction = new QComboBox(this);
    m_closeAction->addItem(QString::fromUtf8(closeMainWindowActionTitle(CloseActionMinimizeTray).c_str()),
                           static_cast<int>(CloseActionMinimizeTray));
    m_closeAction->hide();
    m_closeHint = new QLabel(T("关闭主窗口后回到托盘，工站服务继续运行。"), this);
    m_closeHint->setObjectName(QLatin1String("hint"));
    m_closeHint->setWordWrap(true);

    QWidget* pageGeneral = new QWidget(this);
    QVBoxLayout* gLay = new QVBoxLayout(pageGeneral);
    m_generalBox = new QGroupBox(T("启动与关闭"), pageGeneral);
    QFormLayout* gForm = new QFormLayout(m_generalBox);
    gForm->addRow(QString(), m_autoOp);
    QLabel* lab_2040619317 = new QLabel(T("启动后默认进入"), this); bindTrKey(lab_2040619317, "启动后默认进入"); gForm->addRow(lab_2040619317, m_startupMode);
    gForm->addRow(m_closeHint);
    m_permScope = new QComboBox(pageGeneral);
    m_permScope->addItem(T("按页面或工具统一"), PermScopeUnified);
    m_permScope->addItem(T("按流程实例分开"), PermScopeInstance);
    QLabel* labScope = new QLabel(T("权限管控"), pageGeneral);
    bindTrKey(labScope, "权限管控");
    gForm->addRow(labScope, m_permScope);
    QLabel* scopeHint = new QLabel(T("只有最高权限能改。权限记在本机 data，不写进方案。整个 data 一起拷走后可以继续用。"), pageGeneral);
    scopeHint->setObjectName(QLatin1String("hint"));
    scopeHint->setWordWrap(true);
    gForm->addRow(scopeHint);
    gLay->addWidget(m_generalBox);
    gLay->addStretch();

    // —— 界面外观 ——
    m_theme = new QComboBox(this);
    for (int i = 0; i < themeCount(); ++i) {
        const char* id = themeIdAt(i);
        m_theme->addItem(QString::fromUtf8(themeTitle(id).c_str()), QString::fromLatin1(id));
    }
    m_lang = new QComboBox(this);
    for (int i = 0; i < Translate::languageCount(); ++i) {
        const QString title = QString::fromUtf8(Translate::languageNativeNameAt(i).c_str());
        m_lang->addItem(title, QString::fromLatin1(Translate::languageKeyAt(i).c_str()));
    }
    m_uiHint = new QLabel(T("主界面立即切换。已打开的窗口关闭后重新打开，即为新语言。"), this);
    m_uiHint->setObjectName(QLatin1String("hint"));
    m_uiHint->setWordWrap(true);
    QWidget* pageUi = new QWidget(this);
    QFormLayout* uiForm = new QFormLayout(pageUi);
    QLabel* lab_440008882 = new QLabel(T("主题档"), this); bindTrKey(lab_440008882, "主题档"); uiForm->addRow(lab_440008882, m_theme);
    QLabel* lab_1576360391 = new QLabel(T("界面语言"), this); bindTrKey(lab_1576360391, "界面语言"); uiForm->addRow(lab_1576360391, m_lang);
    uiForm->addRow(m_uiHint);

    // —— 路径与存储 ——
    QWidget* pagePath = new QWidget(this);
    QFormLayout* pForm = new QFormLayout(pagePath);
    QLabel* labp1069909073 = new QLabel(T("数据目录"), this); bindTrKey(labp1069909073, "数据目录"); pForm->addRow(labp1069909073, makePathRow(pagePath, &m_dataDir, qs(AppPaths::dataDir()),
                                              this, SLOT(onOpenDataDir())));
    QLabel* labp_auth = new QLabel(T("权限库"), this); bindTrKey(labp_auth, "权限库"); pForm->addRow(labp_auth, makePathRow(pagePath, &m_dbAuth, qs(AppPaths::authFile()),
                                            this, SLOT(onOpenDataDir())));
    QLabel* labp_log = new QLabel(T("日志库"), this); bindTrKey(labp_log, "日志库"); pForm->addRow(labp_log, makePathRow(pagePath, &m_dbLog, qs(AppPaths::logFile()),
                                            this, SLOT(onOpenDataDir())));
    QLabel* labp_prod = new QLabel(T("生产库"), this); bindTrKey(labp_prod, "生产库"); pForm->addRow(labp_prod, makePathRow(pagePath, &m_dbProd, qs(AppPaths::prodFile()),
                                            this, SLOT(onOpenDataDir())));
    QLabel* labp_verify = new QLabel(T("验证库"), this); bindTrKey(labp_verify, "验证库"); pForm->addRow(labp_verify, makePathRow(pagePath, &m_dbVerify, qs(AppPaths::verifyFile()),
                                            this, SLOT(onOpenDataDir())));
    QLabel* labp_calib = new QLabel(T("标定库"), this); bindTrKey(labp_calib, "标定库"); pForm->addRow(labp_calib, makePathRow(pagePath, &m_dbCalib, qs(AppPaths::calibFile()),
                                            this, SLOT(onOpenDataDir())));
    QLabel* labp_sols = new QLabel(T("方案目录"), this); bindTrKey(labp_sols, "方案目录"); pForm->addRow(labp_sols, makePathRow(pagePath, &m_solsDir, qs(AppPaths::solutionsDir()),
                                              this, SLOT(onOpenSolutionsDir())));
    m_lastSol = new QLineEdit(this);
    m_lastSol->setReadOnly(true);
    QLabel* lab_1172506318 = new QLabel(T("上次方案"), this); bindTrKey(lab_1172506318, "上次方案"); pForm->addRow(lab_1172506318, m_lastSol);
    m_pathHint = new QLabel(T("权限、日志、生产、验证、标定分五个库。数据根目录由安装约定决定，本页仅查看与打开，不支持随意改路径。"), pagePath);
    m_pathHint->setObjectName(QLatin1String("hint"));
    m_pathHint->setWordWrap(true);
    pForm->addRow(m_pathHint);

    // —— 运行与日志 ——
    QWidget* pageRun = new QWidget(this);
    QVBoxLayout* rLay = new QVBoxLayout(pageRun);
    m_runHint = new QLabel(
        T("产量清零、运行画面布局属于方案/运行态，请在运行页或「视图 → 画面配置」中修改。"
          "流程试跑结果弹窗在此配置；默认均不弹窗。"), pageRun);
    m_runHint->setObjectName(QLatin1String("hint"));
    m_runHint->setWordWrap(true);
    m_popupBox = new QGroupBox(T("流程试跑弹窗"), pageRun);
    QVBoxLayout* popupLay = new QVBoxLayout(m_popupBox);
    m_popupJobOk = new QCheckBox(T("结果 OK 时弹窗"), m_popupBox);
    m_popupJobNg = new QCheckBox(T("结果 NG 时弹窗"), m_popupBox);
    m_popupJobEx = new QCheckBox(T("运行异常时弹窗"), m_popupBox);
    popupLay->addWidget(m_popupJobOk);
    popupLay->addWidget(m_popupJobNg);
    popupLay->addWidget(m_popupJobEx);
    m_btnOpenLogsDlg = new QPushButton(T("打开日志管控…"), pageRun);
    setButtonRole(m_btnOpenLogsDlg, "primary");
    connect(m_btnOpenLogsDlg, SIGNAL(clicked()), this, SLOT(onOpenLogDlg()));
    m_btnOpenLogsFolder = new QPushButton(T("打开数据目录"), pageRun);
    connect(m_btnOpenLogsFolder, SIGNAL(clicked()), this, SLOT(onOpenDataDir()));
    rLay->addWidget(m_runHint);
    rLay->addWidget(m_popupBox);
    rLay->addWidget(m_btnOpenLogsDlg, 0, Qt::AlignLeft);
    rLay->addWidget(m_btnOpenLogsFolder, 0, Qt::AlignLeft);
    rLay->addStretch();

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(pageGeneral);
    m_stack->addWidget(pageUi);
    m_stack->addWidget(pagePath);
    m_stack->addWidget(pageRun);

    QHBoxLayout* body = new QHBoxLayout();
    body->setSpacing(12);
    body->addWidget(m_nav);
    body->addWidget(m_stack, 1);

    QDialogButtonBox* box = new QDialogButtonBox(this);
    m_btnOk = box->addButton(T("确定"), QDialogButtonBox::AcceptRole);
    m_btnCancel = box->addButton(T("取消"), QDialogButtonBox::RejectRole);
    m_btnApply = box->addButton(T("应用"), QDialogButtonBox::ApplyRole);
    setButtonRole(m_btnOk, "primary");
    connect(m_btnOk, SIGNAL(clicked()), this, SLOT(onOk()));
    connect(m_btnCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(m_btnApply, SIGNAL(clicked()), this, SLOT(onApply()));

    QVBoxLayout* root = new QVBoxLayout(this);
    m_titleLabel = prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->addLayout(body, 1);
    lay->addWidget(box);

    loadFromSettings();

    m_autoOp->setEnabled(m_canEdit);
    m_startupMode->setEnabled(m_canEdit);
    m_closeAction->setEnabled(false);
    m_theme->setEnabled(m_canEdit);
    m_lang->setEnabled(m_canEdit);
    m_permScope->setEnabled(m_canEdit && m_session && m_session->role() == RoleFactory);
    if (m_popupJobOk) {
        m_popupJobOk->setEnabled(m_canEdit);
    }
    if (m_popupJobNg) {
        m_popupJobNg->setEnabled(m_canEdit);
    }
    if (m_popupJobEx) {
        m_popupJobEx->setEnabled(m_canEdit);
    }
    m_btnOk->setEnabled(m_canEdit);
    m_btnApply->setEnabled(m_canEdit);
}

// 左侧分类切换右侧页
void SettingsDlg::onCategoryChanged(int row)
{
    if (row >= 0 && row < m_stack->count()) {
        m_stack->setCurrentIndex(row);
    }
}

// 用本机设置填充控件
void SettingsDlg::loadFromSettings()
{
    const AppSettings& cur = m_session->settings();
    m_autoOp->setChecked(cur.autoLoginOperator);
    m_startupMode->setCurrentIndex(m_startupMode->findData(static_cast<int>(cur.startupMode)));
    if (m_startupMode->currentIndex() < 0) {
        m_startupMode->setCurrentIndex(0);
    }
    m_closeAction->setCurrentIndex(0);
    for (int i = 0; i < m_theme->count(); ++i) {
        if (m_theme->itemData(i).toString() == QString::fromLatin1(cur.themeId.c_str())) {
            m_theme->setCurrentIndex(i);
            break;
        }
    }
    for (int i = 0; i < m_lang->count(); ++i) {
        if (m_lang->itemData(i).toString() == QString::fromLatin1(cur.language.c_str())) {
            m_lang->setCurrentIndex(i);
            break;
        }
    }
    m_dataDir->setText(qs(AppPaths::dataDir()));
    m_dbAuth->setText(qs(AppPaths::authFile()));
    m_dbLog->setText(qs(AppPaths::logFile()));
    m_dbProd->setText(qs(AppPaths::prodFile()));
    m_dbVerify->setText(qs(AppPaths::verifyFile()));
    m_dbCalib->setText(qs(AppPaths::calibFile()));
    m_solsDir->setText(qs(AppPaths::solutionsDir()));
    m_lastSol->setText(cur.lastSolutionPath.empty()
                           ? T("（尚无）")
                           : qs(cur.lastSolutionPath));
    if (m_popupJobOk) {
        m_popupJobOk->setChecked(cur.popupJobOk);
    }
    if (m_popupJobNg) {
        m_popupJobNg->setChecked(cur.popupJobNg);
    }
    if (m_popupJobEx) {
        m_popupJobEx->setChecked(cur.popupJobException);
    }
    const int scopeIndex = m_permScope->findData(cur.permScope);
    m_permScope->setCurrentIndex(scopeIndex >= 0 ? scopeIndex : 0);
}

// 写盘并应用主题与语言
bool SettingsDlg::applySettings(bool interactivePrompt)
{
    Q_UNUSED(interactivePrompt);
    if (!m_canEdit) {
        return true;
    }
    AppSettings& s = m_session->settings();
    s.autoLoginOperator = m_autoOp->isChecked();
    s.startupMode = startupAppModeFromInt(m_startupMode->currentData().toInt());
    s.closeAction = CloseActionMinimizeTray;
    s.themeId = ss(m_theme->currentData().toString());
    s.language = ss(m_lang->currentData().toString());
    s.popupJobOk = m_popupJobOk && m_popupJobOk->isChecked();
    s.popupJobNg = m_popupJobNg && m_popupJobNg->isChecked();
    s.popupJobException = m_popupJobEx && m_popupJobEx->isChecked();
    if (s.themeId.empty()) {
        s.themeId = "industrial";
    }
    if (s.language.empty()) {
        s.language = "zh-CN";
    }
    if (m_session && m_session->role() == RoleFactory) {
        s.permScope = m_permScope->currentData().toInt();
        if (s.permScope != PermScopeInstance) {
            s.permScope = PermScopeUnified;
        }
    }
    if (!VisionCfgMgr::saveSettings(s)) {
        QMessageBox::warning(this, T("设置"), T("保存失败。"));
        return false;
    }
    applyAppTheme(s.themeId);
    Translate::setLanguageKey(s.language);
    m_session->notifyUiLocaleChanged();
    retranslateUi();
    return true;
}

// 应用但不关窗
void SettingsDlg::onApply()
{
    applySettings(true);
}

// 应用成功后关闭
void SettingsDlg::onOk()
{
    if (!applySettings(true)) {
        return;
    }
    accept();
}

// 用资源管理器打开路径
void SettingsDlg::openPath(const QString& path)
{
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

// 打开数据目录
void SettingsDlg::onOpenDataDir()
{
    openPath(qs(AppPaths::dataDir()));
}

// 打开方案目录
void SettingsDlg::onOpenSolutionsDir()
{
    openPath(qs(AppPaths::solutionsDir()));
}

// 弹出日志管控
void SettingsDlg::onOpenLogDlg()
{
    LogDlg dlg(m_session, this);
    dlg.exec();
}

// 语言热切换：刷新设置中心全部静态文案
void SettingsDlg::retranslateUi()
{
    setWindowTitle(T("设置"));
    if (m_titleLabel) {
        m_titleLabel->setText(T("设置"));
    }
    if (m_nav->count() >= 4) {
        m_nav->item(0)->setText(T("常规"));
        m_nav->item(1)->setText(T("界面外观"));
        m_nav->item(2)->setText(T("路径与存储"));
        m_nav->item(3)->setText(T("运行与日志"));
    }
    m_autoOp->setText(T("开机自动登录操作员（仅 operator，直接进运行态）"));
    if (m_generalBox) {
        m_generalBox->setTitle(T("启动与关闭"));
    }
    if (m_closeHint) {
        m_closeHint->setText(T("「最小化到托盘」将在后续版本提供，当前仅可选择退出程序。"));
    }
    if (m_uiHint) {
        m_uiHint->setText(T("主界面立即切换。已打开的窗口关闭后重新打开，即为新语言。"));
    }
    if (m_pathHint) {
        m_pathHint->setText(T("权限、日志、生产、验证、标定分五个库。数据根目录由安装约定决定，本页仅查看与打开，不支持随意改路径。"));
    }
    if (m_runHint) {
        m_runHint->setText(T("产量清零、运行画面布局属于方案/运行态，请在运行页或「视图 → 画面配置」中修改。流程试跑结果弹窗在此配置；默认均不弹窗。"));
    }
    if (m_popupBox) {
        m_popupBox->setTitle(T("流程试跑弹窗"));
    }
    if (m_popupJobOk) {
        m_popupJobOk->setText(T("结果 OK 时弹窗"));
    }
    if (m_popupJobNg) {
        m_popupJobNg->setText(T("结果 NG 时弹窗"));
    }
    if (m_popupJobEx) {
        m_popupJobEx->setText(T("运行异常时弹窗"));
    }
    if (m_btnOpenLogsDlg) {
        m_btnOpenLogsDlg->setText(T("打开日志管控…"));
    }
    if (m_btnOpenLogsFolder) {
        m_btnOpenLogsFolder->setText(T("打开数据目录"));
    }
    if (m_btnOk) {
        m_btnOk->setText(T("确定"));
    }
    if (m_btnCancel) {
        m_btnCancel->setText(T("取消"));
    }
    if (m_btnApply) {
        m_btnApply->setText(T("应用"));
    }
    // 主题/启动/关闭下拉项标题随语言刷新（保留 itemData）
    for (int i = 0; i < m_theme->count(); ++i) {
        const QString id = m_theme->itemData(i).toString();
        m_theme->setItemText(i, QString::fromUtf8(themeTitle(ss(id).c_str()).c_str()));
    }
    for (int i = 0; i < m_startupMode->count(); ++i) {
        const int v = m_startupMode->itemData(i).toInt();
        m_startupMode->setItemText(i, QString::fromUtf8(startupAppModeTitle(startupAppModeFromInt(v)).c_str()));
    }
    for (int i = 0; i < m_closeAction->count(); ++i) {
        const int v = m_closeAction->itemData(i).toInt();
        m_closeAction->setItemText(i, QString::fromUtf8(closeMainWindowActionTitle(closeMainWindowActionFromInt(v)).c_str()));
    }
    for (int i = 0; i < m_lang->count(); ++i) {
        m_lang->setItemText(i, QString::fromUtf8(Translate::languageNativeNameAt(i).c_str()));
    }
    retranslateBound(this);
    loadFromSettings();
}

}


#include "moc_SettingsDlg.cpp"
