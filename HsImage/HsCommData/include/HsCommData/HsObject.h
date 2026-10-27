/**
 * @file HsObject.h
 * @brief HsObject头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSCOMMDATA_HSOBJECT_H
#define HSCOMMDATA_HSOBJECT_H

#include <HsCommData/HsContour.h>
#include <HsCommData/HsImage.h>
#include <HsCommData/HsPoly.h>
#include <HsCommData/HsRegion.h>
#include <HsCommData/Shape.h>
#include <HsCommData/HsTuple.h>

#include <memory>
#include <mutex>
#include <vector>

namespace HsCommData {

/** @brief 对象元素种类。HsObject 内一格或整组统一时的对外种类 */
enum ObjectKind {
    ObjUndefined = -1, ///< 未定义
    ObjEmpty = 0, ///< 空
    ObjMixed,     ///< 多种类混放
    ObjImage,     ///< 图像
    ObjRegion,    ///< 区域
    ObjContour,   ///< 轮廓
    ObjPoly,      ///< 多边形
    ObjCircle,    ///< 圆
    ObjEllipse,   ///< 椭圆
    ObjRect1,     ///< 轴对齐矩形
    ObjRect2,     ///< 旋转矩形
    ObjLine,       ///< 线段
    ObjKindMax ///< 个数，不是合法值
};

/**
 * @class ObjectUnion
 * @brief 单格对象存储。同一时刻只有一种有效，由 kind 区分
 */
class HSCOMMDATA_EXPORT ObjectUnion {
    friend class HsObject;

public:
    ObjectUnion(); ///< 空
    ~ObjectUnion();
    ObjectUnion(const ObjectUnion& other); ///< 深拷当前种类
    ObjectUnion& operator=(const ObjectUnion& other);

    /** @brief 当前种类 */
    ObjectKind kind() const;

private:
    /** @brief 负下标收到 [0, count())。非法则抛 */
    int resolveIndex(int index) const;
    /** @brief 释放并置空 */
    void reset();
    /** @brief 换成图像，释放旧内容 */
    void setImage(const HsImage& value);
    /** @brief 换成区域 */
    void setRegion(const HsRegion& value);
    /** @brief 换成轮廓 */
    void setContour(const HsContour& value);
    /** @brief 换成多边形 */
    void setPoly(const HsPoly& value);
    /** @brief 换成圆 */
    void setCircle(const CircleP& value);
    /** @brief 换成椭圆 */
    void setEllipse(const EllipseP& value);
    /** @brief 换成轴对齐矩形 */
    void setRect1(const Rect1P& value);
    /** @brief 换成旋转矩形 */
    void setRect2(const Rect2P& value);
    /** @brief 换成线段 */
    void setLine(const LineP& value);

    ObjectKind m_kind; ///< 当前种类
    union {
        HsImage* image;     ///< ObjImage
        HsRegion* region;   ///< ObjRegion
        HsContour* contour; ///< ObjContour
        HsPoly* poly;       ///< ObjPoly
        CircleP* circle;    ///< ObjCircle
        EllipseP* ellipse;  ///< ObjEllipse
        Rect1P* rect1;      ///< ObjRect1
        Rect2P* rect2;      ///< ObjRect2
        LineP* line;        ///< ObjLine
    } m_slot;
};

/**
 * @class HsObject
 * @brief 图形对象容器。拷贝共享缓冲，clone 才深拷
 * 下标从 0 起，可用负下标从末尾数。混放时 kind 为 ObjMixed，逐格用 kindAt
 * 写入会拆开共享，替换一格时其它格仍可共享只读缓冲
 */
class HSCOMMDATA_EXPORT HsObject {
public:
    HsObject(); ///< 空，未分配
    /** @brief 共享缓冲拷贝 */
    HsObject(const HsObject& other);
    /** @brief 共享缓冲赋值。深拷用 clone */
    HsObject& operator=(const HsObject& other);

    /** @brief 单元素图像对象 */
    HsObject(const HsImage& value);
    /** @brief 单元素区域对象 */
    HsObject(const HsRegion& value);
    /** @brief 单元素轮廓对象 */
    HsObject(const HsContour& value);
    /** @brief 单元素多边形对象 */
    HsObject(const HsPoly& value);
    /** @brief 单元素圆对象 */
    HsObject(const CircleP& value);
    /** @brief 单元素椭圆对象 */
    HsObject(const EllipseP& value);
    /** @brief 单元素轴对齐矩形对象 */
    HsObject(const Rect1P& value);
    /** @brief 单元素旋转矩形对象 */
    HsObject(const Rect2P& value);
    /** @brief 单元素线段对象 */
    HsObject(const LineP& value);

    /** @brief 换成单元素图像，不追加 */
    HsObject& operator=(const HsImage& value);
    /** @brief 换成单元素区域，不追加 */
    HsObject& operator=(const HsRegion& value);
    /** @brief 换成单元素轮廓，不追加 */
    HsObject& operator=(const HsContour& value);
    /** @brief 换成单元素多边形，不追加 */
    HsObject& operator=(const HsPoly& value);
    /** @brief 换成单元素圆，不追加 */
    HsObject& operator=(const CircleP& value);
    /** @brief 换成单元素椭圆，不追加 */
    HsObject& operator=(const EllipseP& value);
    /** @brief 换成单元素轴对齐矩形，不追加 */
    HsObject& operator=(const Rect1P& value);
    /** @brief 换成单元素旋转矩形，不追加 */
    HsObject& operator=(const Rect2P& value);
    /** @brief 换成单元素线段，不追加 */
    HsObject& operator=(const LineP& value);

    /** @brief 无元素 */
    bool isEmpty() const;
    /** @brief 已分配且非空 */
    bool isInitialized() const;
    /** @brief 元素个数 */
    int count() const;
    /**
     * @brief 整体种类。空为 ObjEmpty，混放为 ObjMixed
     */
    ObjectKind kind() const;
    /** @brief 第 index 格种类。越界抛 HsException */
    ObjectKind kindAt(int index) const;

    /** @brief 清空全部元素 */
    void clear();
    /** @brief 深拷全部元素，不共享缓冲 */
    HsObject clone() const;

    /** @brief 追加 other 的全部元素到末尾 */
    void append(const HsObject& other);
    /** @brief 取出一格为新对象。越界抛 HsException */
    HsObject select(int index) const;
    /**
     * @brief 取出半开区间 [begin, end)。越界抛 HsException。begin 可大于 end 得空
     */
    HsObject slice(int begin, int end) const;

    /**
     * @brief 按元组下标挑选。indices 元素须为整数下标
     */
    HsObject select(const HsTuple& indices) const;
    /**
     * @brief 在 index 前插入 other 的全部元素。index 等于 count() 则追加
     */
    void insert(int index, const HsObject& other);
    /**
     * @brief 用 other 替换第 index 格。other 须恰好一格
     */
    void replace(int index, const HsObject& other);
    /**
     * @brief 删除第 index 格
     */
    void remove(int index);
    /**
     * @brief 按元组下标批量删除。indices 为整数下标
     */
    void remove(const HsTuple& indices);

    /** @brief 把第 index 格改成图像 */
    void setImage(int index, const HsImage& value);
    /** @brief 把第 index 格改成区域 */
    void setRegion(int index, const HsRegion& value);
    /** @brief 把第 index 格改成轮廓 */
    void setContour(int index, const HsContour& value);
    /** @brief 把第 index 格改成多边形 */
    void setPoly(int index, const HsPoly& value);
    /** @brief 把第 index 格改成圆 */
    void setCircle(int index, const CircleP& value);
    /** @brief 把第 index 格改成椭圆 */
    void setEllipse(int index, const EllipseP& value);
    /** @brief 把第 index 格改成轴对齐矩形 */
    void setRect1(int index, const Rect1P& value);
    /** @brief 把第 index 格改成旋转矩形 */
    void setRect2(int index, const Rect2P& value);
    /** @brief 把第 index 格改成线段 */
    void setLine(int index, const LineP& value);

    /** @brief 由图像构造 */
    static HsObject fromImage(const HsImage& value);
    /** @brief 由区域构造 */
    static HsObject fromRegion(const HsRegion& value);
    /** @brief 由轮廓构造 */
    static HsObject fromContour(const HsContour& value);
    /** @brief 由多边形构造 */
    static HsObject fromPoly(const HsPoly& value);
    /** @brief 由圆构造 */
    static HsObject fromCircle(const CircleP& value);
    /** @brief 由椭圆构造 */
    static HsObject fromEllipse(const EllipseP& value);
    /** @brief 由轴对齐矩形构造 */
    static HsObject fromRect1(const Rect1P& value);
    /** @brief 由旋转矩形构造 */
    static HsObject fromRect2(const Rect2P& value);
    /** @brief 由线段构造 */
    static HsObject fromLine(const LineP& value);

    /** @brief 只读取图像。种类不符或越界抛 HsException */
    const HsImage* imageAt(int index) const;
    /** @brief 只读取区域。种类不符或越界抛 HsException */
    const HsRegion* regionAt(int index) const;
    /** @brief 只读取轮廓。种类不符或越界抛 HsException */
    const HsContour* contourAt(int index) const;
    /** @brief 只读取多边形。种类不符或越界抛 HsException */
    const HsPoly* polyAt(int index) const;
    /** @brief 只读取圆。种类不符或越界抛 HsException */
    const CircleP* circleAt(int index) const;
    /** @brief 只读取椭圆。种类不符或越界抛 HsException */
    const EllipseP* ellipseAt(int index) const;
    /** @brief 只读取轴对齐矩形。种类不符或越界抛 HsException */
    const Rect1P* rect1At(int index) const;
    /** @brief 只读取旋转矩形。种类不符或越界抛 HsException */
    const Rect2P* rect2At(int index) const;
    /** @brief 只读取线段。种类不符或越界抛 HsException */
    const LineP* lineAt(int index) const;

    /**
     * @brief 可写取图像。种类不符或越界抛 HsException
     * 写入前会拆开共享，改动只影响本对象
     */
    HsImage* imageAt(int index);
    /** @brief 可写取区域。种类不符或越界抛 HsException */
    HsRegion* regionAt(int index);
    /** @brief 可写取轮廓。种类不符或越界抛 HsException */
    HsContour* contourAt(int index);
    /** @brief 可写取多边形。种类不符或越界抛 HsException */
    HsPoly* polyAt(int index);
    /** @brief 可写取圆。种类不符或越界抛 HsException */
    CircleP* circleAt(int index);
    /** @brief 可写取椭圆。种类不符或越界抛 HsException */
    EllipseP* ellipseAt(int index);
    /** @brief 可写取轴对齐矩形。种类不符或越界抛 HsException */
    Rect1P* rect1At(int index);
    /** @brief 可写取旋转矩形。种类不符或越界抛 HsException */
    Rect2P* rect2At(int index);
    /** @brief 可写取线段。种类不符或越界抛 HsException */
    LineP* lineAt(int index);
    /**
     * @brief 写出对象文件，对应 write_object
     * @param fileName 路径。空或打不开时抛 HsException
     */
    void writeObject(const char* fileName) const;
    /**
     * @brief 读入对象文件，对应 read_object。覆盖本对象
     * @param fileName 路径
     */
    void readObject(const char* fileName);

private:
    /** @brief 负下标收到 [0, count())。非法则抛 */
    int resolveIndex(int index) const;
    /** @brief 确保已分配缓冲 */
    void ensure();
    /** @brief 若被共享则拆开拷贝 */
    void detach();
    /**
     * @brief 调用方已持有 m_guard。拆开后解析下标
     * @param allowEnd true 时下标可等于元素个数，供插入末尾
     * @param out 解析后的下标
     * @return 越界时按系统异常处理并返回 false，不改表
     */
    bool lockedIndex(int index, bool allowEnd, int* out);
    /** @brief 只读取一格。越界返回 0 */
    const ObjectUnion* item(int index) const;
    /** @brief 可写取一格。越界抛异常 */
    ObjectUnion* itemWrite(int index);
    /** @brief 锁住元素表。未分配则空锁 */
    std::unique_lock<std::recursive_mutex> lockItems() const;

    /**
     * @brief 共享元素表。引用计数归零时释放
     */
    struct Store {
        mutable std::recursive_mutex mu; ///< 保护 list 与引用计数相关操作
        std::vector<std::shared_ptr<ObjectUnion>> list; ///< 元素
    };

    mutable std::recursive_mutex m_guard; ///< 保护本对象对 m_items 的替换
    std::shared_ptr<Store> m_items; ///< 元素表。空指针表示未分配
};

/**
 * @brief 写出对象文件，对应 write_object
 * @param object 待写对象
 * @param fileName 路径。空或打不开时抛 HsException
 */
HSCOMMDATA_EXPORT void writeObject(const HsObject& object, const char* fileName);
/**
 * @brief 读入对象文件，对应 read_object。写入 object
 * @param object 目标。空指针抛 HsException
 * @param fileName 路径
 */
HSCOMMDATA_EXPORT void readObject(HsObject* object, const char* fileName);

}

#endif
