/**
 * @file VerifyDb.cpp
 * @brief 验证复核库。四类试验各一套抬头、样件、读数表
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "VerifyDb.h"
#include "SqliteDatabase.h"
#include "AppPaths.h"
#include <mutex>

namespace Vision {

namespace {

std::recursive_mutex g_mu;
SqliteDatabase g_db;

const char* kKinds[] = {"grr", "repeat", "reproduce", "known_point"};

bool knownKind(const std::string& kind)
{
    for (size_t i = 0; i < sizeof(kKinds) / sizeof(kKinds[0]); ++i) {
        if (kind == kKinds[i]) {
            return true;
        }
    }
    return false;
}

bool createKind(const std::string& kind, std::string* error)
{
    const std::string study = kind + "_study";
    const std::string sample = kind + "_sample";
    const std::string trial = kind + "_trial";
    if (!TableRouter::isSafeIdent(study) || !TableRouter::isSafeIdent(sample) || !TableRouter::isSafeIdent(trial)) {
        return false;
    }
    const std::string studySql =
        "CREATE TABLE IF NOT EXISTS " + study + " ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "solution TEXT, station_id TEXT, job_id TEXT, tool_id TEXT, metric TEXT,"
        "operator_name TEXT, spec_low TEXT, spec_high TEXT,"
        "started_at TEXT, ended_at TEXT, summary TEXT, status TEXT);";
    const std::string sampleSql =
        "CREATE TABLE IF NOT EXISTS " + sample + " ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "study_id INTEGER, sample_no INTEGER, barcode TEXT, nominal TEXT);";
    const std::string trialSql =
        "CREATE TABLE IF NOT EXISTS " + trial + " ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "study_id INTEGER, sample_no INTEGER, appraiser TEXT, trial_no INTEGER,"
        "time_iso TEXT, value_num TEXT, value_text TEXT, image_path TEXT, verdict INTEGER);";
    return g_db.exec(studySql, error) && g_db.exec(sampleSql, error) && g_db.exec(trialSql, error);
}

} // namespace

VerifyDb& VerifyDb::instance()
{
    static VerifyDb g;
    return g;
}

bool VerifyDb::open(std::string* error)
{
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    AppPaths::ensureDirs();
    if (!g_db.isOpen()) {
        if (!g_db.open(AppPaths::verifyFile(), error)) {
            return false;
        }
    }
    SchemaMigrator mig(&g_db);
    if (!mig.ensureVersionTable(error)) {
        return false;
    }
    if (mig.currentVersion(error) < 1) {
        for (size_t i = 0; i < sizeof(kKinds) / sizeof(kKinds[0]); ++i) {
            if (!createKind(kKinds[i], error)) {
                return false;
            }
        }
        if (!mig.setVersion(1, error)) {
            return false;
        }
    }
    return true;
}

int64_t VerifyDb::begin(const std::string& kind, const VerifyStudy& study, std::string* error)
{
    if (!knownKind(kind) || !open(error)) {
        return 0;
    }
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(study.solution));
    a.push_back(DbValue::fromText(study.stationId));
    a.push_back(DbValue::fromText(study.jobId));
    a.push_back(DbValue::fromText(study.toolId));
    a.push_back(DbValue::fromText(study.metric));
    a.push_back(DbValue::fromText(study.operatorName));
    a.push_back(DbValue::fromText(study.specLow));
    a.push_back(DbValue::fromText(study.specHigh));
    a.push_back(DbValue::fromText(study.startedAt));
    a.push_back(DbValue::fromText(std::string()));
    a.push_back(DbValue::fromText(std::string()));
    a.push_back(DbValue::fromText("open"));
    const std::string sql =
        "INSERT INTO " + kind + "_study(solution,station_id,job_id,tool_id,metric,operator_name,"
        "spec_low,spec_high,started_at,ended_at,summary,status) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12);";
    if (!g_db.exec(sql, a, error)) {
        return 0;
    }
    return g_db.lastInsertId();
}

int64_t VerifyDb::addSample(const std::string& kind, int64_t studyId, int sampleNo,
                            const std::string& barcode, const std::string& nominal, std::string* error)
{
    if (!knownKind(kind) || studyId <= 0 || !open(error)) {
        return 0;
    }
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    std::vector<DbValue> a;
    a.push_back(DbValue::fromInt(studyId));
    a.push_back(DbValue::fromInt(sampleNo));
    a.push_back(DbValue::fromText(barcode));
    a.push_back(DbValue::fromText(nominal));
    const std::string sql =
        "INSERT INTO " + kind + "_sample(study_id,sample_no,barcode,nominal) VALUES(?1,?2,?3,?4);";
    if (!g_db.exec(sql, a, error)) {
        return 0;
    }
    return g_db.lastInsertId();
}

bool VerifyDb::addTrial(const std::string& kind, const VerifyTrial& trial, std::string* error)
{
    if (!knownKind(kind) || trial.studyId <= 0 || !open(error)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    std::vector<DbValue> a;
    a.push_back(DbValue::fromInt(trial.studyId));
    a.push_back(DbValue::fromInt(trial.sampleNo));
    a.push_back(DbValue::fromText(trial.appraiser));
    a.push_back(DbValue::fromInt(trial.trialNo));
    a.push_back(DbValue::fromText(trial.timeIso));
    a.push_back(DbValue::fromText(trial.valueNum));
    a.push_back(DbValue::fromText(trial.valueText));
    a.push_back(DbValue::fromText(trial.imagePath));
    a.push_back(DbValue::fromInt(trial.ok ? 1 : 0));
    const std::string sql =
        "INSERT INTO " + kind + "_trial(study_id,sample_no,appraiser,trial_no,time_iso,"
        "value_num,value_text,image_path,verdict) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9);";
    return g_db.exec(sql, a, error);
}

bool VerifyDb::finish(const std::string& kind, int64_t studyId, const std::string& summary, std::string* error)
{
    if (!knownKind(kind) || studyId <= 0 || !open(error)) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    std::vector<DbValue> a;
    a.push_back(DbValue::fromText(summary));
    a.push_back(DbValue::fromInt(studyId));
    const std::string sql =
        "UPDATE " + kind + "_study SET summary=?1, status='closed', "
        "ended_at=strftime('%Y-%m-%dT%H:%M:%S','now','localtime') WHERE id=?2;";
    return g_db.exec(sql, a, error);
}

} // namespace Vision
