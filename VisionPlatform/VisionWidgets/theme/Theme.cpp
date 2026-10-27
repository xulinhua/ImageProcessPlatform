/**
 * @file Theme.cpp
 * @brief Theme实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "Theme.h"
#include "Translate.h"
#include <QAbstractButton>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QBoxLayout>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QSet>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QVariant>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif
#include <QWidget>

namespace Vision {

namespace {

// 标题栏拖拽移动无边框对话框
class DialogDragFilter : public QObject {
public:
    DialogDragFilter(QDialog* dlg, QObject* parent = 0)
        : QObject(parent)
        , m_dlg(dlg)
        , m_dragging(false)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event)
    {
        Q_UNUSED(watched);
        if (!m_dlg) {
            return false;
        }
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() != Qt::LeftButton) {
                break;
            }
            QWidget* w = qobject_cast<QWidget*>(watched);
            if (w) {
                QWidget* at = w->childAt(me->pos());
                for (QWidget* c = at; c && c != w; c = c->parentWidget()) {
                    if (qobject_cast<QAbstractButton*>(c)) {
                        return false;
                    }
                }
            }
            m_dragging = true;
            m_dragPos = me->globalPos() - m_dlg->frameGeometry().topLeft();
            break;
        }
        case QEvent::MouseMove:
            if (m_dragging && (static_cast<QMouseEvent*>(event)->buttons() & Qt::LeftButton)) {
                m_dlg->move(static_cast<QMouseEvent*>(event)->globalPos() - m_dragPos);
            }
            break;
        case QEvent::MouseButtonRelease:
            m_dragging = false;
            break;
        default:
            break;
        }
        return false;
    }

private:
    QDialog* m_dlg;   ///< 目标对话框
    bool m_dragging;  ///< 是否正在拖动
    QPoint m_dragPos; ///< 按下时鼠标相对窗口左上角的偏移
};

// Win 无边框：WM_NCCALCSIZE 让客户区铺满窗口，WM_NCHITTEST 保留边缘缩放
class FramelessNativeFilter : public QAbstractNativeEventFilter {
public:
    static FramelessNativeFilter* instance()
    {
        static FramelessNativeFilter* s = nullptr;
        if (!s && qApp) {
            s = new FramelessNativeFilter();
            qApp->installNativeEventFilter(s);
        }
        return s;
    }

    void registerDialog(QDialog* dlg)
    {
        if (!dlg) {
            return;
        }
        const QPointer<QDialog> p = dlg;
        QObject::connect(dlg, &QObject::destroyed, [this, p]() {
            unbindDialog(p);
        });
    }

    // Show / WinIdChange 后登记 hwnd，原生回调只查表、不再 winId()
    void bindHwnd(QDialog* dlg)
    {
        if (!dlg) {
            return;
        }
        const QPointer<QDialog> p = dlg;
        const HWND hwnd = reinterpret_cast<HWND>(dlg->winId());
        if (!hwnd) {
            return;
        }
        unbindDialog(p);
        m_hwndToDialog.insert(hwnd, p);
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, long* result)
    {
#ifdef _WIN32
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
            return false;
        }
        MSG* msg = static_cast<MSG*>(message);
        const QHash<HWND, QPointer<QDialog> >::const_iterator it = m_hwndToDialog.constFind(msg->hwnd);
        if (it == m_hwndToDialog.constEnd()) {
            return false;
        }
        QDialog* dlg = it.value().data();
        if (!dlg) {
            return false;
        }
        switch (msg->message) {
        case WM_NCCALCSIZE:
            if (msg->wParam) {
                *result = 0;
                return true;
            }
            break;
        case WM_NCHITTEST: {
            const int border = 6;
            RECT wr;
            GetWindowRect(msg->hwnd, &wr);
            const long x = GET_X_LPARAM(msg->lParam);
            const long y = GET_Y_LPARAM(msg->lParam);
            const bool atLeft = x >= wr.left && x < wr.left + border;
            const bool atRight = x < wr.right && x >= wr.right - border;
            const bool atTop = y >= wr.top && y < wr.top + border;
            const bool atBottom = y < wr.bottom && y >= wr.bottom - border;
            if (atTop && atLeft) {
                *result = HTTOPLEFT;
                return true;
            }
            if (atTop && atRight) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (atBottom && atLeft) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (atBottom && atRight) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (atLeft) {
                *result = HTLEFT;
                return true;
            }
            if (atRight) {
                *result = HTRIGHT;
                return true;
            }
            if (atTop) {
                *result = HTTOP;
                return true;
            }
            if (atBottom) {
                *result = HTBOTTOM;
                return true;
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            const QSize minS = dlg->minimumSize();
            if (minS.width() >= 320 && minS.height() >= 240) {
                mmi->ptMinTrackSize.x = minS.width();
                mmi->ptMinTrackSize.y = minS.height();
                const QSize maxS = dlg->maximumSize();
                if (maxS.width() < 16777215) {
                    mmi->ptMaxTrackSize.x = maxS.width();
                }
                if (maxS.height() < 16777215) {
                    mmi->ptMaxTrackSize.y = maxS.height();
                }
                // 消息已处理必须显式回 0，Qt 的 LRESULT 未初始化，漏写会把栈上垃圾返给 Windows
                *result = 0;
                return true;
            }
            break;
        }
        default:
            break;
        }
#else
        Q_UNUSED(eventType);
        Q_UNUSED(message);
        Q_UNUSED(result);
#endif
        return false;
    }

private:
    void unbindDialog(const QPointer<QDialog>& dlg)
    {
        for (QHash<HWND, QPointer<QDialog> >::iterator it = m_hwndToDialog.begin();
             it != m_hwndToDialog.end(); ) {
            if (it.value() == dlg || it.value().isNull()) {
                it = m_hwndToDialog.erase(it);
            } else {
                ++it;
            }
        }
    }

    QHash<HWND, QPointer<QDialog> > m_hwndToDialog; ///< hwnd → 无边框对话框（弱引用）
};

// 无边框窗：Win 原生缩放/阴影 + 外缘描边
class FramelessDialogChromeHelper : public QObject {
public:
    FramelessDialogChromeHelper(QDialog* dlg, QObject* parent = 0)
        : QObject(parent)
        , m_dlg(dlg)
        , m_outline(nullptr)
    {
        if (!m_dlg) {
            return;
        }
        m_outline = new QFrame(m_dlg);
        m_outline->setObjectName(QLatin1String("framelessOutline"));
        m_outline->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_outline->setFrameShape(QFrame::NoFrame);
        m_outline->setAutoFillBackground(false);
        m_dlg->installEventFilter(this);
        FramelessNativeFilter::instance()->registerDialog(m_dlg);
        syncOutline();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event)
    {
        if (watched != m_dlg) {
            return false;
        }
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest) {
            syncOutline();
        }
        if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
            applyNativeChrome();
        }
        return false;
    }

private:
    // 外缘 1px 灰线，随窗口尺寸更新并置顶（不挡鼠标）
    void syncOutline()
    {
        if (!m_dlg || !m_outline) {
            return;
        }
        m_outline->setGeometry(0, 0, m_dlg->width(), m_dlg->height());
        m_outline->raise();
    }

    void applyNativeChrome()
    {
#ifdef _WIN32
        if (!m_dlg) {
            return;
        }
        HWND hwnd = reinterpret_cast<HWND>(m_dlg->winId());
        if (!hwnd) {
            return;
        }
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        if (!(style & WS_THICKFRAME)) {
            style |= WS_THICKFRAME;
            SetWindowLongPtr(hwnd, GWL_STYLE, style);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                         | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        }
        BOOL ncRender = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_ENABLED, &ncRender, sizeof(ncRender));
        FramelessNativeFilter::instance()->bindHwnd(m_dlg);
#else
        Q_UNUSED(m_dlg);
#endif
    }

    QDialog* m_dlg;     ///< 目标对话框
    QFrame* m_outline;  ///< 外缘描边（透明、不拦截点击）
};

const char* kThemeIds[] = {
    "industrial",
    "industrialLarge",
    "industrialContrast"
};

}

// 浅色工业风全局样式；按档改字号与对比度，不改 ROI / 主色
QString appStyleSheet(const std::string& themeId)
{
    int fontPx = 13;
    int bannerPx = 42;
    QString text = QLatin1String("#333333");
    QString border = QLatin1String("#D9D9D9");
    QString hint = QLatin1String("#888888");
    if (themeId == "industrialLarge") {
        fontPx = 16;
        bannerPx = 52;
    } else if (themeId == "industrialContrast") {
        text = QLatin1String("#111111");
        border = QLatin1String("#8C8C8C");
        hint = QLatin1String("#444444");
    }

    QString qss = QString::fromUtf8(
        "QWidget { color: #333333; font-family: 'Microsoft YaHei','Segoe UI'; font-size: 13px; }"
        "QMainWindow, QDialog { background: #F5F7FA; }"
        "QMenuBar { background: #FFFFFF; border-bottom: 1px solid #D9D9D9; }"
        "QMenuBar::item:selected { background: #E6F7FF; color: #1890FF; }"
        "QMenu { background: #FFFFFF; border: 1px solid #D9D9D9; }"
        "QMenu::item:selected { background: #E6F7FF; color: #1890FF; }"
        "QToolBar { background: #FFFFFF; border: none; border-bottom: 1px solid #D9D9D9; spacing: 8px; padding: 4px 8px; }"
        "QStatusBar { background: #F5F7FA; color: #888888; border-top: 1px solid #D9D9D9; }"
        "QLabel { background: transparent; }"
        "QLabel#hint { color: #888888; }"
        "QLabel#section { color: #1890FF; font-weight: bold; font-size: 14px; }"
        "QLabel#bannerOk { background: #52C41A; color: #FFFFFF; font-size: 42px; font-weight: bold; border-radius: 4px; }"
        "QLabel#bannerNg { background: #FF4D4F; color: #FFFFFF; font-size: 42px; font-weight: bold; border-radius: 4px; }"
        "QFrame#card { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; }"
        "QFrame#panel { background: #F5F7FA; border: 1px solid #D9D9D9; }"
        "QTreeWidget, QListWidget, QTableWidget, QPlainTextEdit, QTextEdit {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; alternate-background-color: #FAFAFA;"
        "}"
        "QTreeWidget::item:selected, QListWidget::item:selected, QTableWidget::item:selected {"
        "  background: #E6F7FF; color: #333333;"
        "}"
        "QHeaderView::section { background: #F5F7FA; color: #888888; border: none; border-bottom: 1px solid #D9D9D9; padding: 6px 8px; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 4px 8px; min-height: 22px;"
        "}"
        "QComboBox {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 2px 8px; min-height: 22px;"
        "}"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; outline: 0;"
        "  selection-background-color: #E6F7FF; selection-color: #333333;"
        "}"
        "QComboBox QAbstractItemView::item { min-height: 22px; padding: 2px 8px; }"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #1890FF; }"
        "QGroupBox { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; margin-top: 14px; padding: 12px 8px 8px 8px; font-weight: bold; color: #1890FF; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 6px 16px; min-height: 26px; }"
        "QPushButton:hover { border-color: #1890FF; color: #1890FF; }"
        "QPushButton:pressed { background: #E6F7FF; }"
        "QPushButton:checked { background: #E6F7FF; border-color: #1890FF; color: #1890FF; }"
        "QPushButton:disabled { color: #BBBBBB; background: #F5F5F5; }"
        "QPushButton[primary=\"true\"] { background: #1890FF; color: #FFFFFF; border: none; }"
        "QPushButton[primary=\"true\"]:hover { background: #40A9FF; color: #FFFFFF; }"
        "QPushButton[success=\"true\"] { background: #52C41A; color: #FFFFFF; border: none; }"
        "QPushButton[success=\"true\"]:hover { background: #73D13D; color: #FFFFFF; }"
        "QPushButton#modeTab { background: transparent; border: none; border-bottom: 3px solid transparent; border-radius: 0; padding: 10px 28px; font-size: 15px; }"
        "QPushButton#modeTab:checked { color: #1890FF; font-weight: bold; border-bottom: 3px solid #1890FF; }"
        "QPushButton#rtStart { background: #52C41A; color: #FFFFFF; border: none; border-radius: 6px; font-size: 16px; min-height: 56px; }"
        "QPushButton#rtStop { background: #595959; color: #FFFFFF; border: none; border-radius: 6px; font-size: 16px; min-height: 56px; }"
        "QPushButton#rtGhost { background: #FFFFFF; color: #333333; border: 1px solid #D9D9D9; border-radius: 6px; font-size: 16px; min-height: 56px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
        "QSplitter::handle { background: #F5F7FA; }"
        "QScrollArea { border: none; background: #FFFFFF; }"
        "QDialogButtonBox QPushButton { min-width: 80px; }"
        "QToolButton { background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px; padding: 4px 10px; }"
        "QToolButton:checked { background: #E6F7FF; border-color: #1890FF; color: #1890FF; }"
        "QTabWidget::pane { border: 1px solid #D9D9D9; background: #FFFFFF; }"
        "QTabBar::tab { background: #F5F7FA; padding: 8px 16px; border: 1px solid #D9D9D9; border-bottom: none; }"
        "QTabBar::tab:selected { background: #FFFFFF; color: #1890FF; }"
        "QDialog[frameless=\"true\"] { background: #FFFFFF; }"
        "QFrame#framelessOutline { background: transparent; border: 1px solid #D9D9D9; }"
        "QWidget#dialogBody { background: #F5F7FA; }"
        "QWidget#toolDebugBody { background: #FFFFFF; }"
        "QFrame#dialogTitleBar { background: #FFFFFF; border: none; border-bottom: 1px solid #E8E8E8; border-radius: 0; min-height: 42px; }"
        "QLabel#dialogTitleText { font-size: 15px; font-weight: bold; color: #333333; background: transparent; }"
        "QPushButton#dialogCloseBtn { background: transparent; border: none; border-left: 1px solid #E8E8E8; min-width: 46px; max-width: 46px; min-height: 42px; max-height: 42px; border-radius: 0; font-size: 22px; color: #595959; padding: 0px; margin: 0px; }"
        "QPushButton#dialogCloseBtn:hover { background: #E81123; color: #FFFFFF; border-left: 1px solid #E81123; }"
        "QFrame#toolDebugHeader { background: #FFFFFF; border: none; border-bottom: 1px solid #E8E8E8; border-radius: 0; min-height: 42px; }"
        "QLabel#toolDebugLogo { background: #1890FF; color: #FFFFFF; font-weight: bold; font-size: 13px; min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px; border-radius: 4px; }"
        "QLabel#toolDebugBrand { color: #333333; font-weight: bold; font-size: 14px; }"
        "QFrame#toolDebugHeaderSep { color: #E0E0E0; max-width: 1px; }"
        "QLabel#toolDebugHeaderTitle { font-size: 15px; font-weight: bold; color: #333333; }"
        "QLabel#toolDebugHeaderSub { color: #888888; font-size: 12px; }"
        "QLabel#toolDebugHeaderHelp { color: #888888; font-size: 12px; min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; border: 1px solid #D9D9D9; border-radius: 9px; }"
        "QLabel#toolDebugFieldLabel { color: #666666; font-size: 12px; }"
        "QComboBox#toolDebugCombo { min-height: 28px; border: 1px solid #D9D9D9; border-radius: 4px; padding: 2px 8px; background: #FFFFFF; }"
        "QWidget#toolDebugParamBody { background: #FFFFFF; }"
        "QWidget#toolDebugParam { background: #FFFFFF; }"
        "QWidget#toolDebugParamCell { background: transparent; }"
        "QWidget#toolDebugParamRow { background: transparent; }"
        "QLabel#toolDebugParamLabel { color: #595959; font-size: 12px; background: transparent; min-height: 18px; }"
        "QSpinBox#toolDebugParamSpin, QDoubleSpinBox#toolDebugParamSpin {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px;"
        "  min-height: 28px; max-height: 28px; padding: 0 4px 0 8px;"
        "}"
        "QSpinBox#toolDebugParamSpin:focus, QDoubleSpinBox#toolDebugParamSpin:focus { border: 1px solid #1890FF; }"
        "QSpinBox#toolDebugParamSpin::up-button, QDoubleSpinBox#toolDebugParamSpin::up-button {"
        "  subcontrol-origin: border; subcontrol-position: top right; width: 22px; height: 14px;"
        "  border-left: 1px solid #E8E8E8; background: #FAFBFC; border-top-right-radius: 3px;"
        "}"
        "QSpinBox#toolDebugParamSpin::down-button, QDoubleSpinBox#toolDebugParamSpin::down-button {"
        "  subcontrol-origin: border; subcontrol-position: bottom right; width: 22px; height: 14px;"
        "  border-left: 1px solid #E8E8E8; background: #FAFBFC; border-bottom-right-radius: 3px;"
        "}"
        "QSpinBox#toolDebugParamSpin::up-button:hover, QDoubleSpinBox#toolDebugParamSpin::up-button:hover,"
        "QSpinBox#toolDebugParamSpin::down-button:hover, QDoubleSpinBox#toolDebugParamSpin::down-button:hover {"
        "  background: #E6F7FF;"
        "}"
        "QSpinBox#toolDebugParamSpin::up-arrow, QDoubleSpinBox#toolDebugParamSpin::up-arrow {"
        "  width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent;"
        "  border-bottom: 5px solid #595959; margin-bottom: 1px;"
        "}"
        "QSpinBox#toolDebugParamSpin::down-arrow, QDoubleSpinBox#toolDebugParamSpin::down-arrow {"
        "  width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent;"
        "  border-top: 5px solid #595959; margin-top: 1px;"
        "}"
        "QComboBox#toolDebugParamCombo {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px;"
        "  min-height: 28px; max-height: 28px; padding: 0 8px;"
        "}"
        "QComboBox#toolDebugParamCombo:focus { border: 1px solid #1890FF; }"
        "QComboBox#toolDebugParamCombo::drop-down { border: none; width: 24px; background: #FAFBFC;"
        "  border-left: 1px solid #E8E8E8; border-top-right-radius: 3px; border-bottom-right-radius: 3px; }"
        "QComboBox#toolDebugParamCombo::down-arrow {"
        "  width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent;"
        "  border-top: 5px solid #595959;"
        "}"
        "QCheckBox#toolDebugParamCheck { color: #333333; spacing: 8px; min-height: 28px; }"
        "QCheckBox#toolDebugParamCheck::indicator { width: 16px; height: 16px; border: 1px solid #D9D9D9;"
        "  border-radius: 3px; background: #FFFFFF; }"
        "QCheckBox#toolDebugParamCheck::indicator:checked { background: #1890FF; border-color: #1890FF; }"
        "QLineEdit#toolDebugParamLine {"
        "  background: #FFFFFF; border: 1px solid #D9D9D9; border-radius: 4px;"
        "  min-height: 28px; max-height: 28px; padding: 0 8px;"
        "}"
        "QLineEdit#toolDebugParamLine:focus { border: 1px solid #1890FF; }"
        "QComboBox#toolDebugCombo { min-height: 28px; border: 1px solid #D9D9D9; border-radius: 4px; padding: 2px 8px; background: #FFFFFF; }"
        "QFrame#toolDebugImageWrap { background: #E8ECF1; border: 1px solid #D9D9D9; border-radius: 6px; }"
        "QLabel#toolDebugDrawLabel { color: #595959; font-size: 12px; padding: 0 2px; }"
        "QCheckBox#toolDebugDrawSwitch { spacing: 0; }"
        "QCheckBox#toolDebugDrawSwitch::indicator { width: 36px; height: 20px; border-radius: 10px; background: #D9D9D9; border: none; }"
        "QCheckBox#toolDebugDrawSwitch::indicator:checked { background: #1890FF; }"
        "QFrame#toolDebugDrawPanel { background: transparent; }"
        "QToolButton#toolDebugShapeBtn { background: #FFFFFF; border: 1px solid #E8E8E8; border-radius: 6px; padding: 2px 10px; color: #595959; min-width: 40px; }"
        "QToolButton#toolDebugShapeBtn:hover { border-color: #91D5FF; color: #1890FF; }"
        "QToolButton#toolDebugShapeBtn:checked { background: #E6F7FF; border-color: #1890FF; color: #1890FF; }"
        "QFrame#toolDebugCard { background: #FFFFFF; border: 1px solid #E0E0E0; border-radius: 8px; }"
        "QFrame#toolDebugCardTitleBar { background: #FAFBFC; border-bottom: 1px solid #F0F0F0; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
        "QLabel#toolDebugCardBadge { background: #1890FF; color: #FFFFFF; font-weight: bold; font-size: 11px; min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; border-radius: 9px; qproperty-alignment: AlignCenter; }"
        "QLabel#toolDebugCardTitle { color: #333333; font-weight: bold; font-size: 13px; }"
        "QTableWidget#toolDebugRoiTable { border: 1px solid #F0F0F0; gridline-color: #F0F0F0; background: #FFFFFF; }"
        "QTableWidget#toolDebugRoiTable QHeaderView::vertical { width: 0px; max-width: 0px; border: none; margin: 0; padding: 0; }"
        "QTableWidget#toolDebugRoiTable::item { padding: 2px 4px; }"
        "QHeaderView::section { background: #FAFBFC; border: none; border-bottom: 1px solid #F0F0F0; padding: 4px 6px; color: #666666; font-size: 12px; }"
        "QPushButton#toolDebugAddRoi { background: #FFFFFF; color: #1890FF; border: 1px solid #1890FF; border-radius: 4px; padding: 4px 12px; }"
        "QPushButton#toolDebugAddRoi:hover { background: #E6F7FF; }"
        "QPushButton#toolDebugClearRoi { background: #FFFFFF; color: #FF4D4F; border: 1px solid #FF4D4F; border-radius: 4px; padding: 4px 12px; }"
        "QPushButton#toolDebugClearRoi:hover { background: #FFF1F0; }"
        "QToolButton#toolDebugRoiDel { color: #FF4D4F; border: none; background: transparent; font-size: 14px; }"
        "QPushButton#toolDebugSaveBtn { min-width: 88px; }"
        "QPushButton#toolDebugRunBtn { background: #1890FF; color: #FFFFFF; border: none; border-radius: 4px; min-width: 120px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton#toolDebugRunBtn:hover { background: #40A9FF; color: #FFFFFF; }"
        "QPlainTextEdit#toolDebugLog { background: #FAFBFC; border: 1px solid #E8E8E8; border-radius: 4px; padding: 4px 8px; color: #595959; font-size: 12px; }"
        "QFrame#toolDebugFooter { background: #FFFFFF; border: none; border-top: 1px solid #E8E8E8; border-radius: 0; }"
        "QFrame#toolDebugBottom { background: transparent; }"
        "QFrame#toolDebugTop { background: transparent; }"
        "QSplitter#toolDebugSplit::handle { background: #E8E8E8; height: 4px; margin: 0 4px; }"
        "QSplitter#toolDebugSplit::handle:hover { background: #1890FF; }"
    );
    qss.replace(QLatin1String("color: #333333"), QString::fromLatin1("color: ") + text);
    qss.replace(QLatin1String("#D9D9D9"), border);
    qss.replace(QLatin1String("#888888"), hint);
    qss.replace(QLatin1String("font-size: 13px"), QString::fromLatin1("font-size: %1px").arg(fontPx));
    qss.replace(QLatin1String("font-size: 42px"), QString::fromLatin1("font-size: %1px").arg(bannerPx));
    if (themeId == "industrialLarge") {
        qss.replace(QLatin1String("font-size: 15px"), QLatin1String("font-size: 18px"));
        qss.replace(QLatin1String("font-size: 16px"), QLatin1String("font-size: 18px"));
        qss.replace(QLatin1String("font-size: 14px"), QLatin1String("font-size: 16px"));
    }
    return qss;
}

// 默认浅色工业档
QString appStyleSheet()
{
    return appStyleSheet(std::string("industrial"));
}

// 把主题 QSS 设到 qApp
void applyAppTheme(const std::string& themeId)
{
    if (qApp) {
        qApp->setStyleSheet(appStyleSheet(themeId));
    }
}

// 主题档数量
int themeCount()
{
    return 3;
}

// 第 i 个主题存盘键
const char* themeIdAt(int i)
{
    if (i < 0 || i >= 3) {
        return kThemeIds[0];
    }
    return kThemeIds[i];
}

// 主题显示名
std::string themeTitle(const std::string& themeId)
{
    if (themeId == "industrialLarge") {
        return Translate::tr("浅色大字");
    }
    if (themeId == "industrialContrast") {
        return Translate::tr("高对比浅色");
    }
    return Translate::tr("浅色工业");
}

// 给按钮打动态属性并 polish，使样式表 [role=true] 生效
void setButtonRole(QPushButton* btn, const char* role)
{
    if (!btn) {
        return;
    }
    btn->setProperty(role, QVariant(true));
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}

// 白底卡片 Frame
QFrame* makeCard(QWidget* parent)
{
    QFrame* f = new QFrame(parent);
    f->setObjectName(QLatin1String("card"));
    f->setFrameShape(QFrame::NoFrame);
    return f;
}

// 允许对话框手动调整尺寸
void enableDialogResize(QDialog* dlg)
{
    if (!dlg) {
        return;
    }
    dlg->setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, false);
    // 无边框窗由 FramelessDialogChromeHelper 提供边缘缩放，不用右下角 grip
    if (!dlg->property("frameless").toBool()) {
        dlg->setSizeGripEnabled(true);
    }
}

// 去掉系统标题栏
void applyFramelessDialog(QDialog* dlg)
{
    if (!dlg) {
        return;
    }
    Qt::WindowFlags flags = dlg->windowFlags();
    flags |= Qt::FramelessWindowHint;
    flags &= ~Qt::WindowTitleHint;
    dlg->setWindowFlags(flags);
    dlg->setProperty("frameless", QVariant(true));
    dlg->setAttribute(Qt::WA_TranslucentBackground, false);
    dlg->setAutoFillBackground(true);
    dlg->setWindowFlag(Qt::MSWindowsFixedSizeDialogHint, false);
    dlg->setSizeGripEnabled(false);
    dlg->style()->unpolish(dlg);
    dlg->style()->polish(dlg);
    new FramelessDialogChromeHelper(dlg, dlg);
}

// 创建右上角关闭按钮（与标题栏等高）
QPushButton* makeDialogCloseButton(QDialog* dlg, QWidget* parent)
{
    QPushButton* btn = new QPushButton(QString(QChar(0x00D7)), parent);
    btn->setObjectName(QLatin1String("dialogCloseBtn"));
    btn->setToolTip(QString::fromUtf8(Translate::tr("关闭").c_str()));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setFixedSize(46, 42);
    if (dlg) {
        QObject::connect(btn, SIGNAL(clicked()), dlg, SLOT(close()));
    }
    return btn;
}

// 在指定区域启用拖拽移动
void installDialogDragHandle(QDialog* dlg, QWidget* handle)
{
    if (!dlg || !handle) {
        return;
    }
    handle->setCursor(Qt::SizeAllCursor);
    DialogDragFilter* filter = new DialogDragFilter(dlg, handle);
    handle->installEventFilter(filter);
}

// 创建 CV-X 品牌顶栏
QFrame* makeCvXTitleBar(QDialog* dlg, QWidget* parent, const QString& title,
                        const QString& subtitle, bool withHelp, QLabel** titleLabelOut)
{
    QFrame* bar = new QFrame(parent);
    bar->setObjectName(QLatin1String("toolDebugHeader"));
    bar->setFixedHeight(42);
    QHBoxLayout* h = new QHBoxLayout(bar);
    h->setContentsMargins(14, 0, 0, 0);
    h->setSpacing(10);

    QLabel* logo = new QLabel(QString::fromUtf8("V"), bar);
    logo->setObjectName(QLatin1String("toolDebugLogo"));
    logo->setAlignment(Qt::AlignCenter);
    QLabel* brand = new QLabel(QLatin1String("CV-X"), bar);
    brand->setObjectName(QLatin1String("toolDebugBrand"));
    QFrame* sep = new QFrame(bar);
    sep->setObjectName(QLatin1String("toolDebugHeaderSep"));
    sep->setFrameShape(QFrame::VLine);
    QLabel* text = new QLabel(title, bar);
    text->setObjectName(QLatin1String("dialogTitleText"));
    h->addWidget(logo, 0, Qt::AlignVCenter);
    h->addWidget(brand, 0, Qt::AlignVCenter);
    h->addWidget(sep, 0, Qt::AlignVCenter);
    h->addWidget(text, 0, Qt::AlignVCenter);
    if (withHelp) {
        QLabel* help = new QLabel(QString::fromUtf8("?"), bar);
        help->setObjectName(QLatin1String("toolDebugHeaderHelp"));
        help->setAlignment(Qt::AlignCenter);
        help->setToolTip(QString::fromUtf8(Translate::tr("详细调试").c_str()));
        h->addWidget(help, 0, Qt::AlignVCenter);
    }
    if (!subtitle.isEmpty()) {
        QLabel* sub = new QLabel(subtitle, bar);
        sub->setObjectName(QLatin1String("toolDebugHeaderSub"));
        h->addWidget(sub, 0, Qt::AlignVCenter);
    }
    h->addStretch();
    h->addWidget(makeDialogCloseButton(dlg, bar), 0, Qt::AlignTop | Qt::AlignRight);
    if (titleLabelOut) {
        *titleLabelOut = text;
    }
    if (dlg) {
        installDialogDragHandle(dlg, bar);
    }
    return bar;
}

// 在根布局顶插入标题栏
QLabel* prependFramelessTitleBar(QDialog* dlg, QBoxLayout* rootLayout, const QString& title)
{
    if (!dlg || !rootLayout) {
        return 0;
    }
    applyFramelessDialog(dlg);

    QLabel* text = nullptr;
    QFrame* bar = makeCvXTitleBar(dlg, dlg, title, QString(), false, &text);
    rootLayout->insertWidget(0, bar);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    return text;
}

// 无边框对话框内容区（顶栏/底栏之外的留白区域）
QVBoxLayout* framelessDialogContentLayout(QDialog* dlg, QBoxLayout* rootLayout, int margin, int topMargin)
{
    if (!dlg || !rootLayout) {
        return 0;
    }
    QWidget* body = new QWidget(dlg);
    body->setObjectName(QLatin1String("dialogBody"));
    QVBoxLayout* lay = new QVBoxLayout(body);
    lay->setContentsMargins(margin, topMargin, margin, margin);
    lay->setSpacing(10);
    lay->setSizeConstraint(QLayout::SetNoConstraint);
    rootLayout->addWidget(body, 1);
    return lay;
}

// 同步自绘标题栏文字
void syncDialogTitleBar(QDialog* dlg, const QString& title)
{
    if (!dlg) {
        return;
    }
    dlg->setWindowTitle(title);
    if (QLabel* lab = dlg->findChild<QLabel*>(QLatin1String("dialogTitleText"))) {
        lab->setText(title);
    }
}

// 未保存关窗：保存 / 不保存 / 取消
int askUnsavedCloseQuestion(QWidget* parent)
{
    QMessageBox box(QMessageBox::Question,
                    QString::fromUtf8(Translate::tr("未保存的修改").c_str()),
                    QString::fromUtf8(Translate::tr("有未保存的修改，是否保存？").c_str()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                    parent);
    box.setDefaultButton(QMessageBox::Yes);
    if (QAbstractButton* saveBtn = box.button(QMessageBox::Yes)) {
        saveBtn->setText(QString::fromUtf8(Translate::tr("保存").c_str()));
    }
    if (QAbstractButton* discardBtn = box.button(QMessageBox::No)) {
        discardBtn->setText(QString::fromUtf8(Translate::tr("不保存").c_str()));
    }
    if (QAbstractButton* cancelBtn = box.button(QMessageBox::Cancel)) {
        cancelBtn->setText(QString::fromUtf8(Translate::tr("取消").c_str()));
    }
    const int r = box.exec();
    if (r == QMessageBox::Cancel) {
        return 0;
    }
    if (r == QMessageBox::Yes) {
        return 2;
    }
    return 1;
}

}
