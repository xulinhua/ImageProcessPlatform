/**
 * @file UiText.h
 * @brief UiText头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UITEXT_H
#define VISION_UITEXT_H

#include "VisionWidgetsExport.h"
#include "Translate.h"
#include <QAbstractButton>
#include <QAction>
#include <QByteArray>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QObject>
#include <QString>

/**
 * @brief 界面文本：走 VisionLang。源语言键为简体中文
 * @param key 与 Translate::tr 相同的源文字
 */
inline QString T(const char* key)
{
    return QString::fromUtf8(Vision::Translate::tr(key).c_str());
}

/** @brief 带一个占位符的界面句子。键里写 {0} */
inline QString Tf(const char* key, const QString& a0)
{
    const QByteArray bytes = a0.toUtf8();
    return QString::fromUtf8(Vision::Translate::format(key, std::string(bytes.constData(), static_cast<size_t>(bytes.size()))).c_str());
}

/** @brief 绑定源语言键，供语言热切换后重刷 */
inline void bindTrKey(QObject* o, const char* key)
{
    if (o && key) {
        o->setProperty("vpTr", QByteArray(key));
    }
}

/** @brief 按已绑定的 vpTr 键刷新控件/动作文案 */
    VISION_WIDGETS_API void applyTrKey(QObject* o);


/** @brief 递归刷新本对象树中绑定了 vpTr 的文案 */
    VISION_WIDGETS_API void retranslateBound(QObject* root);


/** @brief 设置文案并绑定源键 */
inline void setTrText(QLabel* w, const char* key)
{
    bindTrKey(w, key);
    w->setText(T(key));
}

/** @brief 设置按钮文案并绑定源键 */
inline void setTrText(QAbstractButton* w, const char* key)
{
    bindTrKey(w, key);
    w->setText(T(key));
}

/** @brief 设置动作文案并绑定源键 */
inline void setTrText(QAction* w, const char* key)
{
    bindTrKey(w, key);
    w->setText(T(key));
}

/** @brief 设置分组标题并绑定源键 */
inline void setTrText(QGroupBox* w, const char* key)
{
    bindTrKey(w, key);
    w->setTitle(T(key));
}

#endif
