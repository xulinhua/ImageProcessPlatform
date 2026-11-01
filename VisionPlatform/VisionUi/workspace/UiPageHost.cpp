/**
 * @file UiPageHost.cpp
 * @brief UiPageHost 实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UiPageHost.h"
#include "Job.h"
#include "QtConv.h"
#include "UiText.h"
#include <HsCtrl/HsImageWindow.h>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Vision {

UiPageHost::UiPageHost(QWidget* parent)
    : QWidget(parent)
    , m_sol(0)
    , m_canvas(0)
{
    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_canvas = new QWidget(this);
    m_canvas->setStyleSheet(QString::fromLatin1("background:#1E2228;"));
    lay->addWidget(m_canvas, 1);
}

QString UiPageHost::pathTail(const QString& path, const QString& prefix)
{
    if (path.startsWith(prefix, Qt::CaseInsensitive)) {
        return path.mid(prefix.size());
    }
    return QString();
}

void UiPageHost::bindPage(const UiPage& page, Solution* sol, const QMap<QString, Station*>& stations)
{
    m_page = page;
    m_sol = sol;
    m_stations = stations;
    m_labels.clear();
    m_images.clear();

    const QList<QWidget*> kids = m_canvas->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 0; i < kids.size(); ++i) {
        kids[i]->deleteLater();
    }

    m_canvas->setMinimumSize(page.width > 0 ? page.width : 800, page.height > 0 ? page.height : 600);

    for (size_t i = 0; i < page.widgets.size(); ++i) {
        const UiWidget& w = page.widgets[i];
        const QString wid = qs(w.id);
        if (w.kind == UiWImageView) {
            HsCtrl::HsImageWindow* img = new HsCtrl::HsImageWindow(m_canvas);
            img->setGeometry(w.x, w.y, w.w, w.h);
            img->show();
            m_images.insert(wid, img);
        } else if (w.kind == UiWLabel || w.kind == UiWValue || w.kind == UiWOkNg) {
            QLabel* lb = new QLabel(m_canvas);
            lb->setGeometry(w.x, w.y, w.w, w.h);
            lb->setAlignment(Qt::AlignCenter);
            lb->setText(qs(w.text));
            if (w.kind == UiWOkNg) {
                lb->setStyleSheet(QString::fromLatin1("background:#8C8C8C;color:#fff;font-weight:bold;"));
            } else {
                lb->setStyleSheet(QString::fromLatin1("color:#E8E8E8;"));
            }
            lb->show();
            m_labels.insert(wid, lb);
        } else {
            QPushButton* btn = new QPushButton(m_canvas);
            btn->setGeometry(w.x, w.y, w.w, w.h);
            btn->setText(qs(w.text));
            QString act = qs(w.action);
            if (act.isEmpty()) {
                if (w.kind == UiWRunOnce) {
                    act = QString::fromLatin1("runOnce");
                } else if (w.kind == UiWLoop) {
                    act = QString::fromLatin1("loop");
                }
            }
            btn->setProperty("action", act);
            connect(btn, SIGNAL(clicked()), this, SLOT(onActionClicked()));
            btn->show();
        }
    }
    refreshBindings();
}

void UiPageHost::refreshBindings()
{
    if (!m_sol) {
        return;
    }
    for (size_t i = 0; i < m_page.widgets.size(); ++i) {
        const UiWidget& w = m_page.widgets[i];
        const QString wid = qs(w.id);
        const QString path = qs(w.dataPath);

        if (w.kind == UiWLabel || w.kind == UiWValue) {
            QLabel* lb = m_labels.value(wid, 0);
            if (!lb) {
                continue;
            }
            const QString gname = pathTail(path, QString::fromLatin1("global:"));
            if (!gname.isEmpty()) {
                lb->setText(qs(m_sol->globalVars.getString(ss(gname), ss(qs(w.text)))));
            } else if (!w.text.empty()) {
                lb->setText(qs(w.text));
            }
        } else if (w.kind == UiWOkNg) {
            QLabel* lb = m_labels.value(wid, 0);
            if (!lb) {
                continue;
            }
            QString sid = pathTail(path, QString::fromLatin1("okng:"));
            Station* st = 0;
            if (!sid.isEmpty()) {
                st = m_stations.value(sid, 0);
            } else if (!m_stations.isEmpty()) {
                st = m_stations.begin().value();
            }
            const bool ok = st ? st->runtime().lastOk : false;
            lb->setText(ok ? QString::fromLatin1("OK") : QString::fromLatin1("NG"));
            lb->setStyleSheet(ok
                ? QString::fromLatin1("background:#52C41A;color:#fff;font-weight:bold;")
                : QString::fromLatin1("background:#FF4D4F;color:#fff;font-weight:bold;"));
        } else if (w.kind == UiWImageView) {
            HsCtrl::HsImageWindow* img = m_images.value(wid, 0);
            if (!img) {
                continue;
            }
            const QString toolId = pathTail(path, QString::fromLatin1("image:"));
            bool applied = false;
            if (!toolId.isEmpty()) {
                const ObjectId tid = oid(toolId);
                for (QMap<QString, Station*>::const_iterator it = m_stations.begin();
                     it != m_stations.end(); ++it) {
                    Station* st = it.value();
                    if (!st) {
                        continue;
                    }
                    Job* job = st->findJobOfTool(tid);
                    if (!job) {
                        continue;
                    }
                    const HsCommData::HsImage shot = job->cachedImage(tid);
                    if (!shot.isNull()) {
                        applyFrame(img, shot);
                        applied = true;
                        break;
                    }
                }
            }
            if (!applied) {
                // 占位：无图时保持清空
                img->clearImage();
            }
        }
    }
}

void UiPageHost::onActionClicked()
{
    QObject* s = sender();
    if (!s) {
        return;
    }
    const QString act = s->property("action").toString().trimmed().toLower();
    if (act == QLatin1String("runonce") || act == QLatin1String("runOnce")) {
        emit runOnceRequested();
    } else if (act == QLatin1String("loop")) {
        emit loopRequested();
    } else if (act == QLatin1String("stop")) {
        emit stopRequested();
    } else if (act == QLatin1String("reset")) {
        emit resetRequested();
    }
}

}

#include "moc_UiPageHost.cpp"
