/**
 * @file CompositeMgr.cpp
 * @brief CompositeMgr实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "CompositeMgr.h"
#include "AppPaths.h"
#include "Json.h"
#include "CfgStore.h"
#include "TextEncoding.h"
#include <fstream>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

JsonValue compositeExportJob(const JobCfg& j);
JobCfg compositeImportJob(const JsonValue& o);

namespace {

std::string fileOf(const ObjectId& id)
{
    return AppPaths::compositesDir() + "\\" + id.toString() + ".json";
}

bool readUtf8(const std::string& path, std::string* out)
{
    const CfgBytes got = CfgStore().read(path);
    if (got.kind != CfgFilePlain && got.kind != CfgFileSealed) {
        return false;
    }
    if (out) {
        *out = got.json;
    }
    return true;
}

bool writeUtf8(const std::string& path, const std::string& data)
{
    AppPaths::ensureDirs();
    return CfgStore().write(path, data);
}

JsonValue portToJson(const CompositePort& p)
{
    JsonValue o;
    o.makeObject();
    o.set("name", JsonValue(p.name));
    o.set("dir", JsonValue(static_cast<int>(p.dir)));
    o.set("data", JsonValue(static_cast<int>(p.data)));
    o.set("innerToolId", JsonValue(p.innerToolId.toString()));
    o.set("innerDir", JsonValue(static_cast<int>(p.innerPort.dir)));
    o.set("innerData", JsonValue(static_cast<int>(p.innerPort.data)));
    o.set("innerName", JsonValue(p.innerPort.name));
    return o;
}

CompositePort portFromJson(const JsonValue& o)
{
    CompositePort p;
    p.name = o.value("name").toString();
    p.dir = static_cast<PortDir>(o.value("dir").toInt(0));
    p.data = static_cast<PortData>(o.value("data").toInt(0));
    p.innerToolId = ObjectId::fromString(o.value("innerToolId").toString());
    p.innerPort.dir = static_cast<PortDir>(o.value("innerDir").toInt(0));
    p.innerPort.data = static_cast<PortData>(o.value("innerData").toInt(0));
    p.innerPort.name = o.value("innerName").toString();
    return p;
}

JsonValue roiPubToJson(const CompositeRoiPub& p)
{
    JsonValue o;
    o.makeObject();
    o.set("name", JsonValue(p.name));
    o.set("innerToolId", JsonValue(p.innerToolId.toString()));
    o.set("innerSlot", JsonValue(p.innerSlot));
    return o;
}

CompositeRoiPub roiPubFromJson(const JsonValue& o)
{
    CompositeRoiPub p;
    p.name = o.value("name").toString();
    p.innerToolId = ObjectId::fromString(o.value("innerToolId").toString());
    p.innerSlot = o.value("innerSlot").toString();
    return p;
}

JsonValue paraPubToJson(const CompositeParaPub& p)
{
    JsonValue o;
    o.makeObject();
    o.set("name", JsonValue(p.name));
    o.set("innerToolId", JsonValue(p.innerToolId.toString()));
    o.set("key", JsonValue(p.key));
    o.set("learn", JsonValue(p.learn));
    return o;
}

CompositeParaPub paraPubFromJson(const JsonValue& o)
{
    CompositeParaPub p;
    p.name = o.value("name").toString();
    p.innerToolId = ObjectId::fromString(o.value("innerToolId").toString());
    p.key = o.value("key").toString();
    p.learn = o.value("learn").toBool(false);
    return p;
}

bool jobUses(const JobCfg& job, const ObjectId& id)
{
    for (size_t i = 0; i < job.tools.size(); ++i) {
        if (job.tools[i].info.compositeId == id) {
            return true;
        }
    }
    return false;
}

}

JsonValue compositeToJson(const CompositeDef& d)
{
    JsonValue o;
    o.makeObject();
    o.set("id", JsonValue(d.id.toString()));
    o.set("scope", JsonValue(static_cast<int>(d.scope)));
    o.set("name", JsonValue(d.name));
    o.set("description", JsonValue(d.description));
    o.set("version", JsonValue(d.version));
    o.set("inner", compositeExportJob(d.inner));
    JsonValue ports;
    ports.makeArray();
    for (size_t i = 0; i < d.ports.size(); ++i) {
        ports.append(portToJson(d.ports[i]));
    }
    o.set("ports", ports);
    JsonValue rois;
    rois.makeArray();
    for (size_t i = 0; i < d.rois.size(); ++i) {
        rois.append(roiPubToJson(d.rois[i]));
    }
    o.set("rois", rois);
    JsonValue paras;
    paras.makeArray();
    for (size_t i = 0; i < d.paras.size(); ++i) {
        paras.append(paraPubToJson(d.paras[i]));
    }
    o.set("paras", paras);
    return o;
}

CompositeDef compositeFromJson(const JsonValue& o)
{
    CompositeDef d;
    d.id = ObjectId::fromString(o.value("id").toString());
    d.scope = static_cast<CompositeScope>(o.value("scope").toInt(CompositeLibrary));
    d.name = o.value("name").toString();
    d.description = o.value("description").toString();
    d.version = o.value("version").toString();
    if (d.version.empty()) {
        d.version = "1.0.0.0";
    }
    if (o.value("inner").isObject()) {
        d.inner = compositeImportJob(o.value("inner"));
    }
    const JsonValue ports = o.value("ports");
    for (int i = 0; i < ports.size(); ++i) {
        d.ports.push_back(portFromJson(ports.at(i)));
    }
    const JsonValue rois = o.value("rois");
    for (int i = 0; i < rois.size(); ++i) {
        d.rois.push_back(roiPubFromJson(rois.at(i)));
    }
    const JsonValue paras = o.value("paras");
    for (int i = 0; i < paras.size(); ++i) {
        d.paras.push_back(paraPubFromJson(paras.at(i)));
    }
    return d;
}

CompositeMgr& CompositeMgr::instance()
{
    static CompositeMgr g;
    return g;
}

CompositeMgr::CompositeMgr()
    : m_solution(nullptr)
    , m_open(nullptr)
{}

bool CompositeMgr::load(std::string* error)
{
    m_library.clear();
    AppPaths::ensureDirs();
    const std::wstring pat = utf8ToWide(AppPaths::compositesDir() + "\\*.json");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return true;
    }
    bool ok = true;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        const std::string name = wideToUtf8(fd.cFileName);
        const std::string path = AppPaths::compositesDir() + "\\" + name;
        std::string text;
        if (!readUtf8(path, &text)) {
            ok = false;
            continue;
        }
        std::string perr;
        const JsonValue root = JsonValue::parse(text, &perr);
        if (!perr.empty() || !root.isObject()) {
            ok = false;
            if (error && error->empty()) {
                *error = name;
            }
            continue;
        }
        CompositeDef d = compositeFromJson(root);
        d.scope = CompositeLibrary;
        if (!d.id.isNull()) {
            m_library.push_back(d);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}

bool CompositeMgr::saveOne(const ObjectId& id, std::string* error) const
{
    const CompositeDef* d = find(id);
    if (!d || d->scope != CompositeLibrary) {
        if (error) {
            *error = "不是独立组合工具";
        }
        return false;
    }
    const std::string path = fileOf(id);
    if (!writeUtf8(path, JsonValue::stringify(compositeToJson(*d), true))) {
        if (error) {
            *error = "写入组合文件失败";
        }
        return false;
    }
    return true;
}

void CompositeMgr::setSolutionDefs(std::vector<CompositeDef>* defs)
{
    m_solution = defs;
}

void CompositeMgr::setOpenSolution(Solution* sol)
{
    m_open = sol;
    m_solution = sol ? &sol->composites : 0;
}

ObjectId CompositeMgr::create(CompositeScope scope, const std::string& name, std::string* error)
{
    if (scope == CompositeSolution && !m_solution) {
        if (error) {
            *error = "没有打开的方案";
        }
        return ObjectId();
    }
    CompositeDef d;
    d.id = ObjectId::create();
    d.scope = scope;
    d.name = name.empty() ? std::string("composite") : name;
    d.version = "1.0.0.0";
    d.inner.id = ObjectId::create();
    d.inner.name = d.name;
    d.inner.execMode = ExecActive;
    d.inner.root.kind = GroupSequence;
    d.inner.root.id = ObjectId::create();
    if (scope == CompositeLibrary) {
        m_library.push_back(d);
        saveOne(d.id, error);
    } else {
        m_solution->push_back(d);
    }
    return d.id;
}

CompositeDef* CompositeMgr::find(const ObjectId& id)
{
    for (size_t i = 0; i < m_library.size(); ++i) {
        if (m_library[i].id == id) {
            return &m_library[i];
        }
    }
    if (m_solution) {
        for (size_t i = 0; i < m_solution->size(); ++i) {
            if ((*m_solution)[i].id == id) {
                return &(*m_solution)[i];
            }
        }
    }
    return 0;
}

const CompositeDef* CompositeMgr::find(const ObjectId& id) const
{
    return const_cast<CompositeMgr*>(this)->find(id);
}

const CompositeDef* CompositeMgr::findRaw(const ObjectId& id)
{
    return instance().find(id);
}

bool CompositeMgr::remove(const ObjectId& id, std::string* error)
{
    if (m_open) {
        for (size_t s = 0; s < m_open->stations.size(); ++s) {
            StationCfg& st = m_open->stations[s];
            for (size_t j = 0; j < st.jobs.size(); ++j) {
                if (jobUses(st.jobs[j], id)) {
                    if (error) {
                        *error = "流程里还在使用";
                    }
                    return false;
                }
            }
        }
    }
    for (size_t i = 0; i < m_library.size(); ++i) {
        if (m_library[i].id == id) {
            DeleteFileW(utf8ToWide(fileOf(id)).c_str());
            m_library.erase(m_library.begin() + static_cast<int>(i));
            return true;
        }
    }
    if (m_solution) {
        for (size_t i = 0; i < m_solution->size(); ++i) {
            if ((*m_solution)[i].id == id) {
                m_solution->erase(m_solution->begin() + static_cast<int>(i));
                if (m_open) {
                    m_open->dirty = true;
                }
                return true;
            }
        }
    }
    return false;
}

std::vector<CompositeDef*> CompositeMgr::all()
{
    std::vector<CompositeDef*> out;
    for (size_t i = 0; i < m_library.size(); ++i) {
        out.push_back(&m_library[i]);
    }
    if (m_solution) {
        for (size_t i = 0; i < m_solution->size(); ++i) {
            out.push_back(&(*m_solution)[i]);
        }
    }
    return out;
}

}