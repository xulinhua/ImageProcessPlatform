/**
 * @file HsObjectIo.cpp
 * @brief HsObjectIo实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsObject.h>
#include <HsCommData/HsException.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

namespace HsCommData {

namespace {

void failObjectFile(const char* proc, HsError code)
{
    raiseException(HsException(proc, code));
}

void putBytesO(FILE* f, const void* p, size_t n, const char* proc)
{
    if (n == 0) {
        return;
    }
    if (fwrite(p, 1, n, f) != n) {
        failObjectFile(proc, HS_ERR_FILE_IO);
    }
}

void getBytesO(FILE* f, void* p, size_t n, const char* proc)
{
    if (n == 0) {
        return;
    }
    if (fread(p, 1, n, f) != n) {
        failObjectFile(proc, HS_ERR_FILE_IO);
    }
}

void putI32O(FILE* f, int32_t v, const char* proc) { putBytesO(f, &v, sizeof(v), proc); }
void putI64O(FILE* f, int64_t v, const char* proc) { putBytesO(f, &v, sizeof(v), proc); }
void putU8O(FILE* f, unsigned char v, const char* proc) { putBytesO(f, &v, 1, proc); }
void putF64O(FILE* f, double v, const char* proc) { putBytesO(f, &v, sizeof(v), proc); }
void putF32O(FILE* f, float v, const char* proc) { putBytesO(f, &v, sizeof(v), proc); }

int32_t getI32O(FILE* f, const char* proc)
{
    int32_t v = 0;
    getBytesO(f, &v, sizeof(v), proc);
    return v;
}

int64_t getI64O(FILE* f, const char* proc)
{
    int64_t v = 0;
    getBytesO(f, &v, sizeof(v), proc);
    return v;
}

unsigned char getU8O(FILE* f, const char* proc)
{
    unsigned char v = 0;
    getBytesO(f, &v, 1, proc);
    return v;
}

double getF64O(FILE* f, const char* proc)
{
    double v = 0.0;
    getBytesO(f, &v, sizeof(v), proc);
    return v;
}

float getF32O(FILE* f, const char* proc)
{
    float v = 0.0f;
    getBytesO(f, &v, sizeof(v), proc);
    return v;
}

void writePoint2(FILE* f, const Point2d& p, const char* proc)
{
    putF64O(f, p.x, proc);
    putF64O(f, p.y, proc);
}

Point2d readPoint2(FILE* f, const char* proc)
{
    Point2d p;
    p.x = getF64O(f, proc);
    p.y = getF64O(f, proc);
    return p;
}

void writeBox2(FILE* f, const Box2& b, const char* proc)
{
    putF64O(f, b.x, proc);
    putF64O(f, b.y, proc);
    putF64O(f, b.w, proc);
    putF64O(f, b.h, proc);
}

Box2 readBox2(FILE* f, const char* proc)
{
    Box2 b;
    b.x = getF64O(f, proc);
    b.y = getF64O(f, proc);
    b.w = getF64O(f, proc);
    b.h = getF64O(f, proc);
    return b;
}

void writeImage(FILE* f, const HsImage& img, const char* proc)
{
    putI64O(f, img.frameId, proc);
    putI32O(f, img.width, proc);
    putI32O(f, img.height, proc);
    putI32O(f, img.stride, proc);
    putI32O(f, static_cast<int32_t>(img.kind), proc);
    putI32O(f, static_cast<int32_t>(img.encoding), proc);
    if (img.pixels.size() > static_cast<size_t>(0x7fffffff)) {
        failObjectFile(proc, HS_ERR_FILE_IO);
    }
    putI32O(f, static_cast<int32_t>(img.pixels.size()), proc);
    if (!img.pixels.empty()) {
        putBytesO(f, img.pixels.data(), img.pixels.size(), proc);
    }
}

HsImage readImage(FILE* f, const char* proc)
{
    HsImage img;
    img.frameId = getI64O(f, proc);
    img.width = getI32O(f, proc);
    img.height = getI32O(f, proc);
    img.stride = getI32O(f, proc);
    img.kind = static_cast<PixelKind>(getI32O(f, proc));
    img.encoding = static_cast<PixelEncoding>(getI32O(f, proc));
    const int32_t n = getI32O(f, proc);
    if (n < 0) {
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
    }
    img.pixels.resize(static_cast<size_t>(n));
    if (n > 0) {
        getBytesO(f, img.pixels.data(), static_cast<size_t>(n), proc);
    }
    return img;
}

void writeRegion(FILE* f, const HsRegion& region, const char* proc)
{
    putU8O(f, region.isCompl ? 1 : 0, proc);
    putI32O(f, region.bounds.x, proc);
    putI32O(f, region.bounds.y, proc);
    putI32O(f, region.bounds.width, proc);
    putI32O(f, region.bounds.height, proc);
    if (region.runCount() > static_cast<size_t>(0x7fffffff)) {
        failObjectFile(proc, HS_ERR_FILE_IO);
    }
    putI32O(f, static_cast<int32_t>(region.runCount()), proc);
    for (size_t i = 0; i < region.runCount(); ++i) {
        putI32O(f, region.runAt(i).row, proc);
        putI32O(f, region.runAt(i).cb, proc);
        putI32O(f, region.runAt(i).ce, proc);
    }
}

HsRegion readRegion(FILE* f, const char* proc)
{
    HsRegion region;
    region.isCompl = getU8O(f, proc) != 0;
    region.bounds.x = getI32O(f, proc);
    region.bounds.y = getI32O(f, proc);
    region.bounds.width = getI32O(f, proc);
    region.bounds.height = getI32O(f, proc);
    const int32_t n = getI32O(f, proc);
    if (n < 0) {
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
    }
    region.resizeRuns(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        region.runAt(static_cast<size_t>(i)).row = getI32O(f, proc);
        region.runAt(static_cast<size_t>(i)).cb = getI32O(f, proc);
        region.runAt(static_cast<size_t>(i)).ce = getI32O(f, proc);
    }
    return region;
}

void writeContour(FILE* f, const HsContour& c, const char* proc)
{
    putU8O(f, c.closed ? 1 : 0, proc);
    putI32O(f, static_cast<int32_t>(c.ptCount()), proc);
    for (size_t i = 0; i < c.ptCount(); ++i) {
        writePoint2(f, c.ptAt(i), proc);
    }
    putI32O(f, static_cast<int32_t>(c.angle.size()), proc);
    for (size_t i = 0; i < c.angle.size(); ++i) {
        putF32O(f, c.angle[i], proc);
    }
    putI32O(f, static_cast<int32_t>(c.response.size()), proc);
    for (size_t i = 0; i < c.response.size(); ++i) {
        putF32O(f, c.response[i], proc);
    }
}

HsContour readContour(FILE* f, const char* proc)
{
    HsContour c;
    c.closed = getU8O(f, proc) != 0;
    const int32_t np = getI32O(f, proc);
    if (np < 0) {
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
    }
    c.resizePts(static_cast<size_t>(np));
    for (int32_t i = 0; i < np; ++i) {
        c.ptAt(static_cast<size_t>(i)) = readPoint2(f, proc);
    }
    const int32_t na = getI32O(f, proc);
    if (na < 0) {
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
    }
    c.angle.resize(static_cast<size_t>(na));
    for (int32_t i = 0; i < na; ++i) {
        c.angle[static_cast<size_t>(i)] = getF32O(f, proc);
    }
    const int32_t nr = getI32O(f, proc);
    if (nr < 0) {
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
    }
    c.response.resize(static_cast<size_t>(nr));
    for (int32_t i = 0; i < nr; ++i) {
        c.response[static_cast<size_t>(i)] = getF32O(f, proc);
    }
    return c;
}

void writePoly(FILE* f, const HsPoly& poly, const char* proc)
{
    putU8O(f, poly.closed ? 1 : 0, proc);
    putI32O(f, static_cast<int32_t>(poly.segCount()), proc);
    for (size_t i = 0; i < poly.segCount(); ++i) {
        writePoint2(f, poly.segAt(i).pt, proc);
        putI32O(f, poly.segAt(i).first, proc);
        putI32O(f, poly.segAt(i).last, proc);
    }
}

HsPoly readPoly(FILE* f, const char* proc)
{
    HsPoly poly;
    poly.closed = getU8O(f, proc) != 0;
    const int32_t n = getI32O(f, proc);
    if (n < 0) {
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
    }
    poly.resizeSegs(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        poly.segAt(static_cast<size_t>(i)).pt = readPoint2(f, proc);
        poly.segAt(static_cast<size_t>(i)).first = getI32O(f, proc);
        poly.segAt(static_cast<size_t>(i)).last = getI32O(f, proc);
    }
    return poly;
}

void writeOne(FILE* f, const HsObject& object, int index, const char* proc)
{
    const ObjectKind kind = object.kindAt(index);
    putI32O(f, static_cast<int32_t>(kind), proc);
    switch (kind) {
    case ObjImage:
        writeImage(f, *object.imageAt(index), proc);
        break;
    case ObjRegion:
        writeRegion(f, *object.regionAt(index), proc);
        break;
    case ObjContour:
        writeContour(f, *object.contourAt(index), proc);
        break;
    case ObjPoly:
        writePoly(f, *object.polyAt(index), proc);
        break;
    case ObjCircle: {
        const CircleP* c = object.circleAt(index);
        writePoint2(f, c->c, proc);
        putF64O(f, c->r, proc);
        break;
    }
    case ObjEllipse: {
        const EllipseP* e = object.ellipseAt(index);
        writePoint2(f, e->c, proc);
        putF64O(f, e->ra, proc);
        putF64O(f, e->rb, proc);
        putF64O(f, e->phi, proc);
        break;
    }
    case ObjRect1: {
        const Rect1P* r = object.rect1At(index);
        writeBox2(f, r->r, proc);
        break;
    }
    case ObjRect2: {
        const Rect2P* r = object.rect2At(index);
        writePoint2(f, r->c, proc);
        putF64O(f, r->l1, proc);
        putF64O(f, r->l2, proc);
        putF64O(f, r->phi, proc);
        break;
    }
    case ObjLine: {
        const LineP* line = object.lineAt(index);
        writePoint2(f, line->a, proc);
        writePoint2(f, line->b, proc);
        break;
    }
    default:
        failObjectFile(proc, HS_ERR_TYPE);
        break;
    }
}

void readOne(FILE* f, HsObject* object, const char* proc)
{
    const ObjectKind kind = static_cast<ObjectKind>(getI32O(f, proc));
    switch (kind) {
    case ObjImage:
        object->append(HsObject::fromImage(readImage(f, proc)));
        break;
    case ObjRegion:
        object->append(HsObject::fromRegion(readRegion(f, proc)));
        break;
    case ObjContour:
        object->append(HsObject::fromContour(readContour(f, proc)));
        break;
    case ObjPoly:
        object->append(HsObject::fromPoly(readPoly(f, proc)));
        break;
    case ObjCircle: {
        CircleP c;
        c.c = readPoint2(f, proc);
        c.r = getF64O(f, proc);
        object->append(HsObject::fromCircle(c));
        break;
    }
    case ObjEllipse: {
        EllipseP e;
        e.c = readPoint2(f, proc);
        e.ra = getF64O(f, proc);
        e.rb = getF64O(f, proc);
        e.phi = getF64O(f, proc);
        object->append(HsObject::fromEllipse(e));
        break;
    }
    case ObjRect1: {
        Rect1P r;
        r.r = readBox2(f, proc);
        object->append(HsObject::fromRect1(r));
        break;
    }
    case ObjRect2: {
        Rect2P r;
        r.c = readPoint2(f, proc);
        r.l1 = getF64O(f, proc);
        r.l2 = getF64O(f, proc);
        r.phi = getF64O(f, proc);
        object->append(HsObject::fromRect2(r));
        break;
    }
    case ObjLine: {
        LineP line;
        line.a = readPoint2(f, proc);
        line.b = readPoint2(f, proc);
        object->append(HsObject::fromLine(line));
        break;
    }
    default:
        failObjectFile(proc, HS_ERR_FILE_FORMAT);
        break;
    }
}

}

// 写入本地图标对象文件
void HsObject::writeObject(const char* fileName) const
{
    const char* proc = "writeObject";
    if (!fileName || !fileName[0]) {
        failObjectFile(proc, HS_ERR_FILE_OPEN);
    }
    FILE* f = fopen(fileName, "wb");
    if (!f) {
        failObjectFile(proc, HS_ERR_FILE_OPEN);
    }
    try {
        putBytesO(f, "HSOB", 4, proc);
        putI32O(f, 1, proc);
        const int n = count();
        putI32O(f, n, proc);
        for (int i = 0; i < n; ++i) {
            writeOne(f, *this, i, proc);
        }
    } catch (...) {
        fclose(f);
        throw;
    }
    if (fclose(f) != 0) {
        failObjectFile(proc, HS_ERR_FILE_IO);
    }
}

// 从本地文件读入图标对象
void HsObject::readObject(const char* fileName)
{
    const char* proc = "readObject";
    if (!fileName || !fileName[0]) {
        failObjectFile(proc, HS_ERR_FILE_OPEN);
    }
    FILE* f = fopen(fileName, "rb");
    if (!f) {
        failObjectFile(proc, HS_ERR_FILE_OPEN);
    }
    try {
        char magic[4];
        getBytesO(f, magic, 4, proc);
        if (std::memcmp(magic, "HSOB", 4) != 0) {
            failObjectFile(proc, HS_ERR_FILE_FORMAT);
        }
        const int32_t ver = getI32O(f, proc);
        if (ver != 1) {
            failObjectFile(proc, HS_ERR_FILE_FORMAT);
        }
        const int32_t n = getI32O(f, proc);
        if (n < 0) {
            failObjectFile(proc, HS_ERR_FILE_FORMAT);
        }
        clear();
        for (int32_t i = 0; i < n; ++i) {
            readOne(f, this, proc);
        }
    } catch (...) {
        fclose(f);
        throw;
    }
    if (fclose(f) != 0) {
        failObjectFile(proc, HS_ERR_FILE_IO);
    }
}

// 自由函数转调成员
void writeObject(const HsObject& object, const char* fileName)
{
    object.writeObject(fileName);
}

// 自由函数转调成员。空指针抛类型错
void readObject(HsObject* object, const char* fileName)
{
    if (!object) {
        failObjectFile("readObject", HS_ERR_TYPE);
    }
    object->readObject(fileName);
}

}
