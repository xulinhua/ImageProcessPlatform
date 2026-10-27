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

// ????????????????????????????????????????
class HSCTRL_EXPORT ImageDrawPad : public QWidget
{
    Q_OBJECT
public:
    explicit ImageDrawPad(QWidget* parent = nullptr);

    void setImageWindow(HsCtrl::HsImageWindow* view);
    HsCtrl::HsImageWindow* imageWindow() const;

    // ??????????????????????????????
    void setTools(const std::vector<HsCtrl::DrawTool>& tools);
    void popup(QWidget* anchor);
    // ?????????????????????????????????????
    void popupBrowse(QWidget* anchor);
    void startTool(HsCtrl::DrawTool tool);

signals:
    void drawingFinished(const HsCommData::Graphic& roi, bool accepted);

private slots:
    void onTool(int id);
    void onCommit();
    void onCancel();
    void onSessionFinished(const HsCommData::Graphic& roi, bool accepted);
    void onViewDestroyed();
    void onPenChanged();
    void onPickColor();

private:
    void rebuild();
    void bindView(HsCtrl::HsImageWindow* view);
    void syncSessionButtons();
    void place(QWidget* anchor);
    void startPreferredTool();
    void startBrowse();
    void applyPen();
    void buildStyleRow();
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void armDrag();
    static QString toolTitle(HsCtrl::DrawTool tool);

    HsCtrl::HsImageWindow* view_ = nullptr;
    std::vector<HsCtrl::DrawTool> tools_;
    QButtonGroup* group_ = nullptr;
    QToolButton* commit_ = nullptr;
    QToolButton* cancel_ = nullptr;
    QWidget* top_ = nullptr;
    QSlider* width_ = nullptr;
    QColor color_{22, 119, 255};
    bool dash_ = false;
    bool fill_ = false;
    QPoint dragPos_;
    bool dragging_ = false;
};

}
