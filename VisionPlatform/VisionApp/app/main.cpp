/**
 * @file main.cpp
 * @brief main实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "MainWindow.h"
#include "CrashDump.h"
#include "LicenseGate.h"
#include "VisionLicense.h"
#include "ConsoleHost.h"
#include "TrayApp.h"
#include "SharedFrames.h"
#include "NodeSettings.h"
#include "AppPaths.h"
#include "Translate.h"
#include "QtConv.h"
#include "PluginHost.h"
#include "LogMgr.h"
#include "LoginDlg.h"
#include "ToolEditorRegistry.h"
#include "Session.h"
#include "VisionCfgMgr.h"
#include <HsCommData/Graphic.h>
#include "Theme.h"
#include <QApplication>
#include <QMetaObject>
#include <QEvent>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QMessageBox>
#include <QObject>
#include <QStringList>
#include <Windows.h>

namespace {

// 把 exe 里的图标画进位图。标题栏用小尺寸，任务栏用大尺寸
QPixmap pixmapFromIcon(HICON icon, int side)
{
    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = side;
    info.bmiHeader.biHeight = -side;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = 0;
    HDC screen = GetDC(0);
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, 0, 0);
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, dib);
    DrawIconEx(mem, 0, 0, icon, side, side, 0, 0, DI_NORMAL);
    const QImage image(static_cast<const uchar*>(bits), side, side, side * 4,
        QImage::Format_ARGB32_Premultiplied);
    const QPixmap pix = QPixmap::fromImage(image.copy());
    SelectObject(mem, old);
    DeleteDC(mem);
    DeleteObject(dib);
    ReleaseDC(0, screen);
    return pix;
}

// 加载应用图标：先用 exe 资源（标题栏和任务栏），再试旁边的文件
QIcon loadAppIcon()
{
    QIcon embedded;
    const int sizes[] = { 16, 24, 32, 48, 64, 256 };
    for (int i = 0; i < 6; ++i) {
        const int side = sizes[i];
        HICON handle = reinterpret_cast<HICON>(LoadImageW(
            GetModuleHandleW(0), MAKEINTRESOURCEW(1), IMAGE_ICON, side, side, LR_DEFAULTCOLOR));
        if (!handle) {
            continue;
        }
        embedded.addPixmap(pixmapFromIcon(handle, side));
        DestroyIcon(handle);
    }
    if (!embedded.isNull() && !embedded.availableSizes().isEmpty()) {
        return embedded;
    }
    const QStringList candidates = QStringList()
        << (QApplication::applicationDirPath() + QLatin1String("/icons/app/app.png"))
        << (QApplication::applicationDirPath() + QLatin1String("/icons/app/app.ico"))
        << QLatin1String("icons/app/app.png")
        << QLatin1String("icons/app/app.ico");
    for (int i = 0; i < candidates.size(); ++i) {
        const QIcon ic(candidates.at(i));
        if (!ic.isNull() && !ic.availableSizes().isEmpty()) {
            return ic;
        }
    }
    return QIcon();
}

// 所有 QDialog 默认可手动拉伸
class DialogResizeFilter : public QObject {
public:
    explicit DialogResizeFilter(QObject* parent)
        : QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject* obj, QEvent* ev)
    {
        if (ev->type() == QEvent::Polish) {
            QDialog* dlg = qobject_cast<QDialog*>(obj);
            if (dlg) {
                Vision::enableDialogResize(dlg);
            }
        }
        return QObject::eventFilter(obj, ev);
    }
};

// 判断字符串是否为合法 UTF-8
static bool utf8Ok(const std::string& s)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        const unsigned char c = p[i];
        size_t need = 1;
        if (c <= 0x7F) {
            need = 1;
        } else if ((c & 0xE0) == 0xC0 && c >= 0xC2) {
            need = 2;
        } else if ((c & 0xF0) == 0xE0) {
            need = 3;
        } else if ((c & 0xF8) == 0xF0 && c <= 0xF4) {
            need = 4;
        } else {
            return false;
        }
        if (i + need > n) {
            return false;
        }
        for (size_t k = 1; k < need; ++k) {
            if ((p[i + k] & 0xC0) != 0x80) {
                return false;
            }
        }
        i += need;
    }
    return true;
}

// 单机控制台：把 start/stop 排队到界面线程
void consoleStartStation(void* window)
{
    QMetaObject::invokeMethod(static_cast<Vision::MainWindow*>(window), "startStation", Qt::QueuedConnection);
}

void consoleStopStation(void* window)
{
    QMetaObject::invokeMethod(static_cast<Vision::MainWindow*>(window), "stopStation", Qt::QueuedConnection);
}

// 库里旧的显示名若不是 UTF-8，改成角色名。不从 users.json 导入
static bool repairDisplayNames(Vision::AuthMgr* users)
{
    if (!users) {
        return false;
    }
    bool changed = false;
    for (int i = 0; i < users->count(); ++i) {
        Vision::UserAccount* u = users->findById(users->at(i).id);
        if (!u) {
            continue;
        }
        if (u->displayName.empty() || !utf8Ok(u->displayName)) {
            u->displayName = Vision::roleTitle(u->role);
            changed = true;
        }
    }
    return changed;
}

} // namespace

// 单实例、注册工具、加载用户设置，再进主窗
int main(int argc, char* argv[])
{
    Vision::CrashDump::install();
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VisionPlatform"));
    app.setOrganizationName(QStringLiteral("VisionPlatform"));
    const QIcon appIcon = loadAppIcon();
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\VisionPlatform.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    Vision::AppPaths::setExeDir(Vision::ss(QApplication::applicationDirPath()));
    Vision::License license;
    if (!Vision::promptLicense(&license, license.open())) {
        return 0;
    }
    HsCtrl::registerGraphicMetaTypes();
    // 先加载插件（Hal/OpenCV/PCL/Open3D 等会改堆或装自己的分配器），再启日志写库线程，
    // 避免 VisionLog 后台 SQLite malloc 与插件 DllMain/静态初始化并发导致 Debug CRT 断在 sqlite3MemMalloc。
    {
        Vision::PluginHost host;
        Vision::loadVisionPlugins(&host);
    }
    Vision::LogMgr::startup();
    Vision::registerToolEditors();

    Vision::Session session;
    std::string loadErr;
    if (!session.users().load(&loadErr)
        || session.users().count() == 0) {
        session.users().ensureDefaults();
        if (!session.users().save(&loadErr)) {
            QMessageBox::warning(0,
                                 QString::fromUtf8(Vision::Translate::tr("用户库").c_str()),
                                 QString::fromUtf8(Vision::Translate::tr("无法读取本机设置").c_str())
                                 + QLatin1Char('\n') + QString::fromUtf8(loadErr.c_str()));
        }
    } else if (repairDisplayNames(&session.users())) {
        session.users().save(0);
    }
    const Vision::FileLoadStatus sst = Vision::VisionCfgMgr::loadSettings(&session.settings(), &loadErr);
    if (sst == Vision::FileLoadCorrupt) {
        QMessageBox::warning(0,
                             QString::fromUtf8(Vision::Translate::tr("本机设置").c_str()),
                             QString::fromUtf8(Vision::Translate::tr("本机设置损坏，已使用默认设置。").c_str())
                             + QLatin1Char('\n') + QString::fromUtf8(loadErr.c_str()));
    }
    Vision::Translate::setLanguageKey(session.settings().language);
    Vision::applyAppTheme(session.settings().themeId);
    app.installEventFilter(new DialogResizeFilter(&app));

    // 开发阶段：不弹登录，管理员空口令，恢复出厂权限
#ifdef _DEBUG
    bool usersChanged = session.users().ensureBuiltinEmptyPasswords();
    usersChanged = session.users().ensureAdminAccount() || usersChanged;
    if (usersChanged) {
        session.users().save(0);
    }
    if (!session.enterAdminForDev()) {
        QMessageBox::critical(0,
                              QString::fromUtf8(Vision::Translate::tr("用户库").c_str()),
                              QString::fromUtf8(Vision::Translate::tr("无法进入系统：没有可用账户。").c_str()));
        Vision::LogMgr::shutdown();
        return 1;
    }
    session.resetDevPerms();
    Vision::VisionCfgMgr::saveSettings(session.settings());
#else
    if (!session.autoLoginOperator()) {
        Vision::LoginDlg loginDlg(&session);
        if (loginDlg.exec() != QDialog::Accepted || !session.isLoggedIn()) {
            Vision::LogMgr::shutdown();
            return 0;
        }
    }
#endif

    Vision::NodeSettings node;
    node.load(0);
    const bool serviceRun = node.role != Vision::NodeRoleStandalone;
    const bool stationSvc = serviceRun && node.stationEnabled;
    const bool linkSvc = serviceRun && node.linkEnabled;
    std::string ipcErr;
    if (serviceRun) {
        Vision::SharedFrames::instance().map(Vision::SharedFrames::kDefaultSlotBytes, &ipcErr);
    }
    const bool consoleMode = app.arguments().contains(QStringLiteral("--console"));
    Vision::MainWindow w(&session);
    w.setResidentTray(!consoleMode);
    w.setServiceRun(serviceRun);
    ConsoleHost console;
    TrayApp* tray = 0;
    if (consoleMode) {
        if (!serviceRun) {
            console.setLocalRun(&consoleStartStation, &consoleStopStation, &w);
        }
        console.start();
    } else {
        tray = new TrayApp(&w, stationSvc, linkSvc, &app);
    }
    Vision::watchLicense(&app, &w);
    Vision::CrashDump::install();
    const int rc = app.exec();
    delete tray;
    Vision::LogMgr::shutdown();
    return rc;
}
