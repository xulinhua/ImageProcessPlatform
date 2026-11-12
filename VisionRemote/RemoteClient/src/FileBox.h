/**
 * @file FileBox.h
 * @brief 被控端文件通道。目录、建目录、上传和下载都走这里
 *
 * 控制端路径用反斜杠，这里先换成 Linux 路径。下载写到 path.vrpart，
 * 校验 SHA-256 后再改成正式名字。同一时间只传一个文件。
 */

#ifndef REMOTE_CLIENT_FILE_BOX_H
#define REMOTE_CLIENT_FILE_BOX_H

#include "Frame.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Vision {

class FileBox {
public:
    FileBox();
    ~FileBox();

    /** @brief 断开会话时关掉正在传的文件，不删已经收了一半的 .vrpart */
    void reset();
    /**
     * @brief 处理文件通道的一包，返回要回给中控台的帧
     * @param allow 已经拿到操作权，并且认证里带了文件权限
     * @param deny 不允许时回给中控台的原因
     */
    std::vector<CommBytes> take(const Frame::Packet& packet, bool allow, const std::string& deny);
    /** @brief 把正在上传的下几块取出来。没有上传时返回空 */
    std::vector<CommBytes> pump();
    /** @brief 界面上的一行状态，例如「正在上传」 */
    std::string status() const;

private:
    /** @brief 控制端的反斜杠路径换成绝对路径。空串表示列根。不合法返回 false */
    bool normalize(const std::string& raw, std::string* path) const;
    /** @brief 列出根或者一个目录。打不开时返回错误行 */
    std::string list(const std::string& path) const;
    /** @brief 把目录和它缺的上级一起建出来 */
    bool makeDir(const std::string& path) const;
    /** @brief 文件的 SHA-256 十六进制。打不开返回空 */
    std::string digest(const std::string& path) const;
    /** @brief 关掉当前文件 */
    void closeFile();

    FILE* m_file;
    bool m_pull;                ///< true：被控端把文件发给中控台
    uint32_t m_id;
    std::string m_path;
    std::string m_sha;
    uint32_t m_size;
    uint32_t m_off;             ///< 已经写入或读出的位置
    uint32_t m_acked;           ///< 中控台确认过的位置
    uint32_t m_next;            ///< 下一块从这里读
    std::string m_status;
};

}

#endif
