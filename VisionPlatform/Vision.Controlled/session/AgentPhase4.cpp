/**
 * @file AgentPhase4.cpp
 * @brief 被控端第四期：剪贴板、文件、聊天、电源、隐私屏、系统信息
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "AgentSession.h"

#include <QBuffer>
#include <QImage>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

namespace Vision {

uint64_t AgentSession::hashClipImage(const QImage& image) const
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return 0;
    }
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
    uint64_t hash = 14695981039346656037ull;
    const uint32_t w = static_cast<uint32_t>(rgb.width());
    const uint32_t h = static_cast<uint32_t>(rgb.height());
    hash ^= w;
    hash *= 1099511628211ull;
    hash ^= h;
    hash *= 1099511628211ull;
    for (int y = 0; y < rgb.height(); ++y) {
        const unsigned char* row = rgb.constScanLine(y);
        for (int x = 0; x < rgb.width(); ++x) {
            hash ^= row[x * 4];
            hash *= 1099511628211ull;
            hash ^= row[x * 4 + 1];
            hash *= 1099511628211ull;
            hash ^= row[x * 4 + 2];
            hash *= 1099511628211ull;
        }
    }
    return hash == 0 ? 1 : hash;
}

std::wstring AgentSession::canonicalPath(const std::wstring& path) const
{
    wchar_t full[MAX_PATH * 2];
    DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH * 2, full, 0);
    if (n == 0 || n >= MAX_PATH * 2) {
        return path;
    }
    HANDLE file = CreateFileW(full, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (file == INVALID_HANDLE_VALUE) {
        return full;
    }
    wchar_t finalPath[MAX_PATH * 2];
    const DWORD got = GetFinalPathNameByHandleW(file, finalPath, MAX_PATH * 2, FILE_NAME_NORMALIZED);
    CloseHandle(file);
    if (got == 0 || got >= MAX_PATH * 2) {
        return full;
    }
    std::wstring out = finalPath;
    // \\?\D:\ 只有 7 个字符，盘符根目录以前被长度判断漏掉，整盘都会变成「路径不允许」
    if (out.size() >= 8 && _wcsnicmp(out.c_str(), L"\\\\?\\UNC\\", 8) == 0) {
        out.replace(0, 8, L"\\\\");
    } else if (out.size() >= 6 && out.compare(0, 4, L"\\\\?\\") == 0) {
        out.erase(0, 4);
    }
    return out;
}

bool AgentSession::pathAllowed(const std::wstring& path) const
{
    if (path.empty()) {
        return true;
    }
    if (path.find(L"..") != std::wstring::npos) {
        return false;
    }
    const std::wstring full = canonicalPath(path);
    if (full.size() < 3 || full[1] != L':') {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    const std::vector<std::wstring> roots = m_roots.empty() ? std::vector<std::wstring>(1, L"D:\\") : m_roots;
    for (size_t i = 0; i < roots.size(); ++i) {
        std::wstring root = roots[i];
        if (!root.empty() && root.back() != L'\\') {
            root += L'\\';
        }
        if (full.size() >= root.size() && _wcsnicmp(full.c_str(), root.c_str(), root.size()) == 0) {
            return true;
        }
        if (full.size() + 1 == root.size() && _wcsnicmp((full + L"\\").c_str(), root.c_str(), root.size()) == 0) {
            return true;
        }
    }
    return false;
}

bool AgentSession::makeDirectory(const std::wstring& path) const
{
    if (path.empty() || !pathAllowed(path)) {
        return false;
    }
    std::wstring full = canonicalPath(path);
    if (!full.empty() && full.back() != L'\\') {
        full.push_back(L'\\');
    }
    for (size_t i = 0; i < full.size(); ++i) {
        if (full[i] != L'\\' || i < 3) {
            continue;
        }
        const std::wstring part = full.substr(0, i);
        if (GetFileAttributesW(part.c_str()) != INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        if (!CreateDirectoryW(part.c_str(), 0)) {
            return false;
        }
    }
    if (!full.empty() && full.back() == L'\\') {
        full.pop_back();
    }
    const DWORD attr = GetFileAttributesW(full.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::string AgentSession::listDirectory(const std::wstring& path) const
{
    std::string out;
    if (path.empty()) {
        std::vector<std::wstring> roots;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            roots = m_roots;
        }
        if (roots.empty()) {
            roots.push_back(L"D:\\");
        }
        for (size_t i = 0; i < roots.size(); ++i) {
            out += "D\t0\t";
            out += utf8Full(roots[i]);
            out += "\t\n";
        }
        return out;
    }
    std::wstring pattern = path;
    if (pattern.back() != L'\\') {
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
        out += utf8Full(data.cFileName);
        out += "\t";
        SYSTEMTIME utc;
        SYSTEMTIME local;
        wchar_t stamp[32] = L"";
        if (FileTimeToSystemTime(&data.ftLastWriteTime, &utc)
            && SystemTimeToTzSpecificLocalTime(0, &utc, &local)) {
            swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u",
                local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute);
        }
        out += utf8Full(stamp);
        out += "\n";
        if (out.size() > 200000) {
            out += "E\t0\t还有更多，没有列完\t\n";
            break;
        }
    } while (FindNextFileW(found, &data));
    FindClose(found);
    return out;
}

std::string AgentSession::systemText() const
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    ULARGE_INTEGER freeBytes;
    ULARGE_INTEGER totalBytes;
    freeBytes.QuadPart = 0;
    totalBytes.QuadPart = 0;
    GetDiskFreeSpaceExW(L"C:\\", 0, &totalBytes, &freeBytes);
    std::string text = "CPU " + std::to_string(info.dwNumberOfProcessors) + " 核";
    text += " · 内存 " + std::to_string(mem.ullAvailPhys / (1024 * 1024)) + "/"
        + std::to_string(mem.ullTotalPhys / (1024 * 1024)) + " MB";
    text += " · C: " + std::to_string(freeBytes.QuadPart / (1024 * 1024)) + "/"
        + std::to_string(totalBytes.QuadPart / (1024 * 1024)) + " MB";
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_stationText.empty()) {
        text += " · " + m_stationText;
    }
    return text;
}

void AgentSession::takeNotices(std::vector<Notice>* out)
{
    if (!out) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mu);
    out->swap(m_notices);
    m_notices.clear();
}

bool AgentSession::takePhase4(const Frame::Packet& packet)
{
    std::string err;
    if (packet.channel == Frame::ChChat && packet.type == Frame::TypeChatText) {
        std::string name;
        std::string text;
        uint8_t kind = 0;
        if (!Messages::unpackChat(packet.payload, &name, &text, &kind) || text.empty()) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_mu);
        if (m_notices.size() >= 32) {
            m_notices.erase(m_notices.begin());
        }
        Notice note;
        note.name = name;
        note.text = text;
        note.kind = kind == 1 ? 1 : 0;
        m_notices.push_back(note);
        return true;
    }
    if (packet.channel == Frame::ChClip) {
        bool operating = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            operating = m_operating;
        }
        if (!operating || !allows(PermClipboard)) {
            return true;
        }
        if (packet.type == Frame::TypeClipText) {
            std::string text;
            if (!Messages::unpackClipText(packet.payload, &text)) {
                return true;
            }
            std::wstring wide = toWide(text);
            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_clipText = text;
            }
            if (OpenClipboard(0)) {
                EmptyClipboard();
                const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
                HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                if (mem) {
                    void* dst = GlobalLock(mem);
                    if (dst) {
                        std::memcpy(dst, wide.c_str(), bytes);
                        GlobalUnlock(mem);
                        SetClipboardData(CF_UNICODETEXT, mem);
                    }
                }
                CloseClipboard();
                m_clipSeq = GetClipboardSequenceNumber();
            }
        } else if (packet.type == Frame::TypeClipImage) {
            CommBytes png;
            if (!Messages::unpackClipImage(packet.payload, &png)) {
                return true;
            }
            QImage image;
            image.loadFromData(png.data(), static_cast<int>(png.size()), "PNG");
            if (!image.isNull() && OpenClipboard(0)) {
                EmptyClipboard();
                const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
                const int stride = argb.width() * 4;
                const size_t bytes = sizeof(BITMAPINFOHEADER) + static_cast<size_t>(stride) * static_cast<size_t>(argb.height());
                HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                if (mem) {
                    unsigned char* dst = static_cast<unsigned char*>(GlobalLock(mem));
                    if (dst) {
                        BITMAPINFOHEADER* head = reinterpret_cast<BITMAPINFOHEADER*>(dst);
                        std::memset(head, 0, sizeof(*head));
                        head->biSize = sizeof(BITMAPINFOHEADER);
                        head->biWidth = argb.width();
                        head->biHeight = argb.height();
                        head->biPlanes = 1;
                        head->biBitCount = 32;
                        head->biCompression = BI_RGB;
                        unsigned char* bits = dst + sizeof(BITMAPINFOHEADER);
                        for (int y = 0; y < argb.height(); ++y) {
                            const unsigned char* src = argb.constScanLine(argb.height() - 1 - y);
                            std::memcpy(bits + static_cast<size_t>(y) * static_cast<size_t>(stride), src, static_cast<size_t>(stride));
                        }
                        GlobalUnlock(mem);
                        SetClipboardData(CF_DIB, mem);
                    }
                }
                CloseClipboard();
                m_clipSeq = GetClipboardSequenceNumber();
                const uint64_t hash = hashClipImage(image);
                std::lock_guard<std::mutex> lock(m_mu);
                m_clipImageHash = hash;
            }
        }
        return true;
    }
    if (packet.channel == Frame::ChFile) {
        // 中控台回的目录 / 块 / 完成：本机文件窗用，不要求对方持有操作权
        if (packet.type == Frame::TypeDirEntries) {
            std::string listing;
            if (Messages::unpackDirEntries(packet.payload, &listing)) {
                std::lock_guard<std::mutex> lock(m_mu);
                m_dirList = listing;
                m_dirSeq += 1;
            }
            return true;
        }
        if (packet.type == Frame::TypeFileAck) {
            uint32_t id = 0;
            uint64_t offset = 0;
            if (!Messages::unpackFileAck(packet.payload, &id, &offset)) {
                return true;
            }
            std::lock_guard<std::mutex> fileLock(m_fileMu);
            if (m_filePull && id == m_fileId && offset >= m_fileAcked && offset <= m_fileNext) {
                m_fileAcked = static_cast<uint32_t>(offset);
                std::lock_guard<std::mutex> lock(m_mu);
                m_fileUiGot = offset;
                m_fileUiNote = "正在上传";
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
            std::lock_guard<std::mutex> fileLock(m_fileMu);
            if (!m_file || m_filePull || id != m_fileId) {
                return true;
            }
            if (offset != m_fileOff) {
                return true;
            }
            _fseeki64(m_file, offset, SEEK_SET);
            fwrite(data.data(), 1, data.size(), m_file);
            m_fileOff = offset + static_cast<uint32_t>(data.size());
            enqueue(Messages::packFileAck(id, m_fileOff, &err));
            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_fileUiGot = m_fileOff;
                m_fileUiNote = "正在下载";
            }
            return true;
        }
        if (packet.type == Frame::TypeFileDone) {
            uint32_t id = 0;
            uint8_t ok = 0;
            std::string note;
            Messages::unpackFileDone(packet.payload, &id, &ok, &note);
            std::wstring path;
            std::string sha;
            bool wasPull = false;
            {
                std::lock_guard<std::mutex> fileLock(m_fileMu);
                if (m_file) {
                    fclose(m_file);
                    m_file = 0;
                }
                path = m_filePath;
                sha = m_fileSha;
                wasPull = m_filePull;
                m_filePull = false;
                m_filePath.clear();
                m_fileSha.clear();
            }
            std::string shown = ok ? "完成" : (note.empty() ? "失败" : note);
            // 只有存在 .vrpart 才是接收收尾（上传完成时 path 是本机源文件，不能当接收处理）
            const std::wstring part = path + L".vrpart";
            WIN32_FILE_ATTRIBUTE_DATA partInfo;
            const bool hasPart = !path.empty() && !wasPull
                && GetFileAttributesExW(part.c_str(), GetFileExInfoStandard, &partInfo);
            if (hasPart) {
                std::string got;
                bool match = ok != 0;
                if (match && !sha.empty()) {
                    got = sha256File(part);
                    match = got == sha;
                } else if (match && sha.empty() && note.size() == 64) {
                    got = sha256File(part);
                    match = got == note;
                    sha = note;
                }
                if (match) {
                    MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
                    enqueue(Messages::packFileDone(id, 1, sha.empty() ? sha256File(path) : (got.empty() ? sha : got), &err));
                    shown = "完成";
                } else {
                    DeleteFileW(part.c_str());
                    enqueue(Messages::packFileDone(id, 0, "SHA-256 不一致", &err));
                    shown = "SHA-256 不一致";
                }
            }
            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_fileUiNote = shown;
                m_fileUiSeq += 1;
            }
            return true;
        }
        bool operating = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            operating = m_operating;
        }
        if (!operating || !allows(PermFile)) {
            enqueue(Messages::packFileDone(0, 0, !allows(PermFile) ? "未允许文件" : "需要操作权", &err));
            return true;
        }
        if (packet.type == Frame::TypeListDir) {
            std::string path;
            if (!Messages::unpackListDir(packet.payload, &path)) {
                return true;
            }
            const std::wstring wide = toWide(path);
            if (!pathAllowed(wide)) {
                enqueue(Messages::packDirEntries("E\t0\t路径不允许\n", &err));
                return true;
            }
            enqueue(Messages::packDirEntries(listDirectory(wide), &err));
            return true;
        }
        if (packet.type == Frame::TypeMkDir) {
            uint32_t id = 0;
            std::string path;
            if (!Messages::unpackMkDir(packet.payload, &id, &path)) {
                return true;
            }
            const std::wstring wide = toWide(path);
            const bool ok = makeDirectory(wide);
            enqueue(Messages::packFileDone(id, ok ? 1 : 0, ok ? "完成" : "不能建目录", &err));
            if (!ok) {
                m_audit.add("file.reject", path);
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
            const std::wstring wide = toWide(path);
            if (!pathAllowed(wide) || wide.empty() || size > 0xFFFFFFFFull) {
                enqueue(Messages::packFileDone(id, 0, "路径不允许", &err));
                m_audit.add("file.reject", path);
                return true;
            }
            if (pull == 0) {
                const size_t slash = wide.find_last_of(L'\\');
                if (slash != std::wstring::npos && slash > 2) {
                    makeDirectory(wide.substr(0, slash));
                }
            }
            std::lock_guard<std::mutex> fileLock(m_fileMu);
            if (m_file) {
                fclose(m_file);
                m_file = 0;
            }
            m_filePull = pull != 0;
            m_fileId = id;
            m_filePath = wide;
            m_fileSha = sha;
            if (pull) {
                m_file = _wfopen(wide.c_str(), L"rb");
                if (!m_file) {
                    enqueue(Messages::packFileDone(id, 0, "打不开文件", &err));
                    return true;
                }
                _fseeki64(m_file, 0, SEEK_END);
                m_fileSize = static_cast<uint64_t>(_ftelli64(m_file));
                if (resume > m_fileSize) {
                    resume = 0;
                }
                _fseeki64(m_file, static_cast<int64_t>(resume), SEEK_SET);
                m_fileOff = static_cast<uint32_t>(resume);
                m_fileAcked = m_fileOff;
                m_fileNext = m_fileOff;
            } else {
                const std::wstring part = wide + L".vrpart";
                if (resume == 0) {
                    WIN32_FILE_ATTRIBUTE_DATA info;
                    if (GetFileAttributesExW(part.c_str(), GetFileExInfoStandard, &info)) {
                        ULARGE_INTEGER have;
                        have.LowPart = info.nFileSizeLow;
                        have.HighPart = info.nFileSizeHigh;
                        resume = have.QuadPart;
                    }
                }
                if (resume > size) {
                    resume = 0;
                }
                m_file = _wfopen(part.c_str(), resume == 0 ? L"wb" : L"r+b");
                m_fileSize = size;
                m_fileOff = static_cast<uint32_t>(resume);
                if (!m_file) {
                    enqueue(Messages::packFileDone(id, 0, "不能写入", &err));
                    return true;
                }
                _fseeki64(m_file, static_cast<int64_t>(resume), SEEK_SET);
                enqueue(Messages::packFileAck(id, resume, &err));
            }
            return true;
        }
        return true;
    }
    if (packet.channel == Frame::ChControl && packet.type == Frame::TypeInvite) {
        std::string host;
        uint16_t port = 0;
        if (Messages::unpackInvite(packet.payload, &host, &port)) {
            inviteViewer(host, port == 0 ? RemoteConst::kPort : port);
        }
        return true;
    }
    if (packet.channel == Frame::ChControl && packet.type == Frame::TypePower) {
        uint8_t action = 0;
        if (!Messages::unpackPower(packet.payload, &action)) {
            return true;
        }
        bool operating = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            operating = m_operating;
        }
        if (!operating || !allows(PermPower)) {
            return true;
        }
        if (action == 1) {
            m_audit.add("power.lock", "");
            LockWorkStation();
            return true;
        }
        m_audit.add(action == 2 ? "power.logoff" : (action == 3 ? "power.reboot" : "power.shutdown"), "");
        HANDLE token = 0;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
            TOKEN_PRIVILEGES priv;
            priv.PrivilegeCount = 1;
            priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            LookupPrivilegeValueW(0, L"SeShutdownPrivilege", &priv.Privileges[0].Luid);
            AdjustTokenPrivileges(token, FALSE, &priv, 0, 0, 0);
            CloseHandle(token);
        }
        UINT flags = EWX_LOGOFF;
        if (action == 3) {
            flags = EWX_REBOOT;
        } else if (action == 4) {
            flags = EWX_SHUTDOWN;
        }
        ExitWindowsEx(flags | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
        return true;
    }
    if (packet.channel == Frame::ChControl && packet.type == Frame::TypePrivacy) {
        uint8_t on = 0;
        if (!Messages::unpackPrivacy(packet.payload, &on)) {
            return true;
        }
        bool operating = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            operating = m_operating;
        }
        if (on != 0 && (!operating || !allows(PermPrivacy))) {
            return true;
        }
        std::lock_guard<std::mutex> lock(m_mu);
        m_privacy = on != 0;
        m_audit.add(m_privacy ? "privacy.on" : "privacy.off", "");
        return true;
    }
    return false;
}

void AgentSession::pumpPhase4(std::vector<CommBytes>* extra)
{
    if (!extra) {
        return;
    }
    bool stop = false;
    bool connected = false;
    {
        std::lock_guard<std::mutex> lock(m_mu);
        stop = m_fileStop;
        if (stop) {
            m_fileStop = false;
        }
        connected = m_connectedAt != 0 && m_tcp.isOpen();
    }
    if (stop) {
        std::lock_guard<std::mutex> fileLock(m_fileMu);
        if (m_file) {
            fclose(m_file);
            m_file = 0;
        }
        m_filePull = false;
    }
    if (!connected) {
        return;
    }
    std::string err;
    const uint32_t seq = GetClipboardSequenceNumber();
    if (allows(PermClipboard) && seq != 0 && seq != m_clipSeq && OpenClipboard(0)) {
        m_clipSeq = seq;
        std::string text;
        HANDLE mem = GetClipboardData(CF_UNICODETEXT);
        if (mem) {
            const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(mem));
            if (wide) {
                text = utf8Full(wide);
                GlobalUnlock(mem);
            }
        }
        HANDLE drop = GetClipboardData(CF_HDROP);
        std::string files;
        if (drop) {
            HDROP hdrop = static_cast<HDROP>(drop);
            const UINT count = DragQueryFileW(hdrop, 0xFFFFFFFF, 0, 0);
            for (UINT i = 0; i < count && i < 32; ++i) {
                wchar_t path[MAX_PATH];
                if (DragQueryFileW(hdrop, i, path, MAX_PATH) > 0) {
                    files += utf8Full(path);
                    files += "\n";
                }
            }
        }
        CommBytes png;
        QImage image;
        HANDLE dib = GetClipboardData(CF_DIB);
        if (dib && text.empty() && files.empty()) {
            const BITMAPINFOHEADER* head = static_cast<const BITMAPINFOHEADER*>(GlobalLock(dib));
            if (head && head->biWidth > 0 && head->biBitCount >= 24 && head->biHeight != 0) {
                const int width = head->biWidth;
                const int height = head->biHeight > 0 ? head->biHeight : -head->biHeight;
                const int stride = ((width * head->biBitCount + 31) / 32) * 4;
                const unsigned char* bits = reinterpret_cast<const unsigned char*>(head) + head->biSize;
                image = QImage(width, height, QImage::Format_ARGB32);
                for (int y = 0; y < height; ++y) {
                    const int srcY = head->biHeight > 0 ? height - 1 - y : y;
                    const unsigned char* row = bits + static_cast<size_t>(srcY) * static_cast<size_t>(stride);
                    unsigned char* dst = image.scanLine(y);
                    for (int x = 0; x < width; ++x) {
                        dst[x * 4 + 0] = row[x * (head->biBitCount / 8) + 0];
                        dst[x * 4 + 1] = row[x * (head->biBitCount / 8) + 1];
                        dst[x * 4 + 2] = row[x * (head->biBitCount / 8) + 2];
                        dst[x * 4 + 3] = 255;
                    }
                }
                QByteArray bytes;
                QBuffer buffer(&bytes);
                if (buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG") && bytes.size() <= 512 * 1024) {
                    png.assign(bytes.constData(), bytes.constData() + bytes.size());
                }
            }
            GlobalUnlock(dib);
        }
        CloseClipboard();
        const uint64_t imageHash = hashClipImage(image);
        bool same = false;
        bool knownImage = false;
        {
            std::lock_guard<std::mutex> lock(m_mu);
            same = text == m_clipText;
            if (!text.empty()) {
                m_clipText = text;
            }
            knownImage = imageHash != 0 && imageHash == m_clipImageHash;
            if (!png.empty() && imageHash != 0 && !knownImage) {
                m_clipImageHash = imageHash;
            }
        }
        if (!text.empty() && !same) {
            extra->push_back(Messages::packClipText(text, &err));
        }
        if (!files.empty()) {
            extra->push_back(Messages::packClipFiles(files, &err));
        } else if (!png.empty() && !knownImage) {
            extra->push_back(Messages::packClipImage(png, &err));
        }
    }

    static uint64_t nextInfo = 0;
    const uint64_t now = nowMs();
    if (now >= nextInfo) {
        refreshStation();
        extra->push_back(Messages::packSysInfo(systemText(), &err));
        nextInfo = now + 10000;
    }

    static uint64_t nextViewers = 0;
    if (now >= nextViewers) {
        extra->push_back(Messages::packViewers(viewerReport(), &err));
        nextViewers = now + 2000;
    }

    std::wstring donePath;
    uint32_t doneId = 0;
    bool done = false;
    {
        std::lock_guard<std::mutex> fileLock(m_fileMu);
        if (!m_file || !m_filePull) {
            return;
        }
        const uint32_t chunk = static_cast<uint32_t>(RemoteConst::kFileChunk);
        const uint32_t window = chunk * static_cast<uint32_t>(RemoteConst::kFileWindow);
        while (m_fileNext < m_fileSize && m_fileNext - m_fileAcked < window) {
            CommBytes data(chunk);
            _fseeki64(m_file, m_fileNext, SEEK_SET);
            const size_t got = fread(data.data(), 1, data.size(), m_file);
            if (got == 0) {
                break;
            }
            data.resize(got);
            extra->push_back(Messages::packFileChunk(m_fileId, m_fileNext, data, &err));
            m_fileNext += static_cast<uint32_t>(got);
            {
                std::lock_guard<std::mutex> lock(m_mu);
                m_fileUiGot = m_fileNext;
                m_fileUiBytes = m_fileSize;
                m_fileUiNote = "正在上传";
            }
        }
        if (m_fileAcked >= m_fileSize) {
            donePath = m_filePath;
            doneId = m_fileId;
            fclose(m_file);
            m_file = 0;
            m_filePull = false;
            done = true;
        }
    }
    if (done) {
        extra->push_back(Messages::packFileDone(doneId, 1, sha256File(donePath), &err));
        std::lock_guard<std::mutex> lock(m_mu);
        m_fileUiNote = "上传完成，等待校验";
    }
}

std::string AgentSession::armPushToRemote(uint32_t id, const std::string& localPath, const std::string& remotePath)
{
    const std::wstring wide = toWide(localPath);
    const std::string sha = sha256File(wide);
    if (sha.empty()) {
        std::lock_guard<std::mutex> lock(m_mu);
        m_fileUiNote = "打不开本地文件";
        m_fileUiSeq += 1;
        return std::string();
    }
    WIN32_FILE_ATTRIBUTE_DATA info;
    uint64_t size = 0;
    if (GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &info)) {
        ULARGE_INTEGER u;
        u.LowPart = info.nFileSizeLow;
        u.HighPart = info.nFileSizeHigh;
        size = u.QuadPart;
    }
    {
        std::lock_guard<std::mutex> fileLock(m_fileMu);
        if (m_file) {
            fclose(m_file);
            m_file = 0;
        }
        m_file = _wfopen(wide.c_str(), L"rb");
        if (!m_file) {
            std::lock_guard<std::mutex> lock(m_mu);
            m_fileUiNote = "打不开本地文件";
            m_fileUiSeq += 1;
            return std::string();
        }
        m_filePull = true;
        m_fileId = id;
        m_filePath = wide;
        m_fileSha = sha;
        m_fileSize = size;
        m_fileOff = 0;
        m_fileAcked = 0;
        m_fileNext = 0;
    }
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_fileUiGot = 0;
        m_fileUiBytes = size;
        m_fileUiNote = "正在上传";
    }
    std::string err;
    enqueue(Messages::packFileOpen(id, 0, remotePath, size, &err, sha, 0));
    return sha;
}

void AgentSession::armPullFromRemote(uint32_t id, const std::string& localPath, const std::string& remotePath,
    uint64_t total, uint64_t resume)
{
    const std::wstring wide = toWide(localPath);
    if (wide.empty() || !pathAllowed(wide)) {
        std::lock_guard<std::mutex> lock(m_mu);
        m_fileUiNote = "路径不允许";
        m_fileUiSeq += 1;
        return;
    }
    const size_t slash = wide.find_last_of(L'\\');
    if (slash != std::wstring::npos && slash > 2) {
        makeDirectory(wide.substr(0, slash));
    }
    const std::wstring part = wide + L".vrpart";
    {
        std::lock_guard<std::mutex> fileLock(m_fileMu);
        if (m_file) {
            fclose(m_file);
            m_file = 0;
        }
        if (resume == 0) {
            WIN32_FILE_ATTRIBUTE_DATA info;
            if (GetFileAttributesExW(part.c_str(), GetFileExInfoStandard, &info)) {
                ULARGE_INTEGER have;
                have.LowPart = info.nFileSizeLow;
                have.HighPart = info.nFileSizeHigh;
                resume = have.QuadPart;
            }
        }
        if (total > 0 && resume > total) {
            resume = 0;
        }
        m_file = _wfopen(part.c_str(), resume == 0 ? L"wb" : L"r+b");
        if (!m_file) {
            std::lock_guard<std::mutex> lock(m_mu);
            m_fileUiNote = "不能写入";
            m_fileUiSeq += 1;
            return;
        }
        _fseeki64(m_file, static_cast<int64_t>(resume), SEEK_SET);
        m_filePull = false;
        m_fileId = id;
        m_filePath = wide;
        m_fileSha.clear();
        m_fileSize = total;
        m_fileOff = static_cast<uint32_t>(resume);
        m_fileAcked = m_fileOff;
        m_fileNext = m_fileOff;
    }
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_fileUiGot = resume;
        m_fileUiBytes = total;
        m_fileUiNote = "正在下载";
    }
    std::string err;
    enqueue(Messages::packFileOpen(id, 1, remotePath, 0, &err, std::string(), resume));
}

}
