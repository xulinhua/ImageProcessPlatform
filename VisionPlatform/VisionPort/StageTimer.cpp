/**
 * @file StageTimer.cpp
 * @brief StageTimer实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "StageTimer.h"
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Vision {

// 全零段
StageSpan::StageSpan()
    : ms(0)
    , us(0)
{
}

// 计数归零
StageTimer::OpenSpan::OpenSpan()
    : tick(0)
{
}

namespace {

// QPC 频率。进程内只取一次
int64_t qpcFreq()
{
    static int64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER f;
        if (QueryPerformanceFrequency(&f) && f.QuadPart > 0) {
            freq = f.QuadPart;
        } else {
            freq = -1;
        }
    }
    return freq;
}

// 一行：名字加毫秒，保留三位小数
void appendLine(std::string* out, const std::string& name, double ms)
{
    char buf[64];
    sprintf(buf, "  %.3f ms", ms);
    *out += name;
    *out += buf;
    *out += "\n";
}

} // namespace

// 构造
StageTimer::StageTimer()
{
}

// 清空
void StageTimer::clear()
{
    m_open.clear();
    m_spans.clear();
}

// 当前 QPC 计数
int64_t StageTimer::tick()
{
    if (qpcFreq() <= 0) {
        return 0;
    }
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return c.QuadPart;
}

// 计数差换成毫秒和微秒
void StageTimer::ticksTo(int64_t dt, double* ms, int64_t* us)
{
    const int64_t freq = qpcFreq();
    int64_t micros = 0;
    if (freq > 0 && dt > 0) {
        micros = (dt * 1000000) / freq;
    }
    if (us) {
        *us = micros;
    }
    if (ms) {
        *ms = static_cast<double>(micros) / 1000.0;
    }
}

// 未结束段里同名下标
int StageTimer::findOpen(const std::string& name) const
{
    for (int i = static_cast<int>(m_open.size()) - 1; i >= 0; --i) {
        if (m_open[static_cast<size_t>(i)].name == name) {
            return i;
        }
    }
    return -1;
}

// 按下标结束一段
void StageTimer::closeAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_open.size())) {
        return;
    }
    const OpenSpan open = m_open[static_cast<size_t>(index)];
    m_open.erase(m_open.begin() + index);
    StageSpan span;
    span.name = open.name;
    ticksTo(tick() - open.tick, &span.ms, &span.us);
    m_spans.push_back(span);
}

// 开始一段
void StageTimer::begin(const std::string& name)
{
    const int exist = findOpen(name);
    if (exist >= 0) {
        closeAt(exist);
    }
    OpenSpan open;
    open.name = name;
    open.tick = tick();
    m_open.push_back(open);
}

// 结束一段
void StageTimer::end(const std::string& name)
{
    if (m_open.empty()) {
        return;
    }
    if (name.empty()) {
        closeAt(static_cast<int>(m_open.size()) - 1);
        return;
    }
    closeAt(findOpen(name));
}

// 串行切到下一段
void StageTimer::lap(const std::string& name)
{
    if (!m_open.empty()) {
        closeAt(static_cast<int>(m_open.size()) - 1);
    }
    begin(name);
}

// 某阶段最近一次毫秒
double StageTimer::ms(const std::string& name) const
{
    for (int i = static_cast<int>(m_spans.size()) - 1; i >= 0; --i) {
        if (m_spans[static_cast<size_t>(i)].name == name) {
            return m_spans[static_cast<size_t>(i)].ms;
        }
    }
    return -1;
}

// 某阶段最近一次微秒
int64_t StageTimer::us(const std::string& name) const
{
    for (int i = static_cast<int>(m_spans.size()) - 1; i >= 0; --i) {
        if (m_spans[static_cast<size_t>(i)].name == name) {
            return m_spans[static_cast<size_t>(i)].us;
        }
    }
    return -1;
}

// 已结束片段合计毫秒
double StageTimer::totalMs() const
{
    double sum = 0;
    for (size_t i = 0; i < m_spans.size(); ++i) {
        sum += m_spans[i].ms;
    }
    return sum;
}

// 某一阶段一行
std::string StageTimer::report(const std::string& name) const
{
    const double v = ms(name);
    if (v < 0) {
        return std::string();
    }
    std::string line;
    appendLine(&line, name, v);
    if (!line.empty() && line[line.size() - 1] == '\n') {
        line.erase(line.size() - 1);
    }
    return line;
}

// 全部片段汇总
std::string StageTimer::summary() const
{
    std::string text;
    for (size_t i = 0; i < m_spans.size(); ++i) {
        appendLine(&text, m_spans[i].name, m_spans[i].ms);
    }
    appendLine(&text, "合计", totalMs());
    return text;
}

// 已结束片段，按结束顺序
const std::vector<StageSpan>& StageTimer::spans() const
{
    return m_spans;
}

}