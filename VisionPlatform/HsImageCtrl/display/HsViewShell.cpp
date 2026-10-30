/**
 * @file HsViewShell.cpp
 * @brief HsViewShell 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCtrl/HsViewShell.h>
#include <HsCtrl/HsCloudWindow.h>
#include <HsCtrl/HsImageWindow.h>

#include <QAction>
#include <QMenu>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace HsCtrl {

class HsViewShell::Private {
public:
    HsImageWindow* image;
    HsCloudWindow* cloud;
    QStackedLayout* stack;
    QAction* actSwitchCloud;
    ViewMode mode;
    bool cloudEnabled;
    bool autoFollow;

    Private()
        : image(0)
        , cloud(0)
        , stack(0)
        , actSwitchCloud(0)
        , mode(ViewModeImage2D)
        , cloudEnabled(true)
        , autoFollow(false)
    {
    }
};

HsViewShell::HsViewShell(QWidget* parent)
    : QWidget(parent)
    , d(new Private)
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    QWidget* stackHost = new QWidget(this);
    d->stack = new QStackedLayout(stackHost);
    d->stack->setContentsMargins(0, 0, 0, 0);
    d->image = new HsImageWindow(stackHost);
    d->stack->addWidget(d->image);
    root->addWidget(stackHost, 1);
    connect(d->image, SIGNAL(contextMenuExtrasRequested(QList<QAction*>*)),
            this, SLOT(onImageMenuExtras(QList<QAction*>*)));
    d->actSwitchCloud = new QAction(QString::fromUtf8("切换到点云视图"), this);
    connect(d->actSwitchCloud, SIGNAL(triggered()), this, SLOT(onSwitchToCloud()));
    applyStackedVisibility();
}

HsViewShell::~HsViewShell()
{
    delete d;
}

ViewMode HsViewShell::viewMode() const
{
    return d->mode;
}

bool HsViewShell::cloudModeEnabled() const
{
    return d->cloudEnabled;
}

bool HsViewShell::autoFollowResult() const
{
    return d->autoFollow;
}

void HsViewShell::setCloudModeEnabled(bool on)
{
    d->cloudEnabled = on;
}

void HsViewShell::setAutoFollowResult(bool on)
{
    d->autoFollow = on;
}

HsImageWindow* HsViewShell::imageWindow() const
{
    return d->image;
}

HsCloudWindow* HsViewShell::cloudWindow() const
{
    return d->cloud;
}

HsCloudWindow* HsViewShell::ensureCloudWindow()
{
    if (d->cloud) {
        return d->cloud;
    }
    d->cloud = new HsCloudWindow(d->stack->parentWidget());
    d->stack->addWidget(d->cloud);
    connect(d->cloud, SIGNAL(contextMenuRequested(QPoint)), this, SLOT(onCloudContextMenu(QPoint)));
    return d->cloud;
}

void HsViewShell::setViewMode(ViewMode mode)
{
    if (mode == ViewModeCloud3D) {
        if (!d->cloudEnabled) {
            return;
        }
        ensureCloudWindow();
        if (d->cloud && !d->cloud->glReady() && d->cloud->isValid()) {
            // 首次显示会触发 initializeGL；此处仍允许切换
        }
    }
    if (d->mode == mode) {
        applyStackedVisibility();
        return;
    }
    d->mode = mode;
    applyStackedVisibility();
    emit viewModeChanged(mode);
}

void HsViewShell::applyStackedVisibility()
{
    if (!d->stack) {
        return;
    }
    if (d->mode == ViewModeCloud3D && d->cloud) {
        d->stack->setCurrentWidget(d->cloud);
    } else {
        d->stack->setCurrentWidget(d->image);
        d->mode = ViewModeImage2D;
    }
}

void HsViewShell::onImageMenuExtras(QList<QAction*>* out)
{
    if (!out || !d->actSwitchCloud) {
        return;
    }
    d->actSwitchCloud->setEnabled(d->cloudEnabled);
    out->append(d->actSwitchCloud);
}

void HsViewShell::onSwitchToCloud()
{
    setViewMode(ViewModeCloud3D);
}

void HsViewShell::onSwitchToImage()
{
    setViewMode(ViewModeImage2D);
}

void HsViewShell::onCloudResetView()
{
    if (d->cloud) {
        d->cloud->resetView();
    }
}

void HsViewShell::onCloudContextMenu(const QPoint& globalPos)
{
    showCloudContextMenu(globalPos);
}

void HsViewShell::showCloudContextMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    QAction* toImage = menu.addAction(QString::fromUtf8("切换到图像视图"));
    QAction* reset = menu.addAction(QString::fromUtf8("复位视图"));
    QAction* chosen = menu.exec(globalPos);
    if (chosen == toImage) {
        onSwitchToImage();
    } else if (chosen == reset) {
        onCloudResetView();
    }
}

}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#define HSIMAGE_MOC_HSVIEWSHELL "moc_HsViewShell.cpp"
#include HSIMAGE_MOC_HSVIEWSHELL
#endif
