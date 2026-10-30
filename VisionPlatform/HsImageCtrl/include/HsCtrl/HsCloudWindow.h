/**
 * @file HsCloudWindow.h
 * @brief 点云三维浏览窗（OpenGL）。协议只认 HsCommData，不链 PCL
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCTRL_HSCLOUDWINDOW_H
#define HSCTRL_HSCLOUDWINDOW_H

#include <HsCtrl/HsImgCtrlExport.h>
#include <HsCommData/HsCloud.h>
#include <HsCommData/Shape3.h>
#include <HsCommData/Geom3.h>

#include <QOpenGLWidget>
#include <QString>
#include <QVector3D>
#include <string>
#include <vector>

class QContextMenuEvent;
class QMouseEvent;
class QWheelEvent;
class QPaintEvent;

namespace HsCtrl {

/** @brief 底点云着色 */
enum CloudColorMode {
    CloudColorHeight = 0, ///< 按包围盒 Z 映射伪彩
    CloudColorGray,       ///< Cloud3::gray，非法则退回高度
    CloudColorRgb         ///< Cloud3::rgb，非法则退回高度
};

/**
 * @class HsCloudShape
 * @brief 点云窗叠加形状（不依赖 VisionCommon 端口类型）
 */
struct HSCTRL_EXPORT HsCloudShape {
    enum Kind {
        Plane = 0,
        Box,
        Sphere,
        Line,
        Circle
    };
    Kind kind;                 ///< 种类
    HsCommData::Plane3 plane;  ///< Plane
    HsCommData::Box3 box;      ///< Box
    HsCommData::Sphere3 sphere;///< Sphere
    HsCommData::Line3 line;    ///< Line
    HsCommData::Circle3 circle;///< Circle
    /** @brief 缺省平面 */
    HsCloudShape() : kind(Plane) {}
};

/**
 * @class HsCloudWindow
 * @brief QOpenGLWidget 点云浏览。左键轨道球，滚轮缩放，中键平移，双击复位；右键只出菜单（由壳处理）
 */
class HSCTRL_EXPORT HsCloudWindow : public QOpenGLWidget {
    Q_OBJECT
public:
    /** @brief 空窗 */
    explicit HsCloudWindow(QWidget* parent = 0);
    /** @brief 释放 GPU 资源 */
    virtual ~HsCloudWindow();

    /** @brief 换底点云；空则清画面 */
    void setCloud(const HsCommData::Cloud3& cloud);
    /** @brief 当前底点云（只读） */
    const HsCommData::Cloud3& cloud() const;

    /** @brief 额外层；同名替换 */
    void addCloud(const std::string& name, const HsCommData::Cloud3& cloud, const HsCommData::Rgb8& color);
    /** @brief 去掉额外层 */
    void removeCloud(const std::string& name);
    /** @brief 清空额外层 */
    void clearExtraClouds();

    /** @brief 形状叠加 */
    void setShapes(const std::vector<HsCloudShape>& shapes);
    /** @brief 每位姿画小坐标轴（可空） */
    void setPoses(const std::vector<HsCommData::Pose3>& poses);

    /** @brief 底云着色模式 */
    void setColorMode(CloudColorMode mode);
    /** @brief 当前着色模式 */
    CloudColorMode colorMode() const;

    /** @brief 相机正对包围盒 */
    void resetView();

    /** @brief OpenGL 是否已成功初始化 */
    bool glReady() const;

signals:
    /** @brief 右键请求菜单（壳统一弹出） */
    void contextMenuRequested(const QPoint& globalPos);
    /** @brief 拾取点变化；未命中时 valid=false */
    void pickChanged(bool valid, double x, double y, double z);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    struct ExtraLayer;
    struct GpuCloud;
    class Private;
    Private* d;
};

}

#endif
