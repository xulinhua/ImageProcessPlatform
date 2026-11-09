/**
 * @file ControlPhase4.cpp
 * @brief 中控台：剪贴板、文件、聊天、系统信息的收发
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ControlSession.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

namespace Vision {

std::wstring ControlSession::widePath(const std::string& text)
{
    if (text.empty()) {
        return std::wstring();
    }
    const int chars = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, 0, 0);
    if (chars <= 1) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(chars - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &out[0], chars);
    return out;
}

void ControlSession::post(const std::string& peerId, const CommBytes& bytes)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0) {
        return;
    }
    enqueue(&m_machines[static_cast<size_t>(index)], bytes);
}

void ControlSession::armDownload(const std::string& peerId, uint32_t id, const std::string& localPath, uint64_t total)
{
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0) {
        return;
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    machine.downId = id;
    machine.downPath = localPath;
    machine.tile.fileGot = 0;
    WIN32_FILE_ATTRIBUTE_DATA have;
    if (GetFileAttributesExW(widePath(localPath).c_str(), GetFileExInfoStandard, &have)) {
        ULARGE_INTEGER size;
        size.LowPart = have.nFileSizeLow;
        size.HighPart = have.nFileSizeHigh;
        machine.tile.fileGot = size.QuadPart;
    }
    machine.tile.fileBytes = total;
    machine.tile.fileNote = "正在下载";
}

std::string ControlSession::sha256File(const std::wstring& path) const
{
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (!file) {
        return std::string();
    }
    BCRYPT_ALG_HANDLE alg = 0;
    BCRYPT_HASH_HANDLE hash = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, 0, 0) != 0
        || BCryptCreateHash(alg, &hash, 0, 0, 0, 0, 0) != 0) {
        if (alg) {
            BCryptCloseAlgorithmProvider(alg, 0);
        }
        fclose(file);
        return std::string();
    }
    unsigned char buf[65536];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), file)) > 0) {
        BCryptHashData(hash, buf, static_cast<ULONG>(n), 0);
    }
    fclose(file);
    unsigned char dig[32];
    BCryptFinishHash(hash, dig, sizeof(dig), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    static const char* hex = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 32; ++i) {
        out[static_cast<size_t>(i) * 2] = hex[dig[i] >> 4];
        out[static_cast<size_t>(i) * 2 + 1] = hex[dig[i] & 0x0f];
    }
    return out;
}

std::string ControlSession::armUpload(const std::string& peerId, uint32_t id, const std::string& localPath, const std::string& remotePath)
{
    const std::string sha = sha256File(widePath(localPath));
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0) {
        return std::string();
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    if (sha.empty()) {
        machine.tile.fileNote = "打不开本地文件";
        machine.tile.fileSeq += 1;
        return std::string();
    }
    machine.upOn = true;
    machine.upId = id;
    machine.upOff = 0;
    machine.upAcked = 0;
    machine.upNext = 0;
    machine.upSha = sha;
    machine.upReady = false;
    machine.upFinished = false;
    machine.upLocal = localPath;
    machine.upRemote = remotePath;
    WIN32_FILE_ATTRIBUTE_DATA info;
    machine.upSize = 0;
    if (GetFileAttributesExW(widePath(localPath).c_str(), GetFileExInfoStandard, &info)) {
        ULARGE_INTEGER size;
        size.LowPart = info.nFileSizeLow;
        size.HighPart = info.nFileSizeHigh;
        machine.upSize = size.QuadPart;
    }
    machine.tile.fileGot = 0;
    machine.tile.fileBytes = machine.upSize;
    machine.tile.fileNote = "正在上传";
    return sha;
}

bool ControlSession::takePhase4(Machine* machine, const Frame::Packet& packet)
{
    if (!machine) {
        return false;
    }
    if (packet.channel == Frame::ChClip) {
        if (packet.type == Frame::TypeClipText) {
            std::string text;
            if (Messages::unpackClipText(packet.payload, &text)) {
                machine->tile.clipText = text;
                machine->tile.clipSeq += 1;
            }
        } else if (packet.type == Frame::TypeClipImage) {
            CommBytes png;
            if (Messages::unpackClipImage(packet.payload, &png)) {
                machine->tile.clipPng.swap(png);
                machine->tile.clipSeq += 1;
            }
        } else if (packet.type == Frame::TypeClipFiles) {
            std::string paths;
            if (Messages::unpackClipFiles(packet.payload, &paths)) {
                machine->tile.clipFiles = paths;
                machine->tile.clipSeq += 1;
            }
        } else {
            return false;
        }
        return true;
    }
    if (packet.channel == Frame::ChChat && packet.type == Frame::TypeChatText) {
        std::string name;
        std::string text;
        if (Messages::unpackChat(packet.payload, &name, &text)) {
            machine->tile.chatLine = name + ": " + text;
            machine->tile.chatSeq += 1;
        }
        return true;
    }
    if (packet.channel == Frame::ChControl && packet.type == Frame::TypeViewers) {
        std::string listing;
        if (Messages::unpackViewers(packet.payload, &listing) && listing != machine->tile.viewers) {
            machine->tile.viewers = listing;
            machine->tile.viewerSeq += 1;
        }
        return true;
    }
    if (packet.channel == Frame::ChControl && packet.type == Frame::TypeSysInfo) {
        std::string text;
        if (Messages::unpackSysInfo(packet.payload, &text)) {
            fillStation(&machine->tile, text);
        }
        return true;
    }
    if (packet.channel != Frame::ChFile) {
        return false;
    }
    // 被控端发起的列目录 / 建目录 / 打开文件：中控台当服务端
    if (packet.type == Frame::TypeListDir) {
        std::string path;
        if (Messages::unpackListDir(packet.payload, &path)) {
            std::string err;
            enqueue(machine, Messages::packDirEntries(listLocalDirectory(widePath(path)), &err));
        }
        return true;
    }
    if (packet.type == Frame::TypeMkDir) {
        uint32_t id = 0;
        std::string path;
        if (Messages::unpackMkDir(packet.payload, &id, &path)) {
            const bool ok = makeLocalDirectory(widePath(path));
            std::string err;
            enqueue(machine, Messages::packFileDone(id, ok ? 1 : 0, ok ? "完成" : "不能建目录", &err));
        }
        return true;
    }
    if (packet.type == Frame::TypeFileOpen) {
        uint32_t id = 0;
        uint8_t pull = 0;
        std::string path;
        uint64_t size = 0;
        std::string sha;
        uint64_t resume = 0;
        if (!Messages::unpackFileOpen(packet.payload, &id, &pull, &path, &size, &sha, &resume)) {
            return true;
        }
        std::string err;
        if (pull == 0) {
            // 被控端推文件到中控台
            const std::wstring wide = widePath(path);
            if (wide.empty()) {
                enqueue(machine, Messages::packFileDone(id, 0, "路径不允许", &err));
                return true;
            }
            const size_t slash = wide.find_last_of(L'\\');
            if (slash != std::wstring::npos && slash > 2) {
                makeLocalDirectory(wide.substr(0, slash));
            }
            machine->downId = id;
            machine->downPath = path;
            machine->tile.fileGot = resume;
            machine->tile.fileBytes = size;
            machine->tile.fileNote = "正在接收被控端文件";
            FILE* file = _wfopen(wide.c_str(), resume == 0 ? L"wb" : L"r+b");
            if (!file) {
                enqueue(machine, Messages::packFileDone(id, 0, "不能写入", &err));
                machine->downPath.clear();
                return true;
            }
            fclose(file);
            enqueue(machine, Messages::packFileAck(id, resume, &err));
        } else {
            // 被控端要从中控台拉文件
            const std::string shaLocal = sha256File(widePath(path));
            if (shaLocal.empty()) {
                enqueue(machine, Messages::packFileDone(id, 0, "打不开本地文件", &err));
                return true;
            }
            machine->upOn = true;
            machine->upId = id;
            machine->upOff = 0;
            machine->upAcked = static_cast<uint32_t>(resume);
            machine->upNext = static_cast<uint32_t>(resume);
            machine->upSha = shaLocal;
            machine->upReady = true;
            machine->upFinished = false;
            machine->upLocal = path;
            machine->upRemote = path;
            WIN32_FILE_ATTRIBUTE_DATA info;
            machine->upSize = 0;
            if (GetFileAttributesExW(widePath(path).c_str(), GetFileExInfoStandard, &info)) {
                ULARGE_INTEGER u;
                u.LowPart = info.nFileSizeLow;
                u.HighPart = info.nFileSizeHigh;
                machine->upSize = u.QuadPart;
            }
            machine->tile.fileGot = resume;
            machine->tile.fileBytes = machine->upSize;
            machine->tile.fileNote = "正在发给被控端";
        }
        return true;
    }
    if (packet.type == Frame::TypeDirEntries) {
        std::string listing;
        if (Messages::unpackDirEntries(packet.payload, &listing)) {
            machine->tile.dirList = listing;
            machine->tile.dirSeq += 1;
        }
        return true;
    }
    if (packet.type == Frame::TypeFileChunk) {
        uint32_t id = 0;
        uint32_t offset = 0;
        CommBytes data;
        if (!Messages::unpackFileChunk(packet.payload, &id, &offset, &data)) {
            return true;
        }
        if (id != machine->downId || machine->downPath.empty()) {
            return true;
        }
        FILE* file = _wfopen(widePath(machine->downPath).c_str(), offset == 0 ? L"wb" : L"r+b");
        if (!file && offset != 0) {
            file = _wfopen(widePath(machine->downPath).c_str(), L"wb");
        }
        if (file) {
            _fseeki64(file, offset, SEEK_SET);
            fwrite(data.data(), 1, data.size(), file);
            fclose(file);
            if (offset == machine->tile.fileGot) {
                machine->tile.fileGot = offset + data.size();
                std::string err;
                enqueue(machine, Messages::packFileAck(id, machine->tile.fileGot, &err));
            }
            machine->tile.fileNote = "正在下载";
        }
        return true;
    }
    if (packet.type == Frame::TypeFileAck) {
        uint32_t id = 0;
        uint64_t offset = 0;
        if (Messages::unpackFileAck(packet.payload, &id, &offset) && id == machine->upId && machine->upOn) {
            machine->upReady = true;
            if (offset >= machine->upAcked && offset <= machine->upSize) {
                machine->upAcked = static_cast<uint32_t>(offset);
                if (machine->upNext < machine->upAcked) {
                    machine->upNext = machine->upAcked;
                }
            }
        }
        return true;
    }
    if (packet.type == Frame::TypeFileDone) {
        uint32_t id = 0;
        uint8_t ok = 0;
        std::string note;
        if (Messages::unpackFileDone(packet.payload, &id, &ok, &note)) {
            std::string shown = ok ? "完成" : note;
            if (ok && note.size() == 64 && !machine->downPath.empty()) {
                const std::string got = sha256File(widePath(machine->downPath));
                if (!got.empty() && got != note) {
                    DeleteFileW(widePath(machine->downPath).c_str());
                    shown = "SHA-256 不一致，已删除";
                }
            }
            machine->tile.fileNote = shown;
            machine->tile.fileSeq += 1;
            machine->downPath.clear();
            machine->upOn = false;
        }
        return true;
    }
    return true;
}

void ControlSession::pumpUpload(const std::string& peerId)
{
    std::string local;
    uint32_t id = 0;
    uint32_t acked = 0;
    uint32_t next = 0;
    uint64_t size = 0;
    bool on = false;
    bool ready = false;
    bool finished = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        const int index = indexOfPeer(peerId);
        if (index < 0) {
            return;
        }
        Machine& machine = m_machines[static_cast<size_t>(index)];
        on = machine.upOn;
        ready = machine.upReady;
        finished = machine.upFinished;
        local = machine.upLocal;
        id = machine.upId;
        acked = machine.upAcked;
        next = machine.upNext;
        size = machine.upSize;
        if (on && ready && !finished && acked >= size) {
            machine.upFinished = true;
            machine.upOn = false;
            machine.tile.fileNote = "上传完成，等待校验";
            std::string err;
            enqueue(&machine, Messages::packFileDone(id, 1, machine.upSha, &err));
            return;
        }
    }
    if (!on || !ready || finished) {
        return;
    }
    const uint32_t chunk = static_cast<uint32_t>(RemoteConst::kFileChunk);
    const uint32_t window = chunk * static_cast<uint32_t>(RemoteConst::kFileWindow);
    FILE* file = _wfopen(widePath(local).c_str(), L"rb");
    if (!file) {
        std::lock_guard<std::mutex> lock(m_mu);
        const int index = indexOfPeer(peerId);
        if (index >= 0) {
            m_machines[static_cast<size_t>(index)].upOn = false;
            m_machines[static_cast<size_t>(index)].tile.fileNote = "打不开本地文件";
            m_machines[static_cast<size_t>(index)].tile.fileSeq += 1;
        }
        return;
    }
    std::vector<CommBytes> batch;
    std::vector<uint32_t> offsets;
    while (next < size && next - acked < window && batch.size() < static_cast<size_t>(RemoteConst::kFileWindow)) {
        CommBytes data(chunk);
        _fseeki64(file, next, SEEK_SET);
        const size_t got = fread(data.data(), 1, data.size(), file);
        if (got == 0) {
            break;
        }
        data.resize(got);
        offsets.push_back(next);
        batch.push_back(data);
        next += static_cast<uint32_t>(got);
    }
    fclose(file);
    if (batch.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    const int index = indexOfPeer(peerId);
    if (index < 0) {
        return;
    }
    Machine& machine = m_machines[static_cast<size_t>(index)];
    if (!machine.upOn || machine.upNext != offsets.front()) {
        return;
    }
    std::string err;
    for (size_t i = 0; i < batch.size(); ++i) {
        if (machine.out.size() >= 24) {
            break;
        }
        enqueue(&machine, Messages::packFileChunk(id, offsets[i], batch[i], &err));
        machine.upNext = offsets[i] + static_cast<uint32_t>(batch[i].size());
        machine.tile.fileGot = machine.upNext;
    }
}

void ControlSession::fillStation(Tile* tile, const std::string& text)
{
    if (!tile) {
        return;
    }
    tile->sysInfo = text;
    tile->station.clear();
    tile->stationNg = false;
    const std::string mark = "工站";
    const size_t at = text.find(mark);
    if (at != std::string::npos) {
        tile->station = text.substr(at);
    }
    tile->stationNg = text.find("NG") != std::string::npos;
}

std::string ControlSession::listLocalDirectory(const std::wstring& path) const
{
    std::string out;
    if (path.empty()) {
        const DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; ++i) {
            if ((mask & (1u << i)) == 0) {
                continue;
            }
            wchar_t root[4] = { static_cast<wchar_t>(L'A' + i), L':', L'\\', 0 };
            out += "D\t0\t";
            char utf8[8];
            WideCharToMultiByte(CP_UTF8, 0, root, -1, utf8, 8, 0, 0);
            out += utf8;
            out += "\t\n";
        }
        return out;
    }
    std::wstring pattern = path;
    if (!pattern.empty() && pattern.back() != L'\\') {
        pattern += L'\\';
    }
    pattern += L'*';
    WIN32_FIND_DATAW data;
    HANDLE found = FindFirstFileW(pattern.c_str(), &data);
    if (found == INVALID_HANDLE_VALUE) {
        return std::string("E\t0\t打不开目录\n");
    }
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        const bool dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        ULARGE_INTEGER size;
        size.LowPart = data.nFileSizeLow;
        size.HighPart = data.nFileSizeHigh;
        out += link ? "R\t0\t" : (dir ? "D\t0\t" : "F\t");
        if (!dir) {
            out += std::to_string(size.QuadPart);
            out += "\t";
        }
        char name[520];
        WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, name, 520, 0, 0);
        out += name;
        out += "\t";
        SYSTEMTIME utc;
        SYSTEMTIME local;
        if (FileTimeToSystemTime(&data.ftLastWriteTime, &utc)
            && SystemTimeToTzSpecificLocalTime(0, &utc, &local)) {
            char stamp[32];
            sprintf_s(stamp, "%04u-%02u-%02u %02u:%02u",
                local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute);
            out += stamp;
        }
        out += "\n";
        if (out.size() > 200000) {
            out += "E\t0\t还有更多，没有列完\t\n";
            break;
        }
    } while (FindNextFileW(found, &data));
    FindClose(found);
    return out;
}

bool ControlSession::makeLocalDirectory(const std::wstring& path) const
{
    if (path.empty()) {
        return false;
    }
    std::wstring full = path;
    if (!full.empty() && full.back() != L'\\') {
        full.push_back(L'\\');
    }
    for (size_t i = 0; i < full.size(); ++i) {
        if (full[i] != L'\\' || i < 3) {
            continue;
        }
        const std::wstring part = full.substr(0, i);
        CreateDirectoryW(part.c_str(), 0);
    }
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES
        && (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

}
