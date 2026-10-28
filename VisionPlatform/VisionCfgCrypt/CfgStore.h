/**
 * @file CfgStore.h
 * @brief 配置文件读写。内存里仍是 JSON，磁盘上是密文
 */

#ifndef VISION_CFG_STORE_H
#define VISION_CFG_STORE_H

#include "VisionCfgCryptExport.h"
#include <string>

namespace Vision {

/** @brief 读到的配置是哪一种 */
enum CfgFileKind {
    CfgFileMissing = 0, ///< 文件不存在
    CfgFilePlain,       ///< 旧的明文 JSON
    CfgFileSealed,      ///< 本库写出的密文
    CfgFileCorrupt      ///< 头不对或校验失败，原文件保留
};

/** @brief 一次读取的结果。json 只在明文或密文成功时有内容 */
struct VISION_CFGCRYPT_API CfgBytes {
    CfgFileKind kind;
    std::string json;
    CfgBytes() : kind(CfgFileMissing) {}
};

/**
 * @class CfgStore
 * @brief 按路径读写配置。write 加密后原子替换。read 不改原文件
 */
class VISION_CFGCRYPT_API CfgStore {
public:
    /** @brief 读配置。不存在、明文、密文、损坏四种结果 */
    CfgBytes read(const std::string& path) const;
    /** @brief 把 JSON 字节加密后写入。失败时不留下半个文件 */
    bool write(const std::string& path, const std::string& json) const;
};

}

#endif
