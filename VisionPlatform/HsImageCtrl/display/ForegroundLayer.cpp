/**
 * @file ForegroundLayer.cpp
 * @brief ForegroundLayer实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ForegroundLayer.h"
namespace HsCtrl {
ForegroundLayer::ForegroundLayer()
    : m_hud(true)
{
}
void ForegroundLayer::clearInfos()
{
    m_imageInfos.clear();
    m_windowInfos.clear();
}
void ForegroundLayer::clearInfos(DrawInfoSpace space)
{
    if (space == DrawInfoWindow) m_windowInfos.clear();
    else m_imageInfos.clear();
}

// HUD可见
bool& ForegroundLayer::hudVisible()
{
    return m_hud;
}

// HUD可见（只读）
bool ForegroundLayer::hudVisible() const
{
    return m_hud;
}

// 信息文本
QString& ForegroundLayer::infoText()
{
    return m_info;
}

// 信息文本（只读）
const QString& ForegroundLayer::infoText() const
{
    return m_info;
}

// 图像信息项
QVector<DrawInfoEntry>& ForegroundLayer::imageInfos()
{
    return m_imageInfos;
}

// 窗口信息项
QVector<DrawInfoEntry>& ForegroundLayer::windowInfos()
{
    return m_windowInfos;
}

}