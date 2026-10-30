/**
 * @file HsCloudWindow.cpp
 * @brief HsCloudWindow 实现。点云 VBO + 轨道球；形状线框
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCtrl/HsCloudWindow.h>

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QOpenGLFunctions_2_1>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace HsCtrl {

namespace {

const int kDrawBudget = 1500000;

struct Vert {
    float x, y, z;
    float r, g, b;
};

QVector3D toQV(const HsCommData::Point3d& p)
{
    return QVector3D(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
}

void heightColor(double t, float* r, float* g, float* b)
{
    t = std::max(0.0, std::min(1.0, t));
    // 蓝-青-绿-黄-红
    if (t < 0.25) {
        const double u = t / 0.25;
        *r = 0.f;
        *g = static_cast<float>(u);
        *b = 1.f;
    } else if (t < 0.5) {
        const double u = (t - 0.25) / 0.25;
        *r = 0.f;
        *g = 1.f;
        *b = static_cast<float>(1.0 - u);
    } else if (t < 0.75) {
        const double u = (t - 0.5) / 0.25;
        *r = static_cast<float>(u);
        *g = 1.f;
        *b = 0.f;
    } else {
        const double u = (t - 0.75) / 0.25;
        *r = 1.f;
        *g = static_cast<float>(1.0 - u);
        *b = 0.f;
    }
}

void planePolygon(const HsCommData::Plane3& plane, const HsCommData::Aabb3& box,
                  std::vector<HsCommData::Point3d>* poly)
{
    if (!poly || box.isEmpty()) {
        return;
    }
    poly->clear();
    HsCommData::Point3d n = HsCommData::normalized(plane.normal);
    if (HsCommData::length(n) < 1e-12) {
        return;
    }
    const HsCommData::Point3d corners[8] = {
        HsCommData::Point3d(box.minCorner.x, box.minCorner.y, box.minCorner.z),
        HsCommData::Point3d(box.maxCorner.x, box.minCorner.y, box.minCorner.z),
        HsCommData::Point3d(box.minCorner.x, box.maxCorner.y, box.minCorner.z),
        HsCommData::Point3d(box.maxCorner.x, box.maxCorner.y, box.minCorner.z),
        HsCommData::Point3d(box.minCorner.x, box.minCorner.y, box.maxCorner.z),
        HsCommData::Point3d(box.maxCorner.x, box.minCorner.y, box.maxCorner.z),
        HsCommData::Point3d(box.minCorner.x, box.maxCorner.y, box.maxCorner.z),
        HsCommData::Point3d(box.maxCorner.x, box.maxCorner.y, box.maxCorner.z)
    };
    const int edges[12][2] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7},
        {0, 2}, {1, 3}, {4, 6}, {5, 7},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    std::vector<HsCommData::Point3d> hits;
    for (int e = 0; e < 12; ++e) {
        const HsCommData::Point3d& a = corners[edges[e][0]];
        const HsCommData::Point3d& b = corners[edges[e][1]];
        const double da = HsCommData::dot(n, a - plane.origin);
        const double db = HsCommData::dot(n, b - plane.origin);
        if (da * db > 0.0) {
            continue;
        }
        if (std::fabs(da - db) < 1e-12) {
            continue;
        }
        const double t = da / (da - db);
        hits.push_back(a + (b - a) * t);
    }
    if (hits.size() < 3) {
        return;
    }
    // 去重
    std::vector<HsCommData::Point3d> uniq;
    for (size_t i = 0; i < hits.size(); ++i) {
        bool dup = false;
        for (size_t j = 0; j < uniq.size(); ++j) {
            if (HsCommData::dist(hits[i], uniq[j]) < 1e-4) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            uniq.push_back(hits[i]);
        }
    }
    if (uniq.size() < 3) {
        return;
    }
    HsCommData::Point3d c(0, 0, 0);
    for (size_t i = 0; i < uniq.size(); ++i) {
        c = c + uniq[i];
    }
    c = c / static_cast<double>(uniq.size());
    HsCommData::Point3d u = HsCommData::normalized(HsCommData::cross(n, HsCommData::Point3d(0, 1, 0)));
    if (HsCommData::length(u) < 1e-6) {
        u = HsCommData::normalized(HsCommData::cross(n, HsCommData::Point3d(1, 0, 0)));
    }
    const HsCommData::Point3d v = HsCommData::normalized(HsCommData::cross(n, u));
    std::vector<std::pair<double, size_t> > order;
    for (size_t i = 0; i < uniq.size(); ++i) {
        const HsCommData::Point3d d = uniq[i] - c;
        order.push_back(std::make_pair(std::atan2(HsCommData::dot(d, v), HsCommData::dot(d, u)), i));
    }
    std::sort(order.begin(), order.end());
    for (size_t i = 0; i < order.size(); ++i) {
        poly->push_back(uniq[order[i].second]);
    }
}

}

struct HsCloudWindow::ExtraLayer {
    std::string name;
    HsCommData::Cloud3 cloud;
    HsCommData::Rgb8 color;
    GLuint vbo;
    int drawCount;
    int step;
    bool dirty;
    ExtraLayer() : vbo(0), drawCount(0), step(1), dirty(true) {}
};

struct HsCloudWindow::GpuCloud {
    GLuint vbo;
    int drawCount;
    int step;
    bool dirty;
    GpuCloud() : vbo(0), drawCount(0), step(1), dirty(true) {}
};

class HsCloudWindow::Private {
public:
    HsCommData::Cloud3 base;
    std::vector<ExtraLayer> extras;
    std::vector<HsCloudShape> shapes;
    std::vector<HsCommData::Pose3> poses;
    CloudColorMode colorMode;
    GpuCloud gpu;
    QOpenGLFunctions_2_1* gl;
    bool glOk;
    bool inited;

    QVector3D pivot;
    float distance;
    float yawDeg;
    float pitchDeg;
    float nearZ;
    float farZ;

    int dragButton;
    QPoint lastPos;
    int totalPoints;
    int drawnPoints;

    explicit Private()
        : colorMode(CloudColorHeight)
        , gl(0)
        , glOk(false)
        , inited(false)
        , pivot(0, 0, 0)
        , distance(1000.f)
        , yawDeg(0.f)
        , pitchDeg(20.f)
        , nearZ(1.f)
        , farZ(10000.f)
        , dragButton(0)
        , totalPoints(0)
        , drawnPoints(0)
    {
    }
};

HsCloudWindow::HsCloudWindow(QWidget* parent)
    : QOpenGLWidget(parent)
    , d(new Private)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(80, 80);
}

HsCloudWindow::~HsCloudWindow()
{
    makeCurrent();
    if (d->gl && d->gpu.vbo) {
        d->gl->glDeleteBuffers(1, &d->gpu.vbo);
    }
    for (size_t i = 0; i < d->extras.size(); ++i) {
        if (d->gl && d->extras[i].vbo) {
            d->gl->glDeleteBuffers(1, &d->extras[i].vbo);
        }
    }
    doneCurrent();
    delete d->gl;
    delete d;
}

const HsCommData::Cloud3& HsCloudWindow::cloud() const
{
    return d->base;
}

bool HsCloudWindow::glReady() const
{
    return d->glOk;
}

CloudColorMode HsCloudWindow::colorMode() const
{
    return d->colorMode;
}

void HsCloudWindow::setColorMode(CloudColorMode mode)
{
    d->colorMode = mode;
    d->gpu.dirty = true;
    update();
}

void HsCloudWindow::setCloud(const HsCommData::Cloud3& cloud)
{
    d->base = cloud;
    d->gpu.dirty = true;
    HsCommData::Aabb3 box = cloud.bounds();
    if (!box.isEmpty()) {
        d->pivot = toQV(box.center());
        const HsCommData::Point3d ext = box.extent();
        const float diag = static_cast<float>(std::sqrt(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z));
        d->distance = std::max(10.f, diag * 1.6f);
        d->nearZ = std::max(0.01f, d->distance * 0.001f);
        d->farZ = std::max(d->nearZ * 10.f, d->distance * 20.f);
    }
    update();
}

void HsCloudWindow::addCloud(const std::string& name, const HsCommData::Cloud3& cloud, const HsCommData::Rgb8& color)
{
    for (size_t i = 0; i < d->extras.size(); ++i) {
        if (d->extras[i].name == name) {
            d->extras[i].cloud = cloud;
            d->extras[i].color = color;
            d->extras[i].dirty = true;
            update();
            return;
        }
    }
    ExtraLayer layer;
    layer.name = name;
    layer.cloud = cloud;
    layer.color = color;
    layer.dirty = true;
    d->extras.push_back(layer);
    update();
}

void HsCloudWindow::removeCloud(const std::string& name)
{
    for (size_t i = 0; i < d->extras.size(); ++i) {
        if (d->extras[i].name != name) {
            continue;
        }
        makeCurrent();
        if (d->gl && d->extras[i].vbo) {
            d->gl->glDeleteBuffers(1, &d->extras[i].vbo);
        }
        doneCurrent();
        d->extras.erase(d->extras.begin() + static_cast<std::ptrdiff_t>(i));
        update();
        return;
    }
}

void HsCloudWindow::clearExtraClouds()
{
    makeCurrent();
    for (size_t i = 0; i < d->extras.size(); ++i) {
        if (d->gl && d->extras[i].vbo) {
            d->gl->glDeleteBuffers(1, &d->extras[i].vbo);
        }
    }
    doneCurrent();
    d->extras.clear();
    update();
}

void HsCloudWindow::setShapes(const std::vector<HsCloudShape>& shapes)
{
    d->shapes = shapes;
    update();
}

void HsCloudWindow::setPoses(const std::vector<HsCommData::Pose3>& poses)
{
    d->poses = poses;
    update();
}

void HsCloudWindow::resetView()
{
    HsCommData::Aabb3 box = d->base.bounds();
    for (size_t i = 0; i < d->extras.size(); ++i) {
        box = box.united(d->extras[i].cloud.bounds());
    }
    if (box.isEmpty()) {
        d->pivot = QVector3D(0, 0, 0);
        d->distance = 1000.f;
    } else {
        d->pivot = toQV(box.center());
        const HsCommData::Point3d ext = box.extent();
        const float diag = static_cast<float>(std::sqrt(ext.x * ext.x + ext.y * ext.y + ext.z * ext.z));
        d->distance = std::max(10.f, diag * 1.6f);
    }
    d->yawDeg = 0.f;
    d->pitchDeg = 20.f;
    d->nearZ = std::max(0.01f, d->distance * 0.001f);
    d->farZ = std::max(d->nearZ * 10.f, d->distance * 20.f);
    update();
}

void HsCloudWindow::initializeGL()
{
    d->gl = new QOpenGLFunctions_2_1;
    if (!d->gl->initializeOpenGLFunctions()) {
        delete d->gl;
        d->gl = 0;
        d->glOk = false;
        d->inited = true;
        return;
    }
    d->glOk = true;
    d->inited = true;
    d->gl->glEnable(GL_DEPTH_TEST);
    d->gl->glEnable(GL_POINT_SMOOTH);
    d->gl->glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

void HsCloudWindow::resizeGL(int w, int h)
{
    if (!d->gl) {
        return;
    }
    d->gl->glViewport(0, 0, w, h);
}

namespace {

void uploadCloud(QOpenGLFunctions_2_1* gl, const HsCommData::Cloud3& cloud, CloudColorMode mode,
                 const HsCommData::Rgb8* fixedColor, GLuint* vbo, int* drawCount, int* step, bool* dirty,
                 int budgetRemain)
{
    if (!gl || !vbo || !drawCount || !step || !dirty) {
        return;
    }
    if (!*dirty && *vbo != 0) {
        return;
    }
    if (*vbo == 0) {
        gl->glGenBuffers(1, vbo);
    }
    const int n = static_cast<int>(cloud.xyz.size());
    int s = 1;
    if (n > budgetRemain && budgetRemain > 0) {
        s = (n + budgetRemain - 1) / budgetRemain;
    }
    if (s < 1) {
        s = 1;
    }
    *step = s;
    const HsCommData::Aabb3 box = cloud.bounds();
    const double z0 = box.isEmpty() ? 0.0 : box.minCorner.z;
    const double z1 = box.isEmpty() ? 1.0 : box.maxCorner.z;
    const double zspan = (z1 > z0) ? (z1 - z0) : 1.0;
    const bool useGray = mode == CloudColorGray && cloud.gray.size() == cloud.xyz.size();
    const bool useRgb = mode == CloudColorRgb && cloud.rgb.size() == cloud.xyz.size();

    std::vector<Vert> verts;
    verts.reserve(static_cast<size_t>((n + s - 1) / s));
    for (int i = 0; i < n; i += s) {
        Vert v;
        v.x = static_cast<float>(cloud.xyz[static_cast<size_t>(i)].x);
        v.y = static_cast<float>(cloud.xyz[static_cast<size_t>(i)].y);
        v.z = static_cast<float>(cloud.xyz[static_cast<size_t>(i)].z);
        if (fixedColor) {
            v.r = fixedColor->r / 255.f;
            v.g = fixedColor->g / 255.f;
            v.b = fixedColor->b / 255.f;
        } else if (useRgb) {
            const HsCommData::Rgb8& c = cloud.rgb[static_cast<size_t>(i)];
            v.r = c.r / 255.f;
            v.g = c.g / 255.f;
            v.b = c.b / 255.f;
        } else if (useGray) {
            const float g = cloud.gray[static_cast<size_t>(i)];
            v.r = v.g = v.b = std::max(0.f, std::min(1.f, g));
        } else {
            heightColor((cloud.xyz[static_cast<size_t>(i)].z - z0) / zspan, &v.r, &v.g, &v.b);
        }
        verts.push_back(v);
    }
    *drawCount = static_cast<int>(verts.size());
    gl->glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    if (verts.empty()) {
        gl->glBufferData(GL_ARRAY_BUFFER, 0, 0, GL_STATIC_DRAW);
    } else {
        gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vert)),
                         &verts[0], GL_STATIC_DRAW);
    }
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    *dirty = false;
}

void drawVbo(QOpenGLFunctions_2_1* gl, GLuint vbo, int count)
{
    if (!gl || !vbo || count <= 0) {
        return;
    }
    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glEnableClientState(GL_VERTEX_ARRAY);
    gl->glEnableClientState(GL_COLOR_ARRAY);
    gl->glVertexPointer(3, GL_FLOAT, sizeof(Vert), reinterpret_cast<const void*>(0));
    gl->glColorPointer(3, GL_FLOAT, sizeof(Vert), reinterpret_cast<const void*>(sizeof(float) * 3));
    gl->glPointSize(2.f);
    gl->glDrawArrays(GL_POINTS, 0, count);
    gl->glDisableClientState(GL_COLOR_ARRAY);
    gl->glDisableClientState(GL_VERTEX_ARRAY);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

}

void HsCloudWindow::paintGL()
{
    if (!d->glOk || !d->gl) {
        QPainter p(this);
        p.fillRect(rect(), QColor(40, 40, 44));
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("OpenGL 不可用"));
        return;
    }

    d->gl->glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    d->gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const int w = std::max(1, width());
    const int h = std::max(1, height());
    d->gl->glMatrixMode(GL_PROJECTION);
    d->gl->glLoadIdentity();
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float fov = 45.f * static_cast<float>(M_PI) / 180.f;
    const float t = std::tan(fov * 0.5f) * d->nearZ;
    d->gl->glFrustum(-t * aspect, t * aspect, -t, t, d->nearZ, d->farZ);

    d->gl->glMatrixMode(GL_MODELVIEW);
    d->gl->glLoadIdentity();
    const float yaw = d->yawDeg * static_cast<float>(M_PI) / 180.f;
    const float pitch = d->pitchDeg * static_cast<float>(M_PI) / 180.f;
    const QVector3D eye(
        d->pivot.x() + d->distance * std::cos(pitch) * std::sin(yaw),
        d->pivot.y() + d->distance * std::sin(pitch),
        d->pivot.z() + d->distance * std::cos(pitch) * std::cos(yaw));
    // lookAt 手写：上方向接近 +Y
    QVector3D forward = (d->pivot - eye).normalized();
    QVector3D up(0, 1, 0);
    QVector3D right = QVector3D::crossProduct(forward, up).normalized();
    if (right.lengthSquared() < 1e-8f) {
        up = QVector3D(0, 0, 1);
        right = QVector3D::crossProduct(forward, up).normalized();
    }
    up = QVector3D::crossProduct(right, forward).normalized();
    float m[16] = {
        right.x(), up.x(), -forward.x(), 0,
        right.y(), up.y(), -forward.y(), 0,
        right.z(), up.z(), -forward.z(), 0,
        0, 0, 0, 1
    };
    d->gl->glMultMatrixf(m);
    d->gl->glTranslatef(-eye.x(), -eye.y(), -eye.z());

    int budget = kDrawBudget;
    uploadCloud(d->gl, d->base, d->colorMode, 0, &d->gpu.vbo, &d->gpu.drawCount, &d->gpu.step, &d->gpu.dirty, budget);
    budget -= d->gpu.drawCount;
    for (size_t i = 0; i < d->extras.size(); ++i) {
        ExtraLayer& layer = d->extras[i];
        const int remain = std::max(0, budget);
        uploadCloud(d->gl, layer.cloud, CloudColorHeight, &layer.color, &layer.vbo, &layer.drawCount,
                    &layer.step, &layer.dirty, remain);
        budget -= layer.drawCount;
    }

    d->totalPoints = static_cast<int>(d->base.xyz.size());
    d->drawnPoints = d->gpu.drawCount;
    for (size_t i = 0; i < d->extras.size(); ++i) {
        d->totalPoints += static_cast<int>(d->extras[i].cloud.xyz.size());
        d->drawnPoints += d->extras[i].drawCount;
    }

    drawVbo(d->gl, d->gpu.vbo, d->gpu.drawCount);
    for (size_t i = 0; i < d->extras.size(); ++i) {
        drawVbo(d->gl, d->extras[i].vbo, d->extras[i].drawCount);
    }

    // 形状
    HsCommData::Aabb3 scene = d->base.bounds();
    for (size_t i = 0; i < d->extras.size(); ++i) {
        scene = scene.united(d->extras[i].cloud.bounds());
    }
    d->gl->glDisable(GL_DEPTH_TEST);
    d->gl->glEnable(GL_BLEND);
    d->gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (size_t i = 0; i < d->shapes.size(); ++i) {
        const HsCloudShape& s = d->shapes[i];
        if (s.kind == HsCloudShape::Plane) {
            std::vector<HsCommData::Point3d> poly;
            planePolygon(s.plane, scene.isEmpty() ? HsCommData::Aabb3(
                              HsCommData::Point3d(-1, -1, -1), HsCommData::Point3d(1, 1, 1))
                                                  : scene,
                         &poly);
            if (poly.size() >= 3) {
                d->gl->glColor4f(0.2f, 0.7f, 1.f, 0.35f);
                d->gl->glBegin(GL_TRIANGLE_FAN);
                for (size_t k = 0; k < poly.size(); ++k) {
                    d->gl->glVertex3d(poly[k].x, poly[k].y, poly[k].z);
                }
                d->gl->glEnd();
                d->gl->glColor3f(0.9f, 0.95f, 1.f);
                d->gl->glBegin(GL_LINE_LOOP);
                for (size_t k = 0; k < poly.size(); ++k) {
                    d->gl->glVertex3d(poly[k].x, poly[k].y, poly[k].z);
                }
                d->gl->glEnd();
                HsCommData::Point3d n = HsCommData::normalized(s.plane.normal);
                const double len = scene.isEmpty() ? 10.0
                    : 0.15 * HsCommData::length(HsCommData::Point3d(
                                   scene.extent().x, scene.extent().y, scene.extent().z));
                const HsCommData::Point3d tip = s.plane.origin + n * len;
                d->gl->glColor3f(1.f, 0.4f, 0.2f);
                d->gl->glBegin(GL_LINES);
                d->gl->glVertex3d(s.plane.origin.x, s.plane.origin.y, s.plane.origin.z);
                d->gl->glVertex3d(tip.x, tip.y, tip.z);
                d->gl->glEnd();
            }
        } else if (s.kind == HsCloudShape::Box && !s.box.isEmpty()) {
            const HsCommData::Point3d local[8] = {
                HsCommData::Point3d(-s.box.hx, -s.box.hy, -s.box.hz),
                HsCommData::Point3d(s.box.hx, -s.box.hy, -s.box.hz),
                HsCommData::Point3d(s.box.hx, s.box.hy, -s.box.hz),
                HsCommData::Point3d(-s.box.hx, s.box.hy, -s.box.hz),
                HsCommData::Point3d(-s.box.hx, -s.box.hy, s.box.hz),
                HsCommData::Point3d(s.box.hx, -s.box.hy, s.box.hz),
                HsCommData::Point3d(s.box.hx, s.box.hy, s.box.hz),
                HsCommData::Point3d(-s.box.hx, s.box.hy, s.box.hz)
            };
            HsCommData::Point3d wpt[8];
            for (int k = 0; k < 8; ++k) {
                wpt[k] = s.box.pose.apply(local[k]);
            }
            const int edges[12][2] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 0},
                {4, 5}, {5, 6}, {6, 7}, {7, 4},
                {0, 4}, {1, 5}, {2, 6}, {3, 7}
            };
            d->gl->glColor3f(1.f, 0.85f, 0.2f);
            d->gl->glBegin(GL_LINES);
            for (int e = 0; e < 12; ++e) {
                const HsCommData::Point3d& a = wpt[edges[e][0]];
                const HsCommData::Point3d& b = wpt[edges[e][1]];
                d->gl->glVertex3d(a.x, a.y, a.z);
                d->gl->glVertex3d(b.x, b.y, b.z);
            }
            d->gl->glEnd();
        } else if (s.kind == HsCloudShape::Line) {
            d->gl->glColor3f(1.f, 1.f, 0.3f);
            d->gl->glBegin(GL_LINES);
            d->gl->glVertex3d(s.line.a.x, s.line.a.y, s.line.a.z);
            d->gl->glVertex3d(s.line.b.x, s.line.b.y, s.line.b.z);
            d->gl->glEnd();
        }
    }
    d->gl->glDisable(GL_BLEND);
    d->gl->glEnable(GL_DEPTH_TEST);

    // 角标
    QPainter painter(this);
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(8, 18, QString::fromUtf8("点数 %1 / 绘制 %2")
                     .arg(d->totalPoints).arg(d->drawnPoints));
    painter.end();
}

void HsCloudWindow::mousePressEvent(QMouseEvent* e)
{
    d->dragButton = e->button();
    d->lastPos = e->pos();
}

void HsCloudWindow::mouseMoveEvent(QMouseEvent* e)
{
    if (d->dragButton == Qt::LeftButton) {
        const QPoint dlt = e->pos() - d->lastPos;
        d->yawDeg += dlt.x() * 0.4f;
        d->pitchDeg = std::max(-89.f, std::min(89.f, d->pitchDeg - dlt.y() * 0.4f));
        d->lastPos = e->pos();
        update();
    } else if (d->dragButton == Qt::MiddleButton) {
        const QPoint dlt = e->pos() - d->lastPos;
        const float yaw = d->yawDeg * static_cast<float>(M_PI) / 180.f;
        const float pitch = d->pitchDeg * static_cast<float>(M_PI) / 180.f;
        QVector3D forward(
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::cos(yaw));
        QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0)).normalized();
        QVector3D up = QVector3D::crossProduct(right, forward).normalized();
        const float scale = d->distance * 0.002f;
        d->pivot -= right * (dlt.x() * scale);
        d->pivot += up * (dlt.y() * scale);
        d->lastPos = e->pos();
        update();
    } else {
        // 拾取：屏幕邻域最近已上传点（简化）
        bool hit = false;
        double hx = 0, hy = 0, hz = 0;
        float best = 6.f * 6.f;
        const float yaw = d->yawDeg * static_cast<float>(M_PI) / 180.f;
        const float pitch = d->pitchDeg * static_cast<float>(M_PI) / 180.f;
        const QVector3D eye(
            d->pivot.x() + d->distance * std::cos(pitch) * std::sin(yaw),
            d->pivot.y() + d->distance * std::sin(pitch),
            d->pivot.z() + d->distance * std::cos(pitch) * std::cos(yaw));
        QVector3D forward = (d->pivot - eye).normalized();
        QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0));
        if (right.lengthSquared() < 1e-8f) {
            right = QVector3D::crossProduct(forward, QVector3D(0, 0, 1));
        }
        right.normalize();
        QVector3D up = QVector3D::crossProduct(right, forward).normalized();
        const float aspect = static_cast<float>(std::max(1, width())) / static_cast<float>(std::max(1, height()));
        const float fov = 45.f * static_cast<float>(M_PI) / 180.f;
        auto project = [&](const HsCommData::Point3d& p, float* sx, float* sy, float* depth) {
            QVector3D w(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
            QVector3D rel = w - eye;
            const float zcam = QVector3D::dotProduct(rel, forward);
            if (zcam <= d->nearZ) {
                return false;
            }
            const float xcam = QVector3D::dotProduct(rel, right);
            const float ycam = QVector3D::dotProduct(rel, up);
            const float t = std::tan(fov * 0.5f);
            const float ndcx = xcam / (zcam * t * aspect);
            const float ndcy = ycam / (zcam * t);
            *sx = (ndcx * 0.5f + 0.5f) * width();
            *sy = (1.f - (ndcy * 0.5f + 0.5f)) * height();
            *depth = zcam;
            return true;
        };
        auto consider = [&](const HsCommData::Cloud3& c, int step) {
            const int n = static_cast<int>(c.xyz.size());
            for (int i = 0; i < n; i += std::max(1, step)) {
                float sx = 0, sy = 0, dep = 0;
                if (!project(c.xyz[static_cast<size_t>(i)], &sx, &sy, &dep)) {
                    continue;
                }
                const float dx = sx - e->pos().x();
                const float dy = sy - e->pos().y();
                const float dd = dx * dx + dy * dy;
                if (dd < best) {
                    best = dd;
                    hit = true;
                    hx = c.xyz[static_cast<size_t>(i)].x;
                    hy = c.xyz[static_cast<size_t>(i)].y;
                    hz = c.xyz[static_cast<size_t>(i)].z;
                }
            }
        };
        consider(d->base, d->gpu.step);
        for (size_t i = 0; i < d->extras.size(); ++i) {
            consider(d->extras[i].cloud, d->extras[i].step);
        }
        emit pickChanged(hit, hx, hy, hz);
    }
}

void HsCloudWindow::mouseReleaseEvent(QMouseEvent* e)
{
    Q_UNUSED(e);
    d->dragButton = 0;
}

void HsCloudWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    Q_UNUSED(e);
    resetView();
}

void HsCloudWindow::wheelEvent(QWheelEvent* e)
{
    const float factor = (e->angleDelta().y() > 0) ? 0.9f : 1.1f;
    d->distance = std::max(1.f, d->distance * factor);
    d->nearZ = std::max(0.01f, d->distance * 0.001f);
    d->farZ = std::max(d->nearZ * 10.f, d->distance * 20.f);
    update();
}

void HsCloudWindow::contextMenuEvent(QContextMenuEvent* e)
{
    emit contextMenuRequested(e->globalPos());
    e->accept();
}

}

#ifdef HSIMAGE_MOC_FROM_INTDIR
#define HSIMAGE_MOC_HSCLOUDWINDOW "moc_HsCloudWindow.cpp"
#include HSIMAGE_MOC_HSCLOUDWINDOW
#endif
