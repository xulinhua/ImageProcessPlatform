/**
 * @file ToolParaBase.cpp
 * @brief ToolParaBase实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolParaBase.h"
#include <cstdio>
#include <sstream>

namespace Vision {

namespace {

void parseVer(const std::string& s, int out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0;
    int idx = 0;
    int cur = 0;
    for (size_t i = 0; i <= s.size() && idx < 4; ++i) {
        const char c = (i < s.size()) ? s[i] : '.';
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
        } else if (c == '.') {
            out[idx++] = cur;
            cur = 0;
        }
    }
}

int cmpVer(const std::string& a, const std::string& b)
{
    int aa[4];
    int bb[4];
    parseVer(a, aa);
    parseVer(b, bb);
    for (int i = 0; i < 4; ++i) {
        if (aa[i] < bb[i]) {
            return -1;
        }
        if (aa[i] > bb[i]) {
            return 1;
        }
    }
    return 0;
}

std::string normVer(const std::string& s)
{
    int v[4];
    parseVer(s.empty() ? std::string("0.0.0.0") : s, v);
    char buf[32];
    sprintf(buf, "%d.%d.%d.%d", v[0], v[1], v[2], v[3]);
    return std::string(buf);
}

std::string stepName(const char* fromVer, const char* toVer)
{
    int a[4];
    int b[4];
    parseVer(fromVer, a);
    parseVer(toVer, b);
    char buf[64];
    sprintf(buf, "AdaptPara_%02d%02d%02d%02d_%02d%02d%02d%02d",
            a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
    return std::string(buf);
}

std::string firstVer(const std::string& s)
{
    if (s.empty() || normVer(s) == "0.0.0.0") {
        return std::string("1.0.0.0");
    }
    return normVer(s);
}

}

int ToolParaBase::compareVersion(const std::string& a, const std::string& b)
{
    return cmpVer(normVer(a), normVer(b));
}

ParaAdaptStep ToolParaBase::adaptStepAt(int) const
{
    return ParaAdaptStep();
}

bool ToolParaBase::applyAdaptStep(int, ToolParam*, AdaptDir)
{
    return false;
}

bool ToolParaBase::adaptParam(ToolParam* src, const std::string& oldVer, const std::string& newVer, std::string* detail)
{
    if (!src) {
        return false;
    }
    const std::string goal = firstVer(newVer.empty() ? std::string(paraVersion()) : newVer);
    std::string at = firstVer(oldVer.empty() ? src->paraVersion : oldVer);
    const AdaptDir dir = cmpVer(at, goal) <= 0 ? AdaptUpgrade : AdaptDowngrade;
    int guard = 0;
    while (cmpVer(at, goal) != 0 && guard < 32) {
        ++guard;
        bool moved = false;
        for (int i = 0; i < adaptStepCount(); ++i) {
            const ParaAdaptStep step = adaptStepAt(i);
            if (!step.fromVer || !step.toVer || step.fromVer[0] == 0) {
                continue;
            }
            const std::string from = normVer(step.fromVer);
            const std::string to = normVer(step.toVer);
            const bool forward = (dir == AdaptUpgrade && from == at && cmpVer(to, at) > 0 && cmpVer(to, goal) <= 0);
            const bool backward = (dir == AdaptDowngrade && to == at && cmpVer(from, at) < 0 && cmpVer(from, goal) >= 0);
            if (!forward && !backward) {
                continue;
            }
            if (!applyAdaptStep(i, src, dir)) {
                if (detail) {
                    *detail = stepName(step.fromVer, step.toVer);
                }
                return false;
            }
            at = forward ? to : from;
            moved = true;
            break;
        }
        if (!moved) {
            if (detail) {
                *detail = std::string("no adapt step");
            }
            return false;
        }
    }
    src->paraVersion = goal;
    if (detail) {
        detail->clear();
    }
    return true;
}

void ToolParaBase::preview(const std::string& fromVer, const std::string& toVer,
                           std::vector<std::string>* names, bool* needConfirm) const
{
    if (needConfirm) {
        *needConfirm = false;
    }
    const std::string goal = firstVer(toVer);
    std::string at = firstVer(fromVer);
    const AdaptDir dir = cmpVer(at, goal) <= 0 ? AdaptUpgrade : AdaptDowngrade;
    int guard = 0;
    while (names && cmpVer(at, goal) != 0 && guard < 32) {
        ++guard;
        bool moved = false;
        for (int i = 0; i < adaptStepCount(); ++i) {
            const ParaAdaptStep step = adaptStepAt(i);
            if (!step.fromVer || !step.toVer || step.fromVer[0] == 0) {
                continue;
            }
            const std::string from = normVer(step.fromVer);
            const std::string to = normVer(step.toVer);
            const bool forward = (dir == AdaptUpgrade && from == at && cmpVer(to, at) > 0 && cmpVer(to, goal) <= 0);
            const bool backward = (dir == AdaptDowngrade && to == at && cmpVer(from, at) < 0 && cmpVer(from, goal) >= 0);
            if (!forward && !backward) {
                continue;
            }
            names->push_back(stepName(step.fromVer, step.toVer));
            if (needConfirm && step.needConfirm) {
                *needConfirm = true;
            }
            at = forward ? to : from;
            moved = true;
            break;
        }
        if (!moved) {
            break;
        }
    }
}

}