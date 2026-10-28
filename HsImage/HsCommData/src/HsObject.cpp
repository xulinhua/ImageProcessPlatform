/**
 * @file HsObject.cpp
 * @brief HsObject实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include <HsCommData/HsObject.h>

#include <mutex>
#include <climits>
#include <vector>

namespace HsCommData {

namespace {

// 句柄析构时释放联合体
struct ObjectUnionDel {
    void operator()(ObjectUnion* p) const
    {
        if (p) {
            delete p;
        }
    }
};

// 新建一份独立句柄
std::shared_ptr<ObjectUnion> makeUnion()
{
    return std::shared_ptr<ObjectUnion>(new ObjectUnion(), ObjectUnionDel());
}

// 深拷一份句柄
std::shared_ptr<ObjectUnion> copyUnion(const ObjectUnion& src)
{
    return std::shared_ptr<ObjectUnion>(new ObjectUnion(src), ObjectUnionDel());
}

// 下标或种类不对就抛
const ObjectUnion* needKind(const ObjectUnion* u, ObjectKind want)
{
    if (!u) {
        raiseException(HsException(HS_ERR_INDEX));
    }
    if (u->kind() != want) {
        raiseException(HsException(HS_ERR_TYPE));
    }
    return u;
}

// 下标或种类不对就抛
ObjectUnion* needKind(ObjectUnion* u, ObjectKind want)
{
    if (!u) {
        raiseException(HsException(HS_ERR_INDEX));
    }
    if (u->kind() != want) {
        raiseException(HsException(HS_ERR_TYPE));
    }
    return u;
}

} // namespace

// 空联合，指针清零
ObjectUnion::ObjectUnion()
    : m_kind(ObjEmpty)
{
    m_slot.image = nullptr;
}

// 释放当前那一种
ObjectUnion::~ObjectUnion()
{
    reset();
}

// 深拷当前种类
ObjectUnion::ObjectUnion(const ObjectUnion& other)
    : m_kind(ObjEmpty)
{
    m_slot.image = nullptr;
    switch (other.m_kind) {
    case ObjImage:
        setImage(*other.m_slot.image);
        break;
    case ObjRegion:
        setRegion(*other.m_slot.region);
        break;
    case ObjContour:
        setContour(*other.m_slot.contour);
        break;
    case ObjPoly:
        setPoly(*other.m_slot.poly);
        break;
    case ObjCircle:
        setCircle(*other.m_slot.circle);
        break;
    case ObjEllipse:
        setEllipse(*other.m_slot.ellipse);
        break;
    case ObjRect1:
        setRect1(*other.m_slot.rect1);
        break;
    case ObjRect2:
        setRect2(*other.m_slot.rect2);
        break;
    case ObjLine:
        setLine(*other.m_slot.line);
        break;
    default:
        break;
    }
}

// 先清再拷
ObjectUnion& ObjectUnion::operator=(const ObjectUnion& other)
{
    if (this != &other) {
        reset();
        ObjectUnion tmp(other);
        m_kind = tmp.m_kind;
        m_slot = tmp.m_slot;
        tmp.m_kind = ObjEmpty;
        tmp.m_slot.image = nullptr;
    }
    return *this;
}

// 当前种类
ObjectKind ObjectUnion::kind() const
{
    return m_kind;
}

// 释放当前指针
void ObjectUnion::reset()
{
    switch (m_kind) {
    case ObjImage:
        if (m_slot.image) {
            delete m_slot.image;
        }
        break;
    case ObjRegion:
        if (m_slot.region) {
            delete m_slot.region;
        }
        break;
    case ObjContour:
        if (m_slot.contour) {
            delete m_slot.contour;
        }
        break;
    case ObjPoly:
        if (m_slot.poly) {
            delete m_slot.poly;
        }
        break;
    case ObjCircle:
        if (m_slot.circle) {
            delete m_slot.circle;
        }
        break;
    case ObjEllipse:
        if (m_slot.ellipse) {
            delete m_slot.ellipse;
        }
        break;
    case ObjRect1:
        if (m_slot.rect1) {
            delete m_slot.rect1;
        }
        break;
    case ObjRect2:
        if (m_slot.rect2) {
            delete m_slot.rect2;
        }
        break;
    case ObjLine:
        if (m_slot.line) {
            delete m_slot.line;
        }
        break;
    default:
        break;
    }
    m_kind = ObjEmpty;
    m_slot.image = nullptr;
}

// 换成图像
void ObjectUnion::setImage(const HsImage& value)
{
    reset();
    m_slot.image = new HsImage(value);
    m_kind = ObjImage;
}

// 换成区域
void ObjectUnion::setRegion(const HsRegion& value)
{
    reset();
    m_slot.region = new HsRegion(value);
    m_kind = ObjRegion;
}

// 换成轮廓
void ObjectUnion::setContour(const HsContour& value)
{
    reset();
    m_slot.contour = new HsContour(value);
    m_kind = ObjContour;
}

// 换成折线逼近
void ObjectUnion::setPoly(const HsPoly& value)
{
    reset();
    m_slot.poly = new HsPoly(value);
    m_kind = ObjPoly;
}

// 换成圆
void ObjectUnion::setCircle(const CircleP& value)
{
    reset();
    m_slot.circle = new CircleP(value);
    m_kind = ObjCircle;
}

// 换成椭圆
void ObjectUnion::setEllipse(const EllipseP& value)
{
    reset();
    m_slot.ellipse = new EllipseP(value);
    m_kind = ObjEllipse;
}

// 换成轴对齐矩形
void ObjectUnion::setRect1(const Rect1P& value)
{
    reset();
    m_slot.rect1 = new Rect1P(value);
    m_kind = ObjRect1;
}

// 换成旋转矩形
void ObjectUnion::setRect2(const Rect2P& value)
{
    reset();
    m_slot.rect2 = new Rect2P(value);
    m_kind = ObjRect2;
}

// 换成线段
void ObjectUnion::setLine(const LineP& value)
{
    reset();
    m_slot.line = new LineP(value);
    m_kind = ObjLine;
}

// 空对象
HsObject::HsObject()
{
}

// 同类型拷贝，共享句柄。锁住源对象再复制指针
HsObject::HsObject(const HsObject& other)
{
    std::lock_guard<std::recursive_mutex> lock(other.m_guard);
    m_items = other.m_items;
}

// 同类型赋值，共享句柄。按地址顺序加锁，避免互相等待
HsObject& HsObject::operator=(const HsObject& other)
{
    if (this == &other) {
        return *this;
    }
    std::unique_lock<std::recursive_mutex> a(m_guard, std::defer_lock);
    std::unique_lock<std::recursive_mutex> b(other.m_guard, std::defer_lock);
    if (this < &other) {
        std::lock(a, b);
    } else {
        std::lock(b, a);
    }
    m_items = other.m_items;
    return *this;
}

// 由这一个图像构造
HsObject::HsObject(const HsImage& value)
{
    *this = fromImage(value);
}

// 由这一个区域构造
HsObject::HsObject(const HsRegion& value)
{
    *this = fromRegion(value);
}

// 由这一条轮廓构造
HsObject::HsObject(const HsContour& value)
{
    *this = fromContour(value);
}

// 由这一条折线逼近构造
HsObject::HsObject(const HsPoly& value)
{
    *this = fromPoly(value);
}

// 由这一个圆构造
HsObject::HsObject(const CircleP& value)
{
    *this = fromCircle(value);
}

// 由这一个椭圆构造
HsObject::HsObject(const EllipseP& value)
{
    *this = fromEllipse(value);
}

// 由这一个轴对齐矩形构造
HsObject::HsObject(const Rect1P& value)
{
    *this = fromRect1(value);
}

// 由这一个旋转矩形构造
HsObject::HsObject(const Rect2P& value)
{
    *this = fromRect2(value);
}

// 由这一条线段构造
HsObject::HsObject(const LineP& value)
{
    *this = fromLine(value);
}

// 换成这一个图像
HsObject& HsObject::operator=(const HsImage& value)
{
    *this = fromImage(value);
    return *this;
}

// 换成这一个区域
HsObject& HsObject::operator=(const HsRegion& value)
{
    *this = fromRegion(value);
    return *this;
}

// 换成这一条轮廓
HsObject& HsObject::operator=(const HsContour& value)
{
    *this = fromContour(value);
    return *this;
}

// 换成这一条折线逼近
HsObject& HsObject::operator=(const HsPoly& value)
{
    *this = fromPoly(value);
    return *this;
}

// 换成这一个圆
HsObject& HsObject::operator=(const CircleP& value)
{
    *this = fromCircle(value);
    return *this;
}

// 换成这一个椭圆
HsObject& HsObject::operator=(const EllipseP& value)
{
    *this = fromEllipse(value);
    return *this;
}

// 换成这一个轴对齐矩形
HsObject& HsObject::operator=(const Rect1P& value)
{
    *this = fromRect1(value);
    return *this;
}

// 换成这一个旋转矩形
HsObject& HsObject::operator=(const Rect2P& value)
{
    *this = fromRect2(value);
    return *this;
}

// 换成这一条线段
HsObject& HsObject::operator=(const LineP& value)
{
    *this = fromLine(value);
    return *this;
}

// 没有元素
bool HsObject::isEmpty() const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    return !m_items || m_items->list.empty();
}

// 至少有一个元素
bool HsObject::isInitialized() const
{
    return !isEmpty();
}

// 元素个数
int HsObject::count() const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    if (!m_items) {
        return 0;
    }
    return static_cast<int>(m_items->list.size());
}

// 全体种类
ObjectKind HsObject::kind() const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    if (!m_items || m_items->list.empty()) {
        return ObjEmpty;
    }
    const ObjectKind first = m_items->list[0]->kind();
    for (size_t i = 1; i < m_items->list.size(); ++i) {
        if (m_items->list[i]->kind() != first) {
            return ObjMixed;
        }
    }
    return first;
}

// 单个种类。越界抛异常
ObjectKind HsObject::kindAt(int index) const
{
    const ObjectUnion* u = item(index);
    if (!u) {
        raiseException(HsException(HS_ERR_INDEX));
    }
    return u->kind();
}

// 释放句柄
void HsObject::clear()
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    m_items.reset();
}

// 深拷每个元素
HsObject HsObject::clone() const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    HsObject out;
    if (!m_items || m_items->list.empty()) {
        return out;
    }
    out.ensure();
    out.m_items->list.reserve(m_items->list.size());
    for (size_t i = 0; i < m_items->list.size(); ++i) {
        out.m_items->list.push_back(copyUnion(*m_items->list[i]));
    }
    return out;
}

// 接上另一元组的句柄。两把句柄锁按地址顺序获取
void HsObject::append(const HsObject& other)
{
    if (this == &other) {
        std::lock_guard<std::recursive_mutex> guard(m_guard);
        std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
        if (!m_items || m_items->list.empty()) {
            return;
        }
        std::vector<std::shared_ptr<ObjectUnion>> extra = m_items->list;
        detach();
        m_items->list.insert(m_items->list.end(), extra.begin(), extra.end());
        return;
    }
    std::unique_lock<std::recursive_mutex> a(m_guard, std::defer_lock);
    std::unique_lock<std::recursive_mutex> b(other.m_guard, std::defer_lock);
    if (this < &other) {
        std::lock(a, b);
    } else {
        std::lock(b, a);
    }
    if (!other.m_items || other.m_items->list.empty()) {
        return;
    }
    std::unique_lock<std::recursive_mutex> otherItems(other.m_items->mu);
    std::vector<std::shared_ptr<ObjectUnion>> extra = other.m_items->list;
    otherItems.unlock();
    ensure();
    detach();
    m_items->list.insert(m_items->list.end(), extra.begin(), extra.end());
}

// 取出一个，共享该元素。越界抛异常
HsObject HsObject::select(int index) const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    const int n = m_items ? static_cast<int>(m_items->list.size()) : 0;
    if (index < 0) {
        index += n;
    }
    if (!m_items || index < 0 || index >= n) {
        raiseException(HsException(HS_ERR_INDEX));
        return HsObject();
    }
    HsObject out;
    out.ensure();
    out.m_items->list.push_back(m_items->list[static_cast<size_t>(index)]);
    return out;
}

// 取出半开区间。越界抛异常，空区间仍返回空对象
HsObject HsObject::slice(int begin, int end) const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    const int n = m_items ? static_cast<int>(m_items->list.size()) : 0;
    if (begin < 0 || end < begin || end > n) {
        raiseException(HsException(HS_ERR_INDEX));
        return HsObject();
    }
    HsObject out;
    if (begin == end) {
        return out;
    }
    out.ensure();
    out.m_items->list.insert(out.m_items->list.end(),
                             m_items->list.begin() + begin,
                             m_items->list.begin() + end);
    return out;
}

// 单个图像
HsObject HsObject::fromImage(const HsImage& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setImage(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个区域
HsObject HsObject::fromRegion(const HsRegion& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setRegion(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个轮廓
HsObject HsObject::fromContour(const HsContour& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setContour(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个折线逼近
HsObject HsObject::fromPoly(const HsPoly& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setPoly(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个圆
HsObject HsObject::fromCircle(const CircleP& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setCircle(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个椭圆
HsObject HsObject::fromEllipse(const EllipseP& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setEllipse(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个轴对齐矩形
HsObject HsObject::fromRect1(const Rect1P& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setRect1(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个旋转矩形
HsObject HsObject::fromRect2(const Rect2P& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setRect2(value);
    out.m_items->list.push_back(u);
    return out;
}

// 单个线段
HsObject HsObject::fromLine(const LineP& value)
{
    HsObject out;
    out.ensure();
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setLine(value);
    out.m_items->list.push_back(u);
    return out;
}

// 只读图像。种类或下标不对抛异常
const HsImage* HsObject::imageAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjImage);
    return u->m_slot.image;
}

// 只读区域。种类或下标不对抛异常
const HsRegion* HsObject::regionAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjRegion);
    return u->m_slot.region;
}

// 只读轮廓。种类或下标不对抛异常
const HsContour* HsObject::contourAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjContour);
    return u->m_slot.contour;
}

// 只读折线逼近。种类或下标不对抛异常
const HsPoly* HsObject::polyAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjPoly);
    return u->m_slot.poly;
}

// 只读圆。种类或下标不对抛异常
const CircleP* HsObject::circleAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjCircle);
    return u->m_slot.circle;
}

// 只读椭圆。种类或下标不对抛异常
const EllipseP* HsObject::ellipseAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjEllipse);
    return u->m_slot.ellipse;
}

// 只读轴对齐矩形。种类或下标不对抛异常
const Rect1P* HsObject::rect1At(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjRect1);
    return u->m_slot.rect1;
}

// 只读旋转矩形。种类或下标不对抛异常
const Rect2P* HsObject::rect2At(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjRect2);
    return u->m_slot.rect2;
}

// 只读线段。种类或下标不对抛异常
const LineP* HsObject::lineAt(int index) const
{
    const ObjectUnion* u = needKind(item(index), ObjLine);
    return u->m_slot.line;
}

// 可改图像。种类或下标不对抛异常
HsImage* HsObject::imageAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjImage);
    return u->m_slot.image;
}

// 可改区域。种类或下标不对抛异常
HsRegion* HsObject::regionAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjRegion);
    return u->m_slot.region;
}

// 可改轮廓。种类或下标不对抛异常
HsContour* HsObject::contourAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjContour);
    return u->m_slot.contour;
}

// 可改折线逼近。种类或下标不对抛异常
HsPoly* HsObject::polyAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjPoly);
    return u->m_slot.poly;
}

// 可改圆。种类或下标不对抛异常
CircleP* HsObject::circleAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjCircle);
    return u->m_slot.circle;
}

// 可改椭圆。种类或下标不对抛异常
EllipseP* HsObject::ellipseAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjEllipse);
    return u->m_slot.ellipse;
}

// 可改轴对齐矩形。种类或下标不对抛异常
Rect1P* HsObject::rect1At(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjRect1);
    return u->m_slot.rect1;
}

// 可改旋转矩形。种类或下标不对抛异常
Rect2P* HsObject::rect2At(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjRect2);
    return u->m_slot.rect2;
}

// 可改线段。种类或下标不对抛异常
LineP* HsObject::lineAt(int index)
{
    ObjectUnion* u = needKind(itemWrite(index), ObjLine);
    return u->m_slot.line;
}

// 保证元组缓冲存在
void HsObject::ensure()
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    if (!m_items) {
        m_items = std::make_shared<Store>();
    }
}

// 锁住共享元组。调用方已持有 m_guard 时也可再锁
std::unique_lock<std::recursive_mutex> HsObject::lockItems() const
{
    if (!m_items) {
        return std::unique_lock<std::recursive_mutex>();
    }
    return std::unique_lock<std::recursive_mutex>(m_items->mu);
}

// 元组被共享时复制句柄数组
void HsObject::detach()
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    if (!m_items || m_items.use_count() == 1) {
        return;
    }
    std::lock_guard<std::recursive_mutex> body(m_items->mu);
    if (m_items.use_count() == 1) {
        return;
    }
    std::shared_ptr<Store> neu = std::make_shared<Store>();
    neu->list = m_items->list;
    m_items = neu;
}

// 只读元素
const ObjectUnion* HsObject::item(int index) const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> body = lockItems();
    const int n = m_items ? static_cast<int>(m_items->list.size()) : 0;
    if (index < 0) {
        index += n;
    }
    if (!m_items || index < 0 || index >= n) {
        return 0;
    }
    return m_items->list[static_cast<size_t>(index)].get();
}

// 可改元素。共享元组或共享元素时先复制
ObjectUnion* HsObject::itemWrite(int index)
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    if (!m_items) {
        return 0;
    }
    std::lock_guard<std::recursive_mutex> body(m_items->mu);
    const int n = static_cast<int>(m_items->list.size());
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index >= n) {
        return 0;
    }
    if (m_items.use_count() > 1) {
        std::shared_ptr<Store> neu = std::make_shared<Store>();
        neu->list = m_items->list;
        m_items = neu;
    }
    std::shared_ptr<ObjectUnion>& sp = m_items->list[static_cast<size_t>(index)];
    if (!sp) {
        return 0;
    }
    if (sp.use_count() > 1) {
        sp = copyUnion(*sp);
    }
    return sp.get();
}


// 持锁拆开后解析下标。越界则不改表
bool HsObject::lockedIndex(int index, bool allowEnd, int* out)
{
    ensure();
    detach();
    const int n = m_items ? static_cast<int>(m_items->list.size()) : 0;
    if (index < 0) {
        index += n;
    }
    const bool ok = allowEnd ? (index >= 0 && index <= n) : (index >= 0 && index < n);
    if (!ok || !out) {
        raiseException(HsException(HS_ERR_INDEX));
        return false;
    }
    *out = index;
    return true;
}

// 负下标收到 [0, count())
int HsObject::resolveIndex(int index) const
{
    const int n = count();
    if (index < 0) {
        index += n;
    }
    if (index < 0 || index >= n) {
        raiseException(HsException(HS_ERR_INDEX));
    }
    return index;
}



// 元组里的下标。越界则按系统异常处理并返回 false
static bool readObjIndex(const HsTuple& indices, int i, int bound, int* out)
{
    int64_t v = 0;
    if (!indices.getLong(i, &v)) {
        raiseException(HsException(HS_ERR_TYPE));
        return false;
    }
    if (v < INT_MIN || v > INT_MAX) {
        raiseException(HsException(HS_ERR_INT_RANGE));
        return false;
    }
    int idx = static_cast<int>(v);
    if (idx < 0) {
        idx += bound;
    }
    if (idx < 0 || idx >= bound || !out) {
        raiseException(HsException(HS_ERR_INDEX));
        return false;
    }
    *out = idx;
    return true;
}

HsObject HsObject::select(const HsTuple& indices) const
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    std::unique_lock<std::recursive_mutex> itemsLock = lockItems();
    const int n = m_items ? static_cast<int>(m_items->list.size()) : 0;
    HsObject out;
    const int m = indices.length();
    if (m == 0) {
        return out;
    }
    out.ensure();
    for (int i = 0; i < m; ++i) {
        int idx = 0;
        if (!readObjIndex(indices, i, n, &idx)) {
            return out;
        }
        out.m_items->list.push_back(m_items->list[static_cast<size_t>(idx)]);
    }
    return out;
}

// 持锁拆开后再按当前表长检查下标
void HsObject::insert(int index, const HsObject& other)
{
    if (&other == this) {
        HsObject copy = other;
        insert(index, copy);
        return;
    }
    std::unique_lock<std::recursive_mutex> a(m_guard, std::defer_lock);
    std::unique_lock<std::recursive_mutex> b(other.m_guard, std::defer_lock);
    if (this < &other) {
        std::lock(a, b);
    } else {
        std::lock(b, a);
    }
    std::vector<std::shared_ptr<ObjectUnion>> extra;
    if (other.m_items && !other.m_items->list.empty()) {
        std::lock_guard<std::recursive_mutex> otherItems(other.m_items->mu);
        extra = other.m_items->list;
    }
    int at = 0;
    if (!lockedIndex(index, true, &at)) {
        return;
    }
    m_items->list.insert(m_items->list.begin() + at, extra.begin(), extra.end());
}

// 持锁拆开后再按当前表长检查下标
void HsObject::replace(int index, const HsObject& other)
{
    if (&other == this) {
        HsObject copy = other;
        replace(index, copy);
        return;
    }
    std::unique_lock<std::recursive_mutex> a(m_guard, std::defer_lock);
    std::unique_lock<std::recursive_mutex> b(other.m_guard, std::defer_lock);
    if (this < &other) {
        std::lock(a, b);
    } else {
        std::lock(b, a);
    }
    std::vector<std::shared_ptr<ObjectUnion>> extra;
    if (other.m_items && !other.m_items->list.empty()) {
        std::lock_guard<std::recursive_mutex> otherItems(other.m_items->mu);
        extra = other.m_items->list;
    }
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list.erase(m_items->list.begin() + at);
    m_items->list.insert(m_items->list.begin() + at, extra.begin(), extra.end());
}

// 持锁拆开后再按当前表长检查下标
void HsObject::remove(int index)
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list.erase(m_items->list.begin() + at);
}

// 按元组下标删除。持锁拆开后再做掩码
void HsObject::remove(const HsTuple& indices)
{
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    if (!m_items) {
        if (indices.length() > 0) {
            raiseException(HsException(HS_ERR_INDEX));
        }
        return;
    }
    detach();
    const int n = static_cast<int>(m_items->list.size());
    std::vector<char> drop(static_cast<size_t>(n), 0);
    const int m = indices.length();
    for (int i = 0; i < m; ++i) {
        int idx = 0;
        if (!readObjIndex(indices, i, n, &idx)) {
            return;
        }
        drop[static_cast<size_t>(idx)] = 1;
    }
    std::vector<std::shared_ptr<ObjectUnion>> kept;
    kept.reserve(m_items->list.size());
    for (int i = 0; i < n; ++i) {
        if (!drop[static_cast<size_t>(i)]) {
            kept.push_back(m_items->list[static_cast<size_t>(i)]);
        }
    }
    m_items->list.swap(kept);
}

// 持锁拆开后再换上这一格
void HsObject::setImage(int index, const HsImage& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setImage(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setRegion(int index, const HsRegion& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setRegion(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setContour(int index, const HsContour& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setContour(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setPoly(int index, const HsPoly& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setPoly(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setCircle(int index, const CircleP& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setCircle(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setEllipse(int index, const EllipseP& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setEllipse(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setRect1(int index, const Rect1P& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setRect1(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setRect2(int index, const Rect2P& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setRect2(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

// 持锁拆开后再换上这一格
void HsObject::setLine(int index, const LineP& value)
{
    std::shared_ptr<ObjectUnion> u = makeUnion();
    u->setLine(value);
    std::lock_guard<std::recursive_mutex> guard(m_guard);
    int at = 0;
    if (!lockedIndex(index, false, &at)) {
        return;
    }
    m_items->list[static_cast<size_t>(at)] = u;
}

}
