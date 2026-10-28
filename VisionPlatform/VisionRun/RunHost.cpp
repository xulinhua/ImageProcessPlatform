/**
 * @file RunHost.cpp
 * @brief 按部署表启动本机工站，一拍结束后存原图再发布 JPEG
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "RunHost.h"
#include "ThreadPool.h"

namespace Vision {

RunHost::RunHost()
    : m_sol(0)
    , m_node(0)
    , m_deploy(0)
    , m_pool(0)
    , m_running(false)
{
}

RunHost::~RunHost()
{
    stop();
}

void RunHost::bind(Solution* sol, const NodeSettings* node, const DeployTable* deploy, ThreadPool* pool)
{
    stop();
    m_sol = sol;
    m_node = node;
    m_deploy = deploy;
    m_pool = pool;
    if (node) {
        m_raw.setRoot(node->rawDir);
    }
}

bool RunHost::mine(const StationCfg& station) const
{
    if (!station.enabled) {
        return false;
    }
    if (!m_node || m_node->role == NodeRoleStandalone || !m_deploy || m_deploy->owners.empty()) {
        return true;
    }
    const std::string id = station.id.toString();
    const std::string owner = m_deploy->ownerOf(id);
    if (owner.empty()) {
        return false;
    }
    return owner == m_node->nodeId;
}

void RunHost::onCycleThunk(void* user, const StationRunRst& result)
{
    Slot* slot = static_cast<Slot*>(user);
    if (slot && slot->host) {
        slot->host->onCycle(slot, result);
    }
}

void RunHost::onCycle(Slot* slot, const StationRunRst& result)
{
    if (!slot) {
        return;
    }
    ++slot->total;
    if (result.ok) {
        ++slot->pass;
    } else {
        ++slot->fail;
    }
    const uint32_t seq = result.source.frameId > 0
        ? static_cast<uint32_t>(result.source.frameId)
        : static_cast<uint32_t>(slot->total);
    const HsCommData::HsImage& raw = result.source.isNull() ? result.image : result.source;
    m_raw.save(slot->stationId, seq, raw);
    std::vector<unsigned char> jpeg;
    const int quality = m_node ? m_node->jpegQuality : 70;
    m_encoder.encode(result.image.isNull() ? raw : result.image, quality, &jpeg);
    SharedFrames& frames = SharedFrames::instance();
    if (!frames.publishLocal(slot->index, seq, result.ok, result.cycleMs,
                            slot->total, slot->pass, slot->fail, result.summary, jpeg)) {
        frames.setFault(IpcFaultRejected, "画面超出槽大小，这一拍没有发布");
    }
    frames.signalFrame();
}

void RunHost::startOwned()
{
    stop();
    if (!m_sol || !m_node) {
        return;
    }
    SharedFrames& frames = SharedFrames::instance();
    frames.setRole(m_node->role);
    for (size_t i = 0; i < m_sol->stations.size(); ++i) {
        StationCfg& cfg = m_sol->stations[i];
        if (!mine(cfg)) {
            continue;
        }
        const std::string id = cfg.id.toString();
        if (m_deploy && !m_deploy->owners.empty()) {
            int claims = 0;
            for (std::map<std::string, std::string>::const_iterator it = m_deploy->owners.begin();
                 it != m_deploy->owners.end(); ++it) {
                if (it->first == id) {
                    ++claims;
                }
            }
            if (claims > 1) {
                frames.setFault(IpcFaultClaim, "工站 " + id + " 在部署表里出现了两次");
                return;
            }
        }
        Slot* slot = new Slot();
        slot->host = this;
        slot->stationId = id;
        slot->index = frames.bindLocal(id, m_node->nodeId);
        slot->station = new Station(&cfg);
        slot->loop = new StationLoop();
        slot->loop->start(slot->station, m_sol, m_pool, &RunHost::onCycleThunk, slot);
        m_slots.push_back(slot);
    }
    m_running = !m_slots.empty();
    if (m_running) {
        frames.setFault(IpcFaultNone, std::string());
    }
}

void RunHost::stop()
{
    for (size_t i = 0; i < m_slots.size(); ++i) {
        Slot* slot = m_slots[i];
        if (slot->loop) {
            slot->loop->stop();
            delete slot->loop;
        }
        if (slot->station) {
            delete slot->station;
        }
        delete slot;
    }
    m_slots.clear();
    m_running = false;
}

bool RunHost::running() const
{
    return m_running;
}

}
