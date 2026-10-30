/**
 * @file ProcessCombo.h
 * @brief 共享进程画面时选进程的下拉框
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_CONTROLLED_PROCESS_COMBO_H
#define VISION_CONTROLLED_PROCESS_COMBO_H

#include <QComboBox>

namespace Vision {

/**
 * @class ProcessCombo
 * @brief 可编辑下拉框：每次展开时列出当前有顶层窗口的进程（带程序图标），也可手输未运行的进程名
 */
class ProcessCombo : public QComboBox {
public:
    /** @brief 建好后先列一次进程 */
    explicit ProcessCombo(QWidget* parent = 0);

    /** @brief 当前进程名，去掉首尾空白 */
    QString process() const;
    /** @brief 设为指定进程名，不要求它正在运行 */
    void setProcess(const QString& name);
    /** @brief 重新列出进程，保留编辑框里的文字 */
    void reload();

protected:
    /** @brief 展开前刷新列表 */
    void showPopup() override;
};

}

#endif
