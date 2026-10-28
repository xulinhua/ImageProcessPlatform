/**
 * @file AboutDlg.cpp
 * @brief AboutDlg实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AboutDlg.h"
#include "Theme.h"
#include "UiText.h"
#include "VersionDlg.h"
#include "VisionVersion.h"
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtGlobal>

namespace Vision {

// 关于：CV-X 品牌头 + 产品说明 + 版本摘要 + 版权
AboutDlg::AboutDlg(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(T("关于"));
    setModal(true);
    resize(480, 360);

    // 品牌区：大号标识 + 产品名
    QLabel* logo = new QLabel(QLatin1String("V"), this);
    logo->setObjectName(QLatin1String("toolDebugLogo"));
    logo->setAlignment(Qt::AlignCenter);
    logo->setFixedSize(56, 56);
    logo->setStyleSheet(QLatin1String(
        "QLabel#toolDebugLogo { font-size: 28px; font-weight: bold; color: #1677FF;"
        " border: 2px solid #1677FF; border-radius: 12px; background: #E6F4FF; }"));

    QLabel* brand = new QLabel(QLatin1String("CV-X"), this);
    brand->setObjectName(QLatin1String("toolDebugBrand"));
    brand->setStyleSheet(QLatin1String("font-size: 22px; font-weight: bold; color: #1677FF;"));

    QLabel* product = new QLabel(QString::fromUtf8(appProductName()), this);
    product->setStyleSheet(QLatin1String("font-size: 18px; font-weight: bold; color: #1F1F1F;"));

    QLabel* tagline = new QLabel(T("机器视觉通用工具平台"), this);
    tagline->setObjectName(QLatin1String("hint"));
    tagline->setWordWrap(true);

    QVBoxLayout* nameCol = new QVBoxLayout();
    nameCol->setSpacing(4);
    nameCol->addWidget(brand);
    nameCol->addWidget(product);
    nameCol->addWidget(tagline);

    QHBoxLayout* hero = new QHBoxLayout();
    hero->setSpacing(16);
    hero->addWidget(logo, 0, Qt::AlignTop);
    hero->addLayout(nameCol, 1);

    // 分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setObjectName(QLatin1String("toolDebugHeaderSep"));

    // 版本与构建（摘要；明细在版本管理）
    QLabel* ver = new QLabel(T("版本 ") + QString::fromLatin1(appVersion()), this);
    ver->setStyleSheet(QLatin1String("font-size: 14px; color: #1F1F1F;"));

    QLabel* build = new QLabel(T("构建时间：") + QString::fromUtf8(buildStamp().c_str()), this);
    build->setObjectName(QLatin1String("hint"));
    build->setWordWrap(true);

    QLabel* runtime = new QLabel(T("运行环境：Qt ") + QString::fromLatin1(qVersion())
                                 + T(" / VS2022 (v143) / C++17"), this);
    runtime->setObjectName(QLatin1String("hint"));
    runtime->setWordWrap(true);

    QLabel* desc = new QLabel(
        T("面向产线视觉检测：方案设计、多工站运行、工具调试与权限管控。"
          "算法与采集以插件方式注册，方案以 *.vpsol 落盘。"), this);
    desc->setWordWrap(true);
    desc->setObjectName(QLatin1String("hint"));

    QPushButton* moreVer = new QPushButton(T("查看版本详情…"), this);
    moreVer->setFlat(true);
    moreVer->setCursor(Qt::PointingHandCursor);
    moreVer->setStyleSheet(QLatin1String("QPushButton { color: #1677FF; text-align: left; padding: 0; }"));
    connect(moreVer, SIGNAL(clicked()), this, SLOT(onOpenVersions()));

    // 版权
    QLabel* copy = new QLabel(T("Copyright (c) 2026 VisionPlatform. All rights reserved."), this);
    copy->setObjectName(QLatin1String("hint"));
    copy->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    box->button(QDialogButtonBox::Ok)->setText(T("确定"));
    setButtonRole(box->button(QDialogButtonBox::Ok), "primary");
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));

    QVBoxLayout* root = new QVBoxLayout(this);
    prependFramelessTitleBar(this, root, windowTitle());
    QVBoxLayout* lay = framelessDialogContentLayout(this, root);
    lay->setSpacing(12);
    lay->addLayout(hero);
    lay->addWidget(line);
    lay->addWidget(ver);
    lay->addWidget(build);
    lay->addWidget(runtime);
    lay->addWidget(desc);
    lay->addWidget(moreVer, 0, Qt::AlignLeft);
    lay->addStretch(1);
    lay->addWidget(copy);
    lay->addWidget(box);
}

// 跳转版本管理
void AboutDlg::onOpenVersions()
{
    VersionDlg dlg(this);
    dlg.exec();
}

}

#include "moc_AboutDlg.cpp"