/**
 * @file SharedRegion.h
 * @brief 命名共享内存。调用方自己约定里面的布局
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 *
 * 先试 Global，失败再退回 Local。同一台机器上的进程用同一个短名打开同一块。
 */

#ifndef IPC_SHARED_REGION_H
#define IPC_SHARED_REGION_H
#include "IpcExport.h"

#include <stdint.h>
#include <string>

namespace Ipc {

/**
 * @class SharedRegion
 * @brief 创建或打开一块命名共享内存
 */
class IPC_API SharedRegion {
public:
    /************************************************** 生命周期 **************************************************/
    /** @brief 尚未映射 */
    SharedRegion();
    /** @brief 解除映射并关闭句柄 */
    ~SharedRegion();

    /************************************************** 映射 **************************************************/
    /**
     * @brief 按短名创建或打开
     * @param name 不含 Global\\ 或 Local\\，例如 Vision.Ipc
     * @param bytes 映射字节数，由调用方算好
     * @param err 失败原因，可空
     * @return 已映射或映射成功返回 true；已打开时直接返回 true
     */
    bool open(const wchar_t* name, uint32_t bytes, std::string* err);
    /** @brief 解除映射 */
    void close();

    /************************************************** 查询 **************************************************/
    /** @brief 已经映射 */
    bool opened() const;
    /** @brief 本次调用创建了对象。打开已有对象时为 false */
    bool created() const;
    /** @brief 使用了 Global 命名空间。失败退回 Local 时为 false */
    bool global() const;
    /** @brief 映射基址。未打开时为空 */
    void* data() const;
    /** @brief 映射字节数 */
    uint32_t size() const;

private:
    /** @brief 禁止拷贝，映射只归一个对象 */
    SharedRegion(const SharedRegion&);
    /** @brief 禁止赋值 */
    SharedRegion& operator=(const SharedRegion&);

    void* m_mapping;    ///< 文件映射 HANDLE，未打开为空
    void* m_view;       ///< MapViewOfFile 得到的基址，未打开为空
    uint32_t m_bytes;   ///< 映射字节数，未打开为 0
    bool m_created;     ///< 本次 open 新建了对象（非打开已有）
    bool m_global;      ///< 对象位于 Global 命名空间；退回 Local 时为 false
};

}

#endif
