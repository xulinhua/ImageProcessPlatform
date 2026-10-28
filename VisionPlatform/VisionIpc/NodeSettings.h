/**
 * @file NodeSettings.h
 * @brief 本机节点设置和随方案走的部署表
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_NODE_SETTINGS_H
#define VISION_NODE_SETTINGS_H
#include "VisionIpcExport.h"

#include "SharedFrames.h"
#include <map>
#include <string>

namespace Vision {

/**
 * @class NodeSettings
 * @brief 每台机器不同的 node.json。没有壳的选项，托盘总是总控
 */
class VISION_IPC_API NodeSettings {
public:
    std::string nodeId;     ///< 节点 id
    std::string nodeName;   ///< 显示名
    NodeRole role;          ///< standalone / server / client
    std::string serverHost; ///< 客户端要连的服务端
    int serverPort;         ///< 客户端端口
    std::string listenHost; ///< 服务端监听地址，第一版仍绑定全部网卡
    int listenPort;         ///< 服务端端口
    int heartbeatMs;        ///< 心跳间隔，默认 1000
    int jpegQuality;        ///< 显示图质量，默认 70
    uint32_t previewSlotBytes; ///< 画面槽大小
    std::string rawDir;     ///< 原图根目录
    bool stationEnabled;    ///< services.station 不是 disabled
    bool linkEnabled;       ///< services.link 不是 disabled

    /** @brief 单机、链路关闭、工站打开 */
    NodeSettings();

    /** @brief 从 exe\data\node.json 读取。没有文件时保持默认 */
    bool load(std::string* err);
    /** @brief 写回 node.json，便于现场改角色 */
    bool save(std::string* err) const;
    /** @brief 文件路径：数据目录下的 node.json */
    static std::string filePath();
};

/**
 * @class DeployTable
 * @brief 工站 id 到节点 id。各端内容相同，跟方案走
 */
class VISION_IPC_API DeployTable {
public:
    std::string solutionId; ///< 方案标识，Hello 用来核对
    std::map<std::string, std::string> owners; ///< 工站 id → 节点 id

    /** @brief 空表 */
    DeployTable();

    /**
     * @brief 读方案旁边的 deploy.json
     * @param solutionPath 方案文件全路径
     */
    bool loadBeside(const std::string& solutionPath, std::string* err);
    /** @brief 这个工站属于哪个节点。没有登记则空 */
    std::string ownerOf(const std::string& stationId) const;
    /** @brief 是否属于本节点。单机角色时调用方应改走全部已使能工站 */
    bool ownedBy(const std::string& stationId, const std::string& nodeId) const;
    /** @brief deploy.json 的路径 */
    static std::string pathBeside(const std::string& solutionPath);
};

}

#endif
