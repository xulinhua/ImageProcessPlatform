/**
 * @file image_draw_pad.h
 * @brief 绘制面板：工具按钮、提交 / 取消与画笔样式，驱动 HsImageWindow 的绘制会话
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#pragma once

#include <HsCtrl/HsImgCtrlExport.h>
#include <HsCtrl/HsImageWindowTypes.h>
#include <HsCommData/Graphic.h>

#include <QWidget>
#include <QColor>
#include <vector>

class QButtonGroup;
class QSlider;
class QToolButton;

namespace HsCtrl
{
class HsImageWindow;
}

namespace image_widget
{

/**
 * @class ImageDrawPad
 * @brief 浮动绘制面板。选工具即在绑定的画布上开绘制会话，会话结束发 drawingFinished
 */
class HSCTRL_EXPORT ImageDrawPad : public QWidget
{
    Q_OBJECT
public:
    /** @brief 构造面板，默认带浏览、圆、矩形、旋转矩形、椭圆、直线、轮廓、多边形 */
    explicit ImageDrawPad(QWidget* parent = nullptr);

    /************************ 画布与工具 ************************/
    /** @brief 绑定要操作的画布，并把当前画笔样式同步过去 */
    void setImageWindow(HsCtrl::HsImageWindow* view);
    /** @brief 当前绑定的画布 */
    HsCtrl::HsImageWindow* imageWindow() const;

    /** @brief 设置可选工具；空则用默认列表，缺浏览时自动补在最前 */
    void setTools(const std::vector<HsCtrl::DrawTool>& tools);
    /** @brief 在 anchor 旁弹出面板，并开始首选工具（优先轴对齐矩形） */
    void popup(QWidget* anchor);
    /** @brief 在 anchor 旁弹出面板，停在浏览工具，不开会话 */
    void popupBrowse(QWidget* anchor);
    /** @brief 选中并启动指定工具 */
    void startTool(HsCtrl::DrawTool tool);

signals:
    /** @brief 绘制会话结束；accepted 为 false 表示取消 */
    void drawingFinished(const HsCommData::Graphic& roi, bool accepted);

private slots:
    /** @brief 工具按钮切换：浏览取消会话，其他工具切设计态、清旧 Drawing 并开会话 */
    void onTool(int id);
    /** @brief 提交当前会话 */
    void onCommit();
    /** @brief 取消当前会话 */
    void onCancel();
    /** @brief 画布会话结束：回到浏览并转发 drawingFinished */
    void onSessionFinished(const HsCommData::Graphic& roi, bool accepted);
    /** @brief 画布被销毁时解绑 */
    void onViewDestroyed();
    /** @brief 颜色 / 线宽 / 线型 / 填充按钮变化后重设画笔 */
    void onPenChanged();
    /** @brief 弹出取色对话框选自定义颜色 */
    void onPickColor();

private:
    /************************ 内部构建 ************************/
    /** @brief 按 tools_ 重建工具按钮区，提交 / 取消按钮复用 */
    void rebuild();
    /** @brief 切换绑定画布并接好会话结束与销毁信号 */
    void bindView(HsCtrl::HsImageWindow* view);
    /** @brief 按会话状态启用提交 / 取消，无画布时禁用工具按钮 */
    void syncSessionButtons();
    /** @brief 显示面板；用户没拖动过时自动摆在 anchor 左、右、下、上中第一个放得下的位置 */
    void place(QWidget* anchor);
    /** @brief 启动首选工具：有轴对齐矩形用它，否则用第一个非浏览工具 */
    void startPreferredTool();
    /** @brief 选中浏览工具 */
    void startBrowse();
    /** @brief 把面板上的画笔设置写进画布的 Drawing 样式 */
    void applyPen();
    /** @brief 构建颜色、线宽、线型、填充四行样式控件 */
    void buildStyleRow();
    /** @brief 在非按钮、非滑块区域按住左键拖动面板 */
    bool eventFilter(QObject* watched, QEvent* event) override;
    /** @brief 关闭面板时取消进行中的会话 */
    void closeEvent(QCloseEvent* event) override;
    /** @brief 给面板及所有子控件装拖动事件过滤 */
    void armDrag();
    /** @brief 工具按钮显示的名称 */
    static QString toolTitle(HsCtrl::DrawTool tool);

    HsCtrl::HsImageWindow* view_ = nullptr; ///< 绑定的画布，不持有
    std::vector<HsCtrl::DrawTool> tools_;   ///< 可选工具，首项为浏览
    QButtonGroup* group_ = nullptr;         ///< 工具按钮组，id 为 DrawTool
    QToolButton* commit_ = nullptr;         ///< 提交按钮
    QToolButton* cancel_ = nullptr;         ///< 取消按钮
    QWidget* top_ = nullptr;                ///< 工具按钮区
    QSlider* width_ = nullptr;              ///< 线宽滑块，1..8
    QColor color_{22, 119, 255};            ///< 画笔颜色
    bool dash_ = false;                     ///< 是否虚线
    bool fill_ = false;                     ///< 是否填充（半透明同色）
    QPoint dragPos_;                        ///< 拖动时光标相对面板左上角的偏移
    bool dragging_ = false;                 ///< 是否正在拖动面板
};

}
