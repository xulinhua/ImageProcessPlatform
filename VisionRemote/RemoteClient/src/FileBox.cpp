/**
 * @file FileBox.cpp
 * @brief 文件通道。块大小和窗口跟 Windows 被控端一样
 */

#include "FileBox.h"

#include "Messages.h"

#include <openssl/evp.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>

namespace Vision {

FileBox::FileBox()
    : m_file(0)
    , m_pull(false)
    , m_id(0)
    , m_size(0)
    , m_off(0)
    , m_acked(0)
    , m_next(0)
    , m_status("空闲")
{
}

FileBox::~FileBox()
{
    closeFile();
}

void FileBox::closeFile()
{
    if (m_file) {
        std::fclose(m_file);
        m_file = 0;
    }
    m_pull = false;
}

void FileBox::reset()
{
    closeFile();
    m_path.clear();
    m_sha.clear();
    m_size = 0;
    m_off = 0;
    m_acked = 0;
    m_next = 0;
    m_status = "空闲";
}

std::string FileBox::status() const
{
    return m_status;
}

bool FileBox::normalize(const std::string& raw, std::string* path) const
{
    std::string text = raw;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\') {
            text[i] = '/';
        }
    }
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '/' && !out.empty() && out[out.size() - 1] == '/') {
            continue;
        }
        out.push_back(text[i]);
    }
    while (out.size() > 1 && out[out.size() - 1] == '/') {
        out.erase(out.size() - 1);
    }
    if (out.empty()) {
        *path = std::string();
        return true;
    }
    if (out[0] != '/') {
        return false;
    }
    std::string piece;
    for (size_t i = 0; i <= out.size(); ++i) {
        if (i == out.size() || out[i] == '/') {
            if (piece == "..") {
                return false;
            }
            piece.clear();
        } else {
            piece.push_back(out[i]);
        }
    }
    *path = out;
    return true;
}

bool FileBox::makeDir(const std::string& path) const
{
    if (path.empty() || path == "/") {
        return true;
    }
    struct stat info;
    if (stat(path.c_str(), &info) == 0) {
        return S_ISDIR(info.st_mode);
    }
    const size_t slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        if (!makeDir(path.substr(0, slash))) {
            return false;
        }
    }
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

std::string FileBox::digest(const std::string& path) const
{
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return std::string();
    }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), 0) != 1) {
        if (ctx) {
            EVP_MD_CTX_free(ctx);
        }
        std::fclose(file);
        return std::string();
    }
    unsigned char buf[8192];
    while (true) {
        const size_t got = std::fread(buf, 1, sizeof(buf), file);
        if (got > 0 && EVP_DigestUpdate(ctx, buf, got) != 1) {
            EVP_MD_CTX_free(ctx);
            std::fclose(file);
            return std::string();
        }
        if (got < sizeof(buf)) {
            break;
        }
    }
    std::fclose(file);
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(ctx, md, &len) != 1) {
        EVP_MD_CTX_free(ctx);
        return std::string();
    }
    EVP_MD_CTX_free(ctx);
    static const char* hex = "0123456789abcdef";
    std::string out(static_cast<size_t>(len) * 2, '0');
    for (unsigned int i = 0; i < len; ++i) {
        out[static_cast<size_t>(i) * 2] = hex[md[i] >> 4];
        out[static_cast<size_t>(i) * 2 + 1] = hex[md[i] & 0x0f];
    }
    return out;
}

std::string FileBox::list(const std::string& path) const
{
    if (path.empty()) {
        return "D\t0\t/\t\n";
    }
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return "E\t0\t打不开目录\n";
    }
    std::string out;
    while (dirent* ent = readdir(dir)) {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        const std::string full = path == "/" ? std::string("/") + ent->d_name : path + "/" + ent->d_name;
        struct stat info;
        if (lstat(full.c_str(), &info) != 0) {
            continue;
        }
        char stamp[64];
        stamp[0] = 0;
        const std::time_t when = info.st_mtime;
        std::tm local;
        if (localtime_r(&when, &local)) {
            std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d",
                local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
        }
        if (S_ISLNK(info.st_mode)) {
            out += "R\t0\t";
        } else if (S_ISDIR(info.st_mode)) {
            out += "D\t0\t";
        } else {
            out += "F\t";
            out += std::to_string(static_cast<unsigned long long>(info.st_size));
            out += "\t";
        }
        out += ent->d_name;
        out += "\t";
        out += stamp;
        out += "\n";
        if (out.size() > 200000) {
            out += "E\t0\t还有更多，没有列完\t\n";
            break;
        }
    }
    closedir(dir);
    return out;
}

std::vector<CommBytes> FileBox::take(const Frame::Packet& packet, bool allow, const std::string& deny)
{
    std::vector<CommBytes> out;
    std::string err;
    if (packet.type == Frame::TypeFileAck) {
        uint32_t id = 0;
        uint64_t offset = 0;
        if (Messages::unpackFileAck(packet.payload, &id, &offset) && m_pull && m_file && id == m_id
            && offset >= m_acked && offset <= m_next) {
            m_acked = static_cast<uint32_t>(offset);
            m_status = "正在上传";
        }
        return out;
    }
    if (packet.type == Frame::TypeFileChunk) {
        uint32_t id = 0;
        uint32_t offset = 0;
        CommBytes data;
        if (!Messages::unpackFileChunk(packet.payload, &id, &offset, &data)) {
            return out;
        }
        if (!m_file || m_pull || id != m_id || offset != m_off) {
            return out;
        }
        if (std::fwrite(data.data(), 1, data.size(), m_file) == data.size()) {
            m_off = offset + static_cast<uint32_t>(data.size());
            out.push_back(Messages::packFileAck(id, m_off, &err));
            m_status = "正在下载";
        }
        return out;
    }
    if (packet.type == Frame::TypeFileDone) {
        uint32_t id = 0;
        uint8_t ok = 0;
        std::string note;
        Messages::unpackFileDone(packet.payload, &id, &ok, &note);
        const std::string path = m_path;
        const std::string sha = m_sha;
        const bool pull = m_pull;
        closeFile();
        m_path.clear();
        m_sha.clear();
        const std::string part = path + ".vrpart";
        struct stat info;
        const bool hasPart = !path.empty() && !pull && stat(part.c_str(), &info) == 0;
        if (hasPart) {
            std::string got;
            bool match = ok != 0;
            if (match && !sha.empty()) {
                got = digest(part);
                match = got == sha;
            } else if (match && sha.empty() && note.size() == 64) {
                got = digest(part);
                match = got == note;
            }
            if (match) {
                std::rename(part.c_str(), path.c_str());
                const std::string shown = got.empty() ? digest(path) : got;
                out.push_back(Messages::packFileDone(id, 1, shown, &err));
                m_status = "完成";
            } else {
                std::remove(part.c_str());
                out.push_back(Messages::packFileDone(id, 0, "SHA-256 不一致", &err));
                m_status = "SHA-256 不一致";
            }
        } else {
            m_status = ok ? "完成" : (note.empty() ? "失败" : note);
        }
        return out;
    }
    if (!allow) {
        const std::string why = deny.empty() ? "需要操作权" : deny;
        out.push_back(Messages::packFileDone(0, 0, why, &err));
        m_status = why;
        return out;
    }
    if (packet.type == Frame::TypeListDir) {
        std::string raw;
        std::string path;
        if (!Messages::unpackListDir(packet.payload, &raw) || !normalize(raw, &path)) {
            out.push_back(Messages::packDirEntries("E\t0\t路径不允许\n", &err));
            return out;
        }
        out.push_back(Messages::packDirEntries(list(path), &err));
        m_status = "已列出目录";
        return out;
    }
    if (packet.type == Frame::TypeMkDir) {
        uint32_t id = 0;
        std::string raw;
        std::string path;
        if (!Messages::unpackMkDir(packet.payload, &id, &raw) || !normalize(raw, &path) || path.empty()) {
            out.push_back(Messages::packFileDone(id, 0, "路径不允许", &err));
            return out;
        }
        const bool ok = makeDir(path);
        out.push_back(Messages::packFileDone(id, ok ? 1 : 0, ok ? "完成" : "不能建目录", &err));
        m_status = ok ? "已建目录" : "不能建目录";
        return out;
    }
    if (packet.type == Frame::TypeFileOpen) {
        uint32_t id = 0;
        uint8_t pull = 0;
        std::string raw;
        uint64_t size = 0;
        std::string sha;
        uint64_t resume = 0;
        if (!Messages::unpackFileOpen(packet.payload, &id, &pull, &raw, &size, &sha, &resume)) {
            return out;
        }
        std::string path;
        if (!normalize(raw, &path) || path.empty() || path == "/" || size > 0xFFFFFFFFull) {
            out.push_back(Messages::packFileDone(id, 0, "路径不允许", &err));
            m_status = "路径不允许";
            return out;
        }
        closeFile();
        m_pull = pull != 0;
        m_id = id;
        m_path = path;
        m_sha = sha;
        if (m_pull) {
            m_file = std::fopen(path.c_str(), "rb");
            if (!m_file) {
                out.push_back(Messages::packFileDone(id, 0, "打不开文件", &err));
                m_status = "打不开文件";
                m_path.clear();
                return out;
            }
            if (std::fseek(m_file, 0, SEEK_END) != 0) {
                closeFile();
                out.push_back(Messages::packFileDone(id, 0, "打不开文件", &err));
                return out;
            }
            const long end = std::ftell(m_file);
            if (end < 0 || static_cast<uint64_t>(end) > 0xFFFFFFFFull) {
                closeFile();
                out.push_back(Messages::packFileDone(id, 0, "文件太大", &err));
                return out;
            }
            m_size = static_cast<uint32_t>(end);
            if (resume > m_size) {
                resume = 0;
            }
            std::fseek(m_file, static_cast<long>(resume), SEEK_SET);
            m_off = static_cast<uint32_t>(resume);
            m_acked = m_off;
            m_next = m_off;
            m_status = "正在上传";
            return out;
        }
        const size_t slash = path.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
            makeDir(path.substr(0, slash));
        }
        const std::string part = path + ".vrpart";
        if (resume == 0) {
            struct stat info;
            if (stat(part.c_str(), &info) == 0 && S_ISREG(info.st_mode)
                && info.st_size >= 0 && static_cast<uint64_t>(info.st_size) <= size) {
                resume = static_cast<uint64_t>(info.st_size);
            }
        }
        if (resume > size) {
            resume = 0;
        }
        m_file = std::fopen(part.c_str(), resume == 0 ? "wb" : "r+b");
        m_size = static_cast<uint32_t>(size);
        m_off = static_cast<uint32_t>(resume);
        m_acked = m_off;
        m_next = m_off;
        if (!m_file) {
            out.push_back(Messages::packFileDone(id, 0, "不能写入", &err));
            m_status = "不能写入";
            return out;
        }
        std::fseek(m_file, static_cast<long>(resume), SEEK_SET);
        out.push_back(Messages::packFileAck(id, resume, &err));
        m_status = "正在下载";
    }
    return out;
}

std::vector<CommBytes> FileBox::pump()
{
    std::vector<CommBytes> out;
    if (!m_file || !m_pull) {
        return out;
    }
    std::string err;
    const uint32_t chunk = static_cast<uint32_t>(RemoteConst::kFileChunk);
    const uint32_t window = chunk * static_cast<uint32_t>(RemoteConst::kFileWindow);
    while (m_next < m_size && m_next - m_acked < window) {
        CommBytes data(chunk);
        if (std::fseek(m_file, static_cast<long>(m_next), SEEK_SET) != 0) {
            break;
        }
        const size_t got = std::fread(data.data(), 1, data.size(), m_file);
        if (got == 0) {
            break;
        }
        data.resize(got);
        out.push_back(Messages::packFileChunk(m_id, m_next, data, &err));
        m_next += static_cast<uint32_t>(got);
        m_status = "正在上传";
    }
    if (m_acked >= m_size) {
        const std::string path = m_path;
        const uint32_t id = m_id;
        closeFile();
        m_path.clear();
        out.push_back(Messages::packFileDone(id, 1, digest(path), &err));
        m_status = "上传完成，等待校验";
    }
    return out;
}

}
