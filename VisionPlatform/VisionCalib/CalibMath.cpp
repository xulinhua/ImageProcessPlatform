/**
 * @file CalibMath.cpp
 * @brief CalibMath实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibMath.h"
#include "TextEncoding.h"
#include <cmath>
#include <vector>

namespace Vision {

namespace {

const double kPi = 3.14159265358979323846;

// 把宽字符转成 UTF-8
std::string U(const wchar_t* w)
{
    std::string o;
    if (!w) {
        return o;
    }
    for (; *w; ++w) {
        unsigned int c = (unsigned int)(*w);
        if (c < 0x80u) {
            o.push_back((char)c);
        } else if (c < 0x800u) {
            o.push_back((char)(0xC0u | (c >> 6)));
            o.push_back((char)(0x80u | (c & 0x3Fu)));
        } else {
            o.push_back((char)(0xE0u | (c >> 12)));
            o.push_back((char)(0x80u | ((c >> 6) & 0x3Fu)));
            o.push_back((char)(0x80u | (c & 0x3Fu)));
        }
    }
    return o;
}

// 带主元的高斯消元
bool solveLinear(std::vector<double> a, int n, std::vector<double> b, std::vector<double>* x)
{
    if (!x || n <= 0 || (int)a.size() != n * n || (int)b.size() != n) {
        return false;
    }
    for (int col = 0; col < n; ++col) {
        int piv = col;
        double best = std::fabs(a[col * n + col]);
        for (int r = col + 1; r < n; ++r) {
            const double v = std::fabs(a[r * n + col]);
            if (v > best) {
                best = v;
                piv = r;
            }
        }
        if (best < 1e-12) {
            return false;
        }
        if (piv != col) {
            for (int c = col; c < n; ++c) {
                const double tmp = a[col * n + c];
                a[col * n + c] = a[piv * n + c];
                a[piv * n + c] = tmp;
            }
            const double tb = b[col];
            b[col] = b[piv];
            b[piv] = tb;
        }
        const double div = a[col * n + col];
        for (int c = col; c < n; ++c) {
            a[col * n + c] /= div;
        }
        b[col] /= div;
        for (int r = 0; r < n; ++r) {
            if (r == col) {
                continue;
            }
            const double f = a[r * n + col];
            if (f == 0.0) {
                continue;
            }
            for (int c = col; c < n; ++c) {
                a[r * n + c] -= f * a[col * n + c];
            }
            b[r] -= f * b[col];
        }
    }
    *x = b;
    return true;
}

// 最小二乘。矩阵按行主序
bool leastSquares(const std::vector<double>& A, int rows, int cols,
                  const std::vector<double>& b, std::vector<double>* x)
{
    if (!x || rows <= 0 || cols <= 0 || (int)A.size() != rows * cols || (int)b.size() != rows) {
        return false;
    }
    std::vector<double> ata(cols * cols, 0.0);
    std::vector<double> atb(cols, 0.0);
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < cols; ++j) {
            double s = 0.0;
            for (int r = 0; r < rows; ++r) {
                s += A[r * cols + i] * A[r * cols + j];
            }
            ata[i * cols + j] = s;
        }
        double t = 0.0;
        for (int r = 0; r < rows; ++r) {
            t += A[r * cols + i] * b[r];
        }
        atb[i] = t;
    }
    return solveLinear(ata, cols, atb, x);
}

void identity3(double m[9])
{
    for (int i = 0; i < 9; ++i) {
        m[i] = 0.0;
    }
    m[0] = 1.0;
    m[4] = 1.0;
    m[8] = 1.0;
}

void mul3(const double a[9], const double b[9], double o[9])
{
    double t[9];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            t[r * 3 + c] = a[r * 3 + 0] * b[0 * 3 + c]
                + a[r * 3 + 1] * b[1 * 3 + c]
                + a[r * 3 + 2] * b[2 * 3 + c];
        }
    }
    for (int i = 0; i < 9; ++i) {
        o[i] = t[i];
    }
}

bool invert3(const double m[9], double inv[9])
{
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7])
        - m[1] * (m[3] * m[8] - m[5] * m[6])
        + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (std::fabs(det) < 1e-15) {
        return false;
    }
    const double s = 1.0 / det;
    inv[0] = (m[4] * m[8] - m[5] * m[7]) * s;
    inv[1] = (m[2] * m[7] - m[1] * m[8]) * s;
    inv[2] = (m[1] * m[5] - m[2] * m[4]) * s;
    inv[3] = (m[5] * m[6] - m[3] * m[8]) * s;
    inv[4] = (m[0] * m[8] - m[2] * m[6]) * s;
    inv[5] = (m[2] * m[3] - m[0] * m[5]) * s;
    inv[6] = (m[3] * m[7] - m[4] * m[6]) * s;
    inv[7] = (m[1] * m[6] - m[0] * m[7]) * s;
    inv[8] = (m[0] * m[4] - m[1] * m[3]) * s;
    return true;
}

int usedSamples(const std::vector<PlaneSample>& samples, std::vector<PlaneSample>* used)
{
    used->clear();
    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].used) {
            used->push_back(samples[i]);
        }
    }
    return (int)used->size();
}

// 点归一化，使透视拟合更稳
bool normalizePoints(const std::vector<double>& xs, const std::vector<double>& ys, double T[9],
                     std::vector<double>* nx, std::vector<double>* ny)
{
    const int n = (int)xs.size();
    if (n <= 0 || (int)ys.size() != n) {
        return false;
    }
    double mx = 0.0;
    double my = 0.0;
    for (int i = 0; i < n; ++i) {
        mx += xs[i];
        my += ys[i];
    }
    mx /= n;
    my /= n;
    double dist = 0.0;
    for (int i = 0; i < n; ++i) {
        dist += std::sqrt((xs[i] - mx) * (xs[i] - mx) + (ys[i] - my) * (ys[i] - my));
    }
    dist /= n;
    if (dist < 1e-9) {
        return false;
    }
    const double s = std::sqrt(2.0) / dist;
    identity3(T);
    T[0] = s;
    T[4] = s;
    T[2] = -s * mx;
    T[5] = -s * my;
    nx->resize(n);
    ny->resize(n);
    for (int i = 0; i < n; ++i) {
        (*nx)[i] = s * xs[i] + T[2];
        (*ny)[i] = s * ys[i] + T[5];
    }
    return true;
}

void fillResidual(PlaneMap* out)
{
    double sum = 0.0;
    double maxE = 0.0;
    int n = 0;
    for (size_t i = 0; i < out->samples.size(); ++i) {
        if (!out->samples[i].used) {
            continue;
        }
        double wx = 0.0;
        double wy = 0.0;
        if (!mapImageToWorld(out->imageToWorld, out->samples[i].imageX, out->samples[i].imageY, &wx, &wy)) {
            continue;
        }
        const double dx = wx - out->samples[i].worldX;
        const double dy = wy - out->samples[i].worldY;
        const double e = std::sqrt(dx * dx + dy * dy);
        sum += e * e;
        if (e > maxE) {
            maxE = e;
        }
        ++n;
    }
    out->rms = n > 0 ? std::sqrt(sum / n) : 0.0;
    out->maxResidual = maxE;
}

}

// 像素点经3×3映射到毫米
bool mapImageToWorld(const double m[9], double imageX, double imageY, double* worldX, double* worldY)
{
    if (!m || !worldX || !worldY) {
        return false;
    }
    const double w = m[6] * imageX + m[7] * imageY + m[8];
    if (std::fabs(w) < 1e-15) {
        return false;
    }
    *worldX = (m[0] * imageX + m[1] * imageY + m[2]) / w;
    *worldY = (m[3] * imageX + m[4] * imageY + m[5]) / w;
    return true;
}

// 绕中心逆时针转一个角度
void rotatePoint(double cx, double cy, double angleDeg, double x, double y, double* outX, double* outY)
{
    const double rad = angleDeg * kPi / 180.0;
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    const double dx = x - cx;
    const double dy = y - cy;
    if (outX) {
        *outX = cx + dx * c - dy * s;
    }
    if (outY) {
        *outY = cy + dx * s + dy * c;
    }
}

// 用点对拟合比例尺、仿射或透视
bool fitPlaneMap(CalibPlaneFit fit, const std::vector<PlaneSample>& samples, PlaneMap* out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = U(L"\u6CA1\u6709\u8F93\u51FA");
        }
        return false;
    }
    std::vector<PlaneSample> used;
    const int n = usedSamples(samples, &used);
    const int need = (fit == CalibFitScale) ? 2 : (fit == CalibFitAffine ? 3 : 4);
    if (n < need) {
        if (err) {
            *err = U(L"\u53C2\u4E0E\u62DF\u5408\u7684\u70B9\u6570\u4E0D\u591F");
        }
        return false;
    }
    PlaneMap map;
    map.present = true;
    map.fit = fit;
    map.samples = samples;
    identity3(map.imageToWorld);
    identity3(map.worldToImage);

    if (fit == CalibFitScale || fit == CalibFitAffine) {
        const int cols = (fit == CalibFitScale) ? 4 : 3;
        std::vector<double> A;
        std::vector<double> b;
        A.reserve(n * 2 * cols);
        b.reserve(n * 2);
        for (int i = 0; i < n; ++i) {
            const double x = used[i].imageX;
            const double y = used[i].imageY;
            if (fit == CalibFitScale) {
                A.push_back(x);
                A.push_back(-y);
                A.push_back(1.0);
                A.push_back(0.0);
                b.push_back(used[i].worldX);
                A.push_back(y);
                A.push_back(x);
                A.push_back(0.0);
                A.push_back(1.0);
                b.push_back(used[i].worldY);
            } else {
                A.push_back(x);
                A.push_back(y);
                A.push_back(1.0);
                b.push_back(used[i].worldX);
            }
        }
        if (fit == CalibFitAffine) {
            std::vector<double> bx;
            std::vector<double> by;
            std::vector<double> Ax = A;
            for (int i = 0; i < n; ++i) {
                bx.push_back(used[i].worldX);
                by.push_back(used[i].worldY);
            }
            std::vector<double> ux;
            std::vector<double> uy;
            if (!leastSquares(Ax, n, 3, bx, &ux) || !leastSquares(Ax, n, 3, by, &uy)) {
                if (err) {
                    *err = U(L"\u4EFF\u5C04\u62DF\u5408\u5931\u8D25\uFF0C\u70B9\u53EF\u80FD\u5171\u7EBF");
                }
                return false;
            }
            map.imageToWorld[0] = ux[0];
            map.imageToWorld[1] = ux[1];
            map.imageToWorld[2] = ux[2];
            map.imageToWorld[3] = uy[0];
            map.imageToWorld[4] = uy[1];
            map.imageToWorld[5] = uy[2];
            map.mmPerPixel = 0.0;
        } else {
            std::vector<double> u;
            if (!leastSquares(A, n * 2, 4, b, &u)) {
                if (err) {
                    *err = U(L"\u6BD4\u4F8B\u5C3A\u62DF\u5408\u5931\u8D25");
                }
                return false;
            }
            map.imageToWorld[0] = u[0];
            map.imageToWorld[1] = -u[1];
            map.imageToWorld[2] = u[2];
            map.imageToWorld[3] = u[1];
            map.imageToWorld[4] = u[0];
            map.imageToWorld[5] = u[3];
            map.mmPerPixel = std::sqrt(u[0] * u[0] + u[1] * u[1]);
        }
    } else {
        std::vector<double> ix(n);
        std::vector<double> iy(n);
        std::vector<double> wx(n);
        std::vector<double> wy(n);
        for (int i = 0; i < n; ++i) {
            ix[i] = used[i].imageX;
            iy[i] = used[i].imageY;
            wx[i] = used[i].worldX;
            wy[i] = used[i].worldY;
        }
        double Ti[9];
        double Tw[9];
        std::vector<double> nix;
        std::vector<double> niy;
        std::vector<double> nwx;
        std::vector<double> nwy;
        if (!normalizePoints(ix, iy, Ti, &nix, &niy) || !normalizePoints(wx, wy, Tw, &nwx, &nwy)) {
            if (err) {
                *err = U(L"\u70B9\u592A\u96C6\u4E2D\uFF0C\u65E0\u6CD5\u505A\u900F\u89C6\u62DF\u5408");
            }
            return false;
        }
        std::vector<double> A;
        std::vector<double> rhs;
        A.reserve(n * 2 * 8);
        rhs.reserve(n * 2);
        for (int i = 0; i < n; ++i) {
            const double x = nix[i];
            const double y = niy[i];
            const double X = nwx[i];
            const double Y = nwy[i];
            A.push_back(x);
            A.push_back(y);
            A.push_back(1.0);
            A.push_back(0.0);
            A.push_back(0.0);
            A.push_back(0.0);
            A.push_back(-X * x);
            A.push_back(-X * y);
            rhs.push_back(X);
            A.push_back(0.0);
            A.push_back(0.0);
            A.push_back(0.0);
            A.push_back(x);
            A.push_back(y);
            A.push_back(1.0);
            A.push_back(-Y * x);
            A.push_back(-Y * y);
            rhs.push_back(Y);
        }
        std::vector<double> h;
        if (!leastSquares(A, n * 2, 8, rhs, &h)) {
            if (err) {
                *err = U(L"\u900F\u89C6\u62DF\u5408\u5931\u8D25");
            }
            return false;
        }
        double Hn[9] = { h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], 1.0 };
        double TwInv[9];
        if (!invert3(Tw, TwInv)) {
            if (err) {
                *err = U(L"\u900F\u89C6\u62DF\u5408\u5931\u8D25");
            }
            return false;
        }
        double tmp[9];
        mul3(TwInv, Hn, tmp);
        mul3(tmp, Ti, map.imageToWorld);
        if (std::fabs(map.imageToWorld[8]) < 1e-15) {
            if (err) {
                *err = U(L"\u900F\u89C6\u62DF\u5408\u5931\u8D25");
            }
            return false;
        }
        const double s = 1.0 / map.imageToWorld[8];
        for (int i = 0; i < 9; ++i) {
            map.imageToWorld[i] *= s;
        }
        map.mmPerPixel = 0.0;
    }
    if (!invert3(map.imageToWorld, map.worldToImage)) {
        if (err) {
            *err = U(L"\u6620\u5C04\u4E0D\u53EF\u9006");
        }
        return false;
    }
    fillResidual(&map);
    *out = map;
    return true;
}

// 代数圆拟合，残差按点到圆周的距离
bool fitRotateCenter(const std::vector<RotateSample>& samples, RotateCenter* out, std::string* err)
{
    if (!out) {
        if (err) {
            *err = U(L"\u6CA1\u6709\u8F93\u51FA");
        }
        return false;
    }
    const int n = (int)samples.size();
    if (n < 3) {
        if (err) {
            *err = U(L"\u65CB\u8F6C\u4E2D\u5FC3\u81F3\u5C11\u8981\u4E09\u4E2A\u89D2\u5EA6");
        }
        return false;
    }
    std::vector<double> A;
    std::vector<double> rhs;
    A.reserve(n * 3);
    rhs.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double x = samples[i].imageX;
        const double y = samples[i].imageY;
        A.push_back(x);
        A.push_back(y);
        A.push_back(1.0);
        rhs.push_back(-(x * x + y * y));
    }
    std::vector<double> u;
    if (!leastSquares(A, n, 3, rhs, &u)) {
        if (err) {
            *err = U(L"\u65CB\u8F6C\u4E2D\u5FC3\u62DF\u5408\u5931\u8D25");
        }
        return false;
    }
    const double cx = -0.5 * u[0];
    const double cy = -0.5 * u[1];
    const double r2 = cx * cx + cy * cy - u[2];
    if (r2 <= 1.0) {
        if (err) {
            *err = U(L"\u65CB\u8F6C\u534A\u5F84\u8FC7\u5C0F");
        }
        return false;
    }
    const double radius = std::sqrt(r2);
    double sum = 0.0;
    double maxE = 0.0;
    for (int i = 0; i < n; ++i) {
        const double d = std::sqrt((samples[i].imageX - cx) * (samples[i].imageX - cx)
            + (samples[i].imageY - cy) * (samples[i].imageY - cy));
        const double e = std::fabs(d - radius);
        sum += e * e;
        if (e > maxE) {
            maxE = e;
        }
    }
    RotateCenter rc;
    rc.present = true;
    rc.imageX = cx;
    rc.imageY = cy;
    rc.radius = radius;
    rc.rms = std::sqrt(sum / n);
    rc.maxResidual = maxE;
    rc.samples = samples;
    *out = rc;
    return true;
}

}
