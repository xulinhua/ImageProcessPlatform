/**
 * @file CalibDb.cpp
 * @brief 标定记录先入库。内参、平面、旋转各一张表，样本另表
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CalibDb.h"
#include "AppPaths.h"
#include "SolutionDataPaths.h"
#include "SqliteDatabase.h"
#include "TextEncoding.h"
#include <cstdio>
#include <mutex>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

namespace {

std::mutex g_mu;
SqliteDatabase g_db;

std::string num(double v)
{
    char buf[64];
    sprintf(buf, "%.17g", v);
    return std::string(buf);
}

bool copyFileTo(const std::string& src, const std::string& dst)
{
    return CopyFileW(utf8ToWide(src).c_str(), utf8ToWide(dst).c_str(), FALSE) != 0;
}

bool ensureDir(const std::string& path)
{
    return CreateDirectoryW(utf8ToWide(path).c_str(), 0) || GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

CalibDb& CalibDb::instance()
{
    static CalibDb g;
    return g;
}

bool CalibDb::open(std::string* error)
{
    AppPaths::ensureDirs();
    if (!g_db.isOpen() && !g_db.open(AppPaths::calibFile(), error)) {
        return false;
    }
    SchemaMigrator mig(&g_db);
    if (!mig.ensureVersionTable(error)) {
        return false;
    }
    if (mig.currentVersion(error) >= 1) {
        return true;
    }
    const char* plate =
        "CREATE TABLE IF NOT EXISTS plate ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "time_iso TEXT, solution TEXT, camera_id TEXT, camera_name TEXT,"
        "source INTEGER, status INTEGER, applied INTEGER,"
        "rms TEXT, max_residual TEXT, residual_limit TEXT,"
        "fx TEXT, fy TEXT, cx TEXT, cy TEXT, k1 TEXT, k2 TEXT, k3 TEXT, p1 TEXT, p2 TEXT,"
        "plate_kind INTEGER, rows INTEGER, cols INTEGER, spacing_mm TEXT, image_count INTEGER,"
        "image_w INTEGER, image_h INTEGER,"
        "param_rows INTEGER, param_cols INTEGER, param_spacing TEXT, param_thickness TEXT,"
        "param_fit INTEGER, param_res_px TEXT, param_res_mm TEXT,"
        "halcon_path TEXT, image_dir TEXT);";
    const char* plateImage =
        "CREATE TABLE IF NOT EXISTS plate_image ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, plate_id INTEGER, path TEXT);";
    const char* plane =
        "CREATE TABLE IF NOT EXISTS plane ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "time_iso TEXT, solution TEXT, camera_id TEXT, camera_name TEXT,"
        "source INTEGER, status INTEGER, applied INTEGER, parent_plate_id INTEGER,"
        "fit INTEGER, rms TEXT, max_residual TEXT, mm_per_pixel TEXT,"
        "m0 TEXT, m1 TEXT, m2 TEXT, m3 TEXT, m4 TEXT, m5 TEXT, m6 TEXT, m7 TEXT, m8 TEXT,"
        "w0 TEXT, w1 TEXT, w2 TEXT, w3 TEXT, w4 TEXT, w5 TEXT, w6 TEXT, w7 TEXT, w8 TEXT);";
    const char* planePoint =
        "CREATE TABLE IF NOT EXISTS plane_point ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, plane_id INTEGER,"
        "image_x TEXT, image_y TEXT, world_x TEXT, world_y TEXT, used INTEGER);";
    const char* rotate =
        "CREATE TABLE IF NOT EXISTS rotate ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "time_iso TEXT, solution TEXT, camera_id TEXT, camera_name TEXT,"
        "source INTEGER, status INTEGER, applied INTEGER, parent_plane_id INTEGER,"
        "image_x TEXT, image_y TEXT, world_x TEXT, world_y TEXT, world_valid INTEGER,"
        "radius TEXT, rms TEXT, max_residual TEXT);";
    const char* rotateAngle =
        "CREATE TABLE IF NOT EXISTS rotate_angle ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, rotate_id INTEGER,"
        "angle_deg TEXT, image_x TEXT, image_y TEXT);";
    if (!g_db.exec(plate, error) || !g_db.exec(plateImage, error)
        || !g_db.exec(plane, error) || !g_db.exec(planePoint, error)
        || !g_db.exec(rotate, error) || !g_db.exec(rotateAngle, error)) {
        return false;
    }
    return mig.setVersion(1, error);
}

bool CalibDb::recordPlate(const std::string& solutionDir, const std::string& cameraId,
                          const std::string& cameraName, int imageW, int imageH,
                          const CalibParam& param, const CamModel& model,
                          const std::string& halconSrc, const std::string& offlineDir,
                          int64_t* id, std::string* error)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!open(error)) {
        return false;
    }
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(model.updatedAt));
    a.push_back(DbValue::fromText(solutionDir));
    a.push_back(DbValue::fromText(cameraId));
    a.push_back(DbValue::fromText(cameraName));
    a.push_back(DbValue::fromInt(model.source));
    a.push_back(DbValue::fromInt(model.status));
    a.push_back(DbValue::fromInt(0));
    a.push_back(DbValue::fromText(num(model.rms)));
    a.push_back(DbValue::fromText(num(model.maxResidual)));
    a.push_back(DbValue::fromText(num(param.residualMaxPx)));
    a.push_back(DbValue::fromText(num(model.fx)));
    a.push_back(DbValue::fromText(num(model.fy)));
    a.push_back(DbValue::fromText(num(model.cx)));
    a.push_back(DbValue::fromText(num(model.cy)));
    a.push_back(DbValue::fromText(num(model.k1)));
    a.push_back(DbValue::fromText(num(model.k2)));
    a.push_back(DbValue::fromText(num(model.k3)));
    a.push_back(DbValue::fromText(num(model.p1)));
    a.push_back(DbValue::fromText(num(model.p2)));
    a.push_back(DbValue::fromInt(model.plateKind));
    a.push_back(DbValue::fromInt(model.rows));
    a.push_back(DbValue::fromInt(model.cols));
    a.push_back(DbValue::fromText(num(model.spacingMm)));
    a.push_back(DbValue::fromInt(model.count));
    a.push_back(DbValue::fromInt(imageW));
    a.push_back(DbValue::fromInt(imageH));
    a.push_back(DbValue::fromInt(param.rows));
    a.push_back(DbValue::fromInt(param.cols));
    a.push_back(DbValue::fromText(num(param.spacingMm)));
    a.push_back(DbValue::fromText(num(param.thicknessMm)));
    a.push_back(DbValue::fromInt(param.planeFit));
    a.push_back(DbValue::fromText(num(param.residualMaxPx)));
    a.push_back(DbValue::fromText(num(param.residualMaxMm)));
    a.push_back(DbValue::fromText(std::string()));
    a.push_back(DbValue::fromText(offlineDir));
    if (!g_db.exec(
            "INSERT INTO plate(time_iso,solution,camera_id,camera_name,source,status,applied,"
            "rms,max_residual,residual_limit,fx,fy,cx,cy,k1,k2,k3,p1,p2,"
            "plate_kind,rows,cols,spacing_mm,image_count,image_w,image_h,"
            "param_rows,param_cols,param_spacing,param_thickness,param_fit,param_res_px,param_res_mm,"
            "halcon_path,image_dir) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,"
            "?20,?21,?22,?23,?24,?25,?26,?27,?28,?29,?30,?31,?32,?33,?34,?35);",
            a, error)) {
        return false;
    }
    const int64_t rowId = g_db.lastInsertId();
    if (id) {
        *id = rowId;
    }
    char folderName[64];
    sprintf(folderName, "plate_%lld", static_cast<long long>(rowId));
    const std::string folder = pathJoin(AppPaths::calibDir(), folderName);
    ensureDir(folder);
    if (!halconSrc.empty() && GetFileAttributesW(utf8ToWide(halconSrc).c_str()) != INVALID_FILE_ATTRIBUTES) {
        const std::string dst = pathJoin(folder, "camera.halcon.dat");
        if (copyFileTo(halconSrc, dst)) {
            std::vector<DbValue> u;
            u.push_back(DbValue::fromText(dst));
            u.push_back(DbValue::fromInt(rowId));
            g_db.exec("UPDATE plate SET halcon_path=?1 WHERE id=?2;", u, 0);
        }
    }
    const std::wstring pattern = utf8ToWide(pathJoin(offlineDir, "*"));
    WIN32_FIND_DATAW fd;
    HANDLE h = offlineDir.empty() ? INVALID_HANDLE_VALUE : FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        const std::string imageDir = pathJoin(folder, "offline");
        ensureDir(imageDir);
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            char nameBuf[512];
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, nameBuf, (int)sizeof(nameBuf), 0, 0);
            const std::string src = pathJoin(offlineDir, nameBuf);
            const std::string dst = pathJoin(imageDir, nameBuf);
            if (!copyFileTo(src, dst)) {
                continue;
            }
            std::vector<DbValue> img;
            img.push_back(DbValue::fromInt(rowId));
            img.push_back(DbValue::fromText(dst));
            g_db.exec("INSERT INTO plate_image(plate_id, path) VALUES(?1,?2);", img, 0);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return true;
}

bool CalibDb::recordPlane(const std::string& solutionDir, const std::string& cameraId,
                          const std::string& cameraName, const CalibParam& param,
                          const PlaneMap& model, int64_t* id, std::string* error)
{
    (void)param;
    std::lock_guard<std::mutex> lock(g_mu);
    if (!open(error)) {
        return false;
    }
    int64_t parent = 0;
    std::vector<DbValue> q;
    q.push_back(DbValue::fromText(cameraId));
    q.push_back(DbValue::fromInt(CalibValid));
    std::vector<std::vector<DbValue> > rows;
    if (g_db.query(
            "SELECT id FROM plate WHERE camera_id=?1 AND status=?2 ORDER BY id DESC LIMIT 1;",
            q, &rows, 0) && !rows.empty() && !rows[0].empty() && rows[0][0].kind == DbValue::Int) {
        parent = rows[0][0].i;
    }
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(model.updatedAt));
    a.push_back(DbValue::fromText(solutionDir));
    a.push_back(DbValue::fromText(cameraId));
    a.push_back(DbValue::fromText(cameraName));
    a.push_back(DbValue::fromInt(model.source));
    a.push_back(DbValue::fromInt(model.status));
    a.push_back(DbValue::fromInt(0));
    a.push_back(DbValue::fromInt(parent));
    a.push_back(DbValue::fromInt(model.fit));
    a.push_back(DbValue::fromText(num(model.rms)));
    a.push_back(DbValue::fromText(num(model.maxResidual)));
    a.push_back(DbValue::fromText(num(model.mmPerPixel)));
    for (int i = 0; i < 9; ++i) {
        a.push_back(DbValue::fromText(num(model.imageToWorld[i])));
    }
    for (int i = 0; i < 9; ++i) {
        a.push_back(DbValue::fromText(num(model.worldToImage[i])));
    }
    if (!g_db.exec(
            "INSERT INTO plane(time_iso,solution,camera_id,camera_name,source,status,applied,parent_plate_id,"
            "fit,rms,max_residual,mm_per_pixel,"
            "m0,m1,m2,m3,m4,m5,m6,m7,m8,w0,w1,w2,w3,w4,w5,w6,w7,w8) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,"
            "?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,?28,?29,?30);",
            a, error)) {
        return false;
    }
    const int64_t rowId = g_db.lastInsertId();
    if (id) {
        *id = rowId;
    }
    for (size_t i = 0; i < model.samples.size(); ++i) {
        std::vector<DbValue> p;
        p.push_back(DbValue::fromInt(rowId));
        p.push_back(DbValue::fromText(num(model.samples[i].imageX)));
        p.push_back(DbValue::fromText(num(model.samples[i].imageY)));
        p.push_back(DbValue::fromText(num(model.samples[i].worldX)));
        p.push_back(DbValue::fromText(num(model.samples[i].worldY)));
        p.push_back(DbValue::fromInt(model.samples[i].used ? 1 : 0));
        if (!g_db.exec(
                "INSERT INTO plane_point(plane_id,image_x,image_y,world_x,world_y,used) "
                "VALUES(?1,?2,?3,?4,?5,?6);",
                p, error)) {
            return false;
        }
    }
    return true;
}

bool CalibDb::recordRotate(const std::string& solutionDir, const std::string& cameraId,
                           const std::string& cameraName, const CalibParam& param,
                           const RotateCenter& model, int64_t* id, std::string* error)
{
    (void)param;
    std::lock_guard<std::mutex> lock(g_mu);
    if (!open(error)) {
        return false;
    }
    int64_t parent = 0;
    std::vector<DbValue> q;
    q.push_back(DbValue::fromText(cameraId));
    q.push_back(DbValue::fromInt(CalibValid));
    std::vector<std::vector<DbValue> > rows;
    if (g_db.query(
            "SELECT id FROM plane WHERE camera_id=?1 AND status=?2 ORDER BY id DESC LIMIT 1;",
            q, &rows, 0) && !rows.empty() && !rows[0].empty() && rows[0][0].kind == DbValue::Int) {
        parent = rows[0][0].i;
    }
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(model.updatedAt));
    a.push_back(DbValue::fromText(solutionDir));
    a.push_back(DbValue::fromText(cameraId));
    a.push_back(DbValue::fromText(cameraName));
    a.push_back(DbValue::fromInt(model.source));
    a.push_back(DbValue::fromInt(model.status));
    a.push_back(DbValue::fromInt(0));
    a.push_back(DbValue::fromInt(parent));
    a.push_back(DbValue::fromText(num(model.imageX)));
    a.push_back(DbValue::fromText(num(model.imageY)));
    a.push_back(DbValue::fromText(num(model.worldX)));
    a.push_back(DbValue::fromText(num(model.worldY)));
    a.push_back(DbValue::fromInt(model.worldValid ? 1 : 0));
    a.push_back(DbValue::fromText(num(model.radius)));
    a.push_back(DbValue::fromText(num(model.rms)));
    a.push_back(DbValue::fromText(num(model.maxResidual)));
    if (!g_db.exec(
            "INSERT INTO rotate(time_iso,solution,camera_id,camera_name,source,status,applied,parent_plane_id,"
            "image_x,image_y,world_x,world_y,world_valid,radius,rms,max_residual) "
            "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16);",
            a, error)) {
        return false;
    }
    const int64_t rowId = g_db.lastInsertId();
    if (id) {
        *id = rowId;
    }
    for (size_t i = 0; i < model.samples.size(); ++i) {
        std::vector<DbValue> p;
        p.push_back(DbValue::fromInt(rowId));
        p.push_back(DbValue::fromText(num(model.samples[i].angleDeg)));
        p.push_back(DbValue::fromText(num(model.samples[i].imageX)));
        p.push_back(DbValue::fromText(num(model.samples[i].imageY)));
        if (!g_db.exec(
                "INSERT INTO rotate_angle(rotate_id,angle_deg,image_x,image_y) VALUES(?1,?2,?3,?4);",
                p, error)) {
            return false;
        }
    }
    return true;
}

bool CalibDb::markApplied(const char* table, int64_t id)
{
    if (!table || id <= 0 || !TableRouter::isSafeIdent(table)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    std::vector<DbValue> a;
    a.push_back(DbValue::fromInt(id));
    return g_db.exec(std::string("UPDATE ") + table + " SET applied=1 WHERE id=?1;", a, 0);
}

} // namespace Vision
