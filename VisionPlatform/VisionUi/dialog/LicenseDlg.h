/**
 * @file LicenseDlg.h
 * @brief 帮助菜单里的软件注册页：查看本机许可并打开注册程序
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_DLG_H
#define VISION_LICENSE_DLG_H
#include "VisionUiExport.h"

#include <QDialog>

class QLabel;
class QVBoxLayout;

namespace Vision {

/**
 * @class LicenseDlg
 * @brief 软件注册。上面是产品状态，下面可以打开注册程序并刷新
 */
class VISION_UI_API LicenseDlg : public QDialog {
public:
    /** @brief 打开时先复查一次本机许可证 */
    explicit LicenseDlg(QWidget* parent = 0);

private:
    /** @brief 按当前许可证刷新状态、编号、期限和序号 */
    void reload();
    /** @brief 打开 Vision.LicenseRequest.exe，退出后再刷新 */
    void registerProduct();
    /** @brief 在卡片里加一行名称和值，返回值标签 */
    QLabel* addFact(QVBoxLayout* layout, const QString& name);

    QLabel* m_pill;     ///< 已注册、未注册、已过期
    QLabel* m_state;    ///< 许可状态这句话
    QLabel* m_id;       ///< 编号
    QLabel* m_term;     ///< 永久或到期日
    QLabel* m_serial;   ///< 序号
};

}

#endif
