/**
 * @file NodeSettings.cpp
 * @brief 读写 node.json 和方案旁的 deploy.json
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "NodeSettings.h"
#include "AppPaths.h"
#include "Json.h"
#include "TextEncoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <fstream>
#include <sstream>

namespace Vision {

namespace {

std::string readAll(const std::string& path)
{
    std::ifstream in(utf8ToWide(path).c_str(), std::ios::binary);
    if (!in) {
        return std::string();
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeAll(const std::string& path, const std::string& body)
{
    std::ofstream out(utf8ToWide(path).c_str(), std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << body;
    return true;
}

NodeRole roleFromText(const std::string& s)
{
    if (s == "server") {
        return NodeRoleServer;
    }
    if (s == "client") {
        return NodeRoleClient;
    }
    return NodeRoleStandalone;
}

const char* roleText(NodeRole role)
{
    if (role == NodeRoleServer) {
        return "server";
    }
    if (role == NodeRoleClient) {
        return "client";
    }
    return "standalone";
}

}

NodeSettings::NodeSettings()
    : role(NodeRoleStandalone)
    , serverPort(9100)
    , listenPort(9100)
    , heartbeatMs(1000)
    , jpegQuality(70)
    , previewSlotBytes(SharedFrames::kDefaultSlotBytes)
    , rawDir("D:/vision/raw")
    , stationEnabled(true)
    , linkEnabled(false)
{
}

std::string NodeSettings::filePath()
{
    return AppPaths::dataDir() + "\\node.json";
}

bool NodeSettings::load(std::string* err)
{
    const std::string text = readAll(filePath());
    if (text.empty()) {
        return true;
    }
    std::string parseErr;
    const JsonValue root = JsonValue::parse(text, &parseErr);
    if (!root.isObject()) {
        if (err) {
            *err = parseErr.empty() ? "node.json 不是对象" : parseErr;
        }
        return false;
    }
    nodeId = root.stringOf("nodeId");
    nodeName = root.stringOf("nodeName");
    role = roleFromText(root.stringOf("role"));
    serverHost = root.stringOf("serverHost");
    serverPort = root.integerOf("serverPort", serverPort);
    listenHost = root.stringOf("listenHost");
    listenPort = root.integerOf("listenPort", listenPort);
    heartbeatMs = root.integerOf("heartbeatMs", heartbeatMs);
    jpegQuality = root.integerOf("jpegQuality", jpegQuality);
    previewSlotBytes = static_cast<uint32_t>(root.integerOf("previewSlotBytes", static_cast<int>(previewSlotBytes)));
    if (!root.stringOf("rawDir").empty()) {
        rawDir = root.stringOf("rawDir");
    }
    const JsonValue services = root.value("services");
    if (services.isObject()) {
        stationEnabled = services.stringOf("station") != "disabled";
        linkEnabled = services.stringOf("link") != "disabled";
    }
    if (role == NodeRoleStandalone) {
        linkEnabled = false;
    }
    return true;
}

bool NodeSettings::save(std::string* err) const
{
    JsonValue root;
    root.makeObject();
    root.set("nodeId", nodeId);
    root.set("nodeName", nodeName);
    root.set("role", roleText(role));
    root.set("serverHost", serverHost);
    root.set("serverPort", serverPort);
    root.set("listenHost", listenHost);
    root.set("listenPort", listenPort);
    root.set("heartbeatMs", heartbeatMs);
    root.set("jpegQuality", jpegQuality);
    root.set("previewSlotBytes", static_cast<int>(previewSlotBytes));
    root.set("rawDir", rawDir);
    JsonValue services;
    services.makeObject();
    services.set("station", stationEnabled ? "auto" : "disabled");
    services.set("link", linkEnabled ? "auto" : "disabled");
    root.set("services", services);
    if (!writeAll(filePath(), JsonValue::stringify(root, true))) {
        if (err) {
            *err = "写 node.json 失败";
        }
        return false;
    }
    return true;
}

DeployTable::DeployTable()
{
}

std::string DeployTable::pathBeside(const std::string& solutionPath)
{
    const size_t dot = solutionPath.find_last_of('.');
    const size_t slash = solutionPath.find_last_of("\\/");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return solutionPath.substr(0, dot) + ".deploy.json";
    }
    return solutionPath + ".deploy.json";
}

bool DeployTable::loadBeside(const std::string& solutionPath, std::string* err)
{
    owners.clear();
    solutionId.clear();
    const std::string text = readAll(pathBeside(solutionPath));
    if (text.empty()) {
        return true;
    }
    std::string parseErr;
    const JsonValue root = JsonValue::parse(text, &parseErr);
    if (!root.isObject()) {
        if (err) {
            *err = parseErr.empty() ? "deploy.json 不是对象" : parseErr;
        }
        return false;
    }
    solutionId = root.stringOf("solutionId");
    const JsonValue table = root.value("owners");
    if (!table.isObject()) {
        return true;
    }
    const std::map<std::string, JsonValue>& rows = table.object();
    for (std::map<std::string, JsonValue>::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        owners[it->first] = it->second.toString();
    }
    return true;
}

std::string DeployTable::ownerOf(const std::string& stationId) const
{
    std::map<std::string, std::string>::const_iterator it = owners.find(stationId);
    if (it == owners.end()) {
        return std::string();
    }
    return it->second;
}

bool DeployTable::ownedBy(const std::string& stationId, const std::string& nodeId) const
{
    return ownerOf(stationId) == nodeId;
}

}
