/**
 * @file PclTmpl.cpp
 * @brief 点云模板 ICP 定位（PCL）
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "PclTmpl.h"
#include "SolutionDataPaths.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsException.h>
#include <HsCommData/HsTuple.h>
#include <cstdio>

#if defined(VISION_HAS_PCL)
#  include <pcl/filters/voxel_grid.h>
#  include <pcl/registration/icp.h>
#endif

namespace Vision {

namespace {
const char kVer[] = "1.0.0.0";
const char kName[] = "locate3d.template";
const VerNoteRaw kNotes[] = {
    { "1.0.0.0", "点云模板 ICP，示教/落盘与 HalTmpl 同构" },
};
}

std::string PclTmpl::tool_name() const { return kName; }
AlgLibKind PclTmpl::alg_lib() const { return AlgLibPcl; }
AlgToolBase* PclTmpl::clone() const { return new PclTmpl(); }
std::string PclTmpl::version() const { return kVer; }
std::vector<VerNote> PclTmpl::version_notes() const
{
    return verNotesFrom(kNotes, static_cast<int>(sizeof(kNotes) / sizeof(kNotes[0])));
}

PclTmpl::PclTmpl()
    : model_ok_(false)
    , tmp_ok_(false)
    , teach_keep_(false)
    , built_(false)
    , pose_org_ok_(false)
    , shot_err_(0)
    , fitness_(0)
{
}

PclTmpl::~PclTmpl() {}

int PclTmpl::tool_reset()
{
    matches_.clear();
    tol_.reset();
    shot_err_ = 0;
    shot_msg_.clear();
    built_ = false;
    fitness_ = 0;
    scene_ = HsCommData::Cloud3();
    teach_cloud_ = HsCommData::Cloud3();
    return HS_OK;
}

std::string PclTmpl::lib_dir(const std::string& toolDataDir) const
{
    return pathJoin(toolDataDir, "pcl");
}

HsCommData::Cloud3 PclTmpl::downsample(const HsCommData::Cloud3& cloud, double leaf) const
{
#if defined(VISION_HAS_PCL)
    if (leaf <= 0.0 || cloud.isEmpty()) {
        return cloud;
    }
    pcl::PointCloud<pcl::PointXYZ>::Ptr pts = ops_.to_xyz(cloud);
    pcl::VoxelGrid<pcl::PointXYZ> vg;
    vg.setInputCloud(pts);
    const float l = static_cast<float>(leaf);
    vg.setLeafSize(l, l, l);
    pcl::PointCloud<pcl::PointXYZ> filtered;
    vg.filter(filtered);
    return ops_.from_xyz(filtered);
#else
    (void)leaf;
    return cloud;
#endif
}

int PclTmpl::proc_input(const AlgToolIn& in)
{
#if !defined(VISION_HAS_PCL)
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaLocate3dTmpl local;
    const ToolParaLocate3dTmpl* para = paraOf(in, &local);
    if (para == nullptr) {
        shot_err_ = HS_ERR_TYPE;
        shot_msg_ = HsCommData::HsException(HS_ERR_TYPE).errorMessage();
        return shot_err_;
    }
    if (para->loadCode() != HS_OK) {
        shot_err_ = para->loadCode();
        shot_msg_ = HsCommData::HsException(shot_err_).errorMessage();
        return shot_err_;
    }

    const HsCommData::Cloud3* scene = nullptr;
    const HsCommData::Cloud3* tmpl = nullptr;
    for (size_t i = 0; i < in.clouds.size(); ++i) {
        const PortCloud& one = in.clouds[i];
        if (one.cloud.isEmpty()) {
            continue;
        }
        if (one.port == Locate3dTmplKey::tmpl || one.port == "template") {
            tmpl = &one.cloud;
        } else if (scene == nullptr
            || one.port.empty()
            || one.port == Locate3dTmplKey::scene
            || one.port == "scene") {
            if (scene == nullptr || one.port == Locate3dTmplKey::scene || one.port == "scene" || one.port.empty()) {
                scene = &one.cloud;
            }
        }
    }
    if (scene == nullptr && !in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) {
        scene = &in.clouds[0].cloud;
    }
    if (tmpl == nullptr) {
        for (size_t i = 0; i < in.clouds.size(); ++i) {
            if (&in.clouds[i].cloud != scene && !in.clouds[i].cloud.isEmpty()) {
                tmpl = &in.clouds[i].cloud;
                break;
            }
        }
    }
    if (scene == nullptr || scene->isEmpty()) {
        shot_err_ = HS_ERR_OPERATOR;
        shot_msg_ = "没有场景点云";
        return shot_err_;
    }
    scene_ = downsample(*scene, para->teach.leafSize);
    if (tmpl != nullptr && !tmpl->isEmpty()) {
        teach_cloud_ = downsample(*tmpl, para->teach.leafSize);
    }
    return HS_OK;
#endif
}

void PclTmpl::keep_teach_snapshot()
{
    if (!tmp_ok_) {
        return;
    }
    model_keep_ = tmp_model_;
    teach_keep_ = true;
}

int PclTmpl::build_model(const AlgToolIn& in)
{
    (void)in;
    if (teach_cloud_.isEmpty()) {
        shot_err_ = HS_ERR_TMPL_NO_TEACH;
        shot_msg_ = "没有模板点云";
        return shot_err_;
    }
    tmp_model_ = teach_cloud_;
    tmp_ok_ = true;
    built_ = true;
    keep_teach_snapshot();
    return HS_OK;
}

int PclTmpl::read_model(HsCommData::Cloud3* cloud, const std::string& path)
{
    if (cloud == nullptr) {
        return HS_ERR_TYPE;
    }
    std::string err;
    if (!ops_.read_pcd(path, cloud, &err)) {
        if (!ops_.read_ply(path, cloud, &err)) {
            return HS_ERR_FILE_OPEN;
        }
    }
    return cloud->isEmpty() ? HS_ERR_FILE_FORMAT : HS_OK;
}

int PclTmpl::write_model(const HsCommData::Cloud3& cloud, const std::string& path)
{
    if (cloud.isEmpty()) {
        return HS_ERR_TMPL_NO_MODEL;
    }
    std::string err;
    if (!ops_.write_pcd(path, cloud, &err)) {
        return HS_ERR_FILE_IO;
    }
    return HS_OK;
}

int PclTmpl::ensure_model(const AlgToolIn& in, const HsCommData::Cloud3** model)
{
    if (model == nullptr) {
        return HS_ERR_TYPE;
    }
    *model = nullptr;
    if (built_ && tmp_ok_) {
        *model = &tmp_model_;
        return HS_OK;
    }
    if (in.mode == RunMode::Learn && tmp_ok_) {
        *model = &tmp_model_;
        return HS_OK;
    }
    if (model_ok_) {
        *model = &model_;
        return HS_OK;
    }
    ToolParaLocate3dTmpl local;
    const ToolParaLocate3dTmpl* para = paraOf(in, &local);
    const std::string name = para != nullptr ? para->teach.modelFile : "model.pcd";
    if (!in.toolDataDir.empty()) {
        const std::string path = pathJoin(lib_dir(in.toolDataDir), name);
        if (read_model(&model_, path) == HS_OK) {
            model_ok_ = true;
            *model = &model_;
            return HS_OK;
        }
        const std::string legacy = pathJoin(in.toolDataDir, name);
        if (read_model(&model_, legacy) == HS_OK) {
            model_ok_ = true;
            *model = &model_;
            return HS_OK;
        }
    }
    if (!teach_cloud_.isEmpty()) {
        tmp_model_ = teach_cloud_;
        tmp_ok_ = true;
        *model = &tmp_model_;
        return HS_OK;
    }
    shot_err_ = HS_ERR_TMPL_NO_MODEL;
    shot_msg_ = "没有模板模型";
    return shot_err_;
}

int PclTmpl::locate(const AlgToolIn& in, const HsCommData::Cloud3& model)
{
#if !defined(VISION_HAS_PCL)
    (void)in;
    (void)model;
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 PCL";
    return shot_err_;
#else
    ToolParaLocate3dTmpl local;
    const ToolParaLocate3dTmpl* para = paraOf(in, &local);
    const double maxCorr = para != nullptr ? para->run.maxCorr : 5.0;
    const int maxIter = para != nullptr ? para->run.maxIter : 50;

    pcl::PointCloud<pcl::PointXYZ>::Ptr src = ops_.to_xyz(scene_);
    pcl::PointCloud<pcl::PointXYZ>::Ptr tgt = ops_.to_xyz(model);
    pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
    icp.setInputSource(src);
    icp.setInputTarget(tgt);
    icp.setMaxCorrespondenceDistance(maxCorr);
    icp.setMaximumIterations(maxIter);
    pcl::PointCloud<pcl::PointXYZ> aligned;
    icp.align(aligned);
    if (!icp.hasConverged()) {
        shot_err_ = HS_ERR_TMPL_NO_MATCH;
        shot_msg_ = "模板匹配失败";
        return shot_err_;
    }
    MatchOne one;
    one.pose_ = ops_.matrix_to_pose(icp.getFinalTransformation());
    one.fitness_ = icp.getFitnessScore();
    fitness_ = one.fitness_;
    matches_.push_back(one);
    return HS_OK;
#endif
}

int PclTmpl::alg_run(const AlgToolIn& in)
{
    const HsCommData::Cloud3* model = nullptr;
    const int code = ensure_model(in, &model);
    if (code != HS_OK || model == nullptr) {
        return code;
    }
    return locate(in, *model);
}

int PclTmpl::judge_tol(const AlgToolIn& in)
{
    ToolParaLocate3dTmpl local;
    const ToolParaLocate3dTmpl* para = paraOf(in, &local);
    tol_.reset();
    if (matches_.empty()) {
        tol_.ok = false;
        return HS_OK;
    }
    const MatchOne& one = matches_[0];
    tol_.put(Locate3dTmplKey::fitness, one.fitness_);
    tol_.put(Locate3dTmplKey::tx, one.pose_.tx);
    tol_.put(Locate3dTmplKey::ty, one.pose_.ty);
    tol_.put(Locate3dTmplKey::tz, one.pose_.tz);
    tol_.put_int(Locate3dTmplKey::count, static_cast<int>(matches_.size()));
    const double minFit = para != nullptr ? para->run.minFitness : 1.0e9;
    if (one.fitness_ > minFit) {
        tol_.ok = false;
        tol_.add_miss(Locate3dTmplKey::fitness, one.fitness_, 0.0, minFit);
    }
    const std::vector<JudgeParam>& rows = para != nullptr ? para->judge : in.judge;
    tol_.judge(rows, nullptr);
    return HS_OK;
}

int PclTmpl::alg_rst_proc(const AlgToolIn&, AlgToolOut* out)
{
    AlgToolOut result;
    result.errCode = shot_err_;
    result.ok = shot_err_ == HS_OK && !matches_.empty() && tol_.ok;
    result.runOk = shot_err_ == HS_OK && !matches_.empty();
    result.message = shot_msg_;
    result.poseOrgOk = pose_org_ok_;
    if (pose_org_ok_) {
        // 三维原点位姿不进 Pose2；用 values 保留
        result.values.insert("poseOrgTx", pose_org_.tx);
        result.values.insert("poseOrgTy", pose_org_.ty);
        result.values.insert("poseOrgTz", pose_org_.tz);
    }
    tol_.apply(&result);
    for (size_t i = 0; i < matches_.size(); ++i) {
        PortPose3 pp;
        pp.index = static_cast<int>(i);
        pp.pose = matches_[i].pose_;
        result.poses3.push_back(pp);
        result.values.insert(Locate3dTmplKey::tx, matches_[i].pose_.tx);
        result.values.insert(Locate3dTmplKey::ty, matches_[i].pose_.ty);
        result.values.insert(Locate3dTmplKey::tz, matches_[i].pose_.tz);
        result.values.insert(Locate3dTmplKey::qx, matches_[i].pose_.qx);
        result.values.insert(Locate3dTmplKey::qy, matches_[i].pose_.qy);
        result.values.insert(Locate3dTmplKey::qz, matches_[i].pose_.qz);
        result.values.insert(Locate3dTmplKey::qw, matches_[i].pose_.qw);
        result.values.insert(Locate3dTmplKey::fitness, matches_[i].fitness_);
    }
    if (!result.values.contains(Locate3dTmplKey::count)) {
        result.values.insert(Locate3dTmplKey::count, static_cast<int>(matches_.size()));
    }
    if (!matches_.empty()) {
        PortTuple tuple;
        HsCommData::HsTuple values;
        const MatchOne& one = matches_[0];
        values.append(one.pose_.tx);
        values.append(one.pose_.ty);
        values.append(one.pose_.tz);
        values.append(one.pose_.qx);
        values.append(one.pose_.qy);
        values.append(one.pose_.qz);
        values.append(one.pose_.qw);
        values.append(one.fitness_);
        tuple.value = values;
        result.tuples.push_back(tuple);
        char msg[128];
        sprintf_s(msg, "fitness %.4f", one.fitness_);
        if (result.message.empty()) {
            result.message = msg;
        }
    }
    *out = result;
    return shot_err_;
}

int PclTmpl::execute(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    run_timer_.start();
    tool_reset();
    run_timer_.mark("tool_reset");
    int stepRet = proc_input(in);
    run_timer_.mark("proc_input");
    if (stepRet == HS_OK) {
        stepRet = alg_run(in);
        run_timer_.mark("alg_run");
    }
    if (stepRet == HS_OK) {
        stepRet = judge_tol(in);
        run_timer_.mark("judge_tol");
    }
    alg_rst_proc(in, out);
    run_timer_.stop("alg_rst_proc");
    write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
    return stepRet != HS_OK ? stepRet : shot_err_;
}

int PclTmpl::teach(const AlgToolIn& in, AlgToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    if (in.toolDataDir.empty()) {
        out->ok = false;
        out->errCode = HS_ERR_TMPL_NO_DIR;
        out->message = HsCommData::HsException(HS_ERR_TMPL_NO_DIR).errorMessage();
        return HS_ERR_TMPL_NO_DIR;
    }
    ensureDirectoryTree(in.toolDataDir);
    tool_reset();
    int step = proc_input(in);
    if (step == HS_OK && teach_cloud_.isEmpty()) {
        shot_err_ = HS_ERR_TMPL_NO_TEACH;
        shot_msg_ = "没有模板点云";
        step = shot_err_;
    }
    if (step == HS_OK) {
        step = build_model(in);
    }
    AlgToolIn teachIn = in;
    teachIn.mode = RunMode::Learn;
    if (step == HS_OK) {
        step = alg_run(teachIn);
    }
    if (step == HS_OK && !matches_.empty()) {
        pose_org_ = matches_[0].pose_;
        pose_org_ok_ = true;
    }
    if (step == HS_OK) {
        judge_tol(teachIn);
    }
    alg_rst_proc(teachIn, out);
    return step != HS_OK ? step : shot_err_;
}

int PclTmpl::action(const std::string& id, const AlgToolIn& in, AlgToolOut* out)
{
    if (id == Locate3dTmplKey::teachModel) {
        return teach(in, out);
    }
    return AlgToolBase::action(id, in, out);
}

void PclTmpl::init(const std::string& toolDataDir)
{
    model_ = HsCommData::Cloud3();
    tmp_model_ = HsCommData::Cloud3();
    model_ok_ = false;
    tmp_ok_ = false;
    teach_keep_ = false;
    pose_org_ok_ = false;
    pose_org_ = HsCommData::Pose3();
    tool_reset();
    if (toolDataDir.empty()) {
        return;
    }
    const std::string dir = lib_dir(toolDataDir);
    if (read_model(&model_, pathJoin(dir, "model.pcd")) == HS_OK
        || read_model(&model_, pathJoin(toolDataDir, "model.pcd")) == HS_OK) {
        model_ok_ = true;
    }
}

void PclTmpl::save(const std::string& toolDataDir)
{
    if (toolDataDir.empty()) {
        return;
    }
    const std::string dir = lib_dir(toolDataDir);
    ensureDirectoryTree(dir);
    if (tmp_ok_) {
        model_ = tmp_model_;
        model_ok_ = true;
        tmp_ok_ = false;
        tmp_model_ = HsCommData::Cloud3();
    }
    if (model_ok_) {
        write_model(model_, pathJoin(dir, "model.pcd"));
    }
    if (teach_keep_ && !model_keep_.isEmpty()) {
        write_model(model_keep_, pathJoin(dir, "model_teach.pcd"));
    }
}

void PclTmpl::clear(const std::string& toolDataDir)
{
    model_ = HsCommData::Cloud3();
    tmp_model_ = HsCommData::Cloud3();
    model_keep_ = HsCommData::Cloud3();
    model_ok_ = false;
    tmp_ok_ = false;
    teach_keep_ = false;
    pose_org_ok_ = false;
    if (!toolDataDir.empty()) {
        removeFileIfExists(pathJoin(lib_dir(toolDataDir), "model.pcd"));
        removeFileIfExists(pathJoin(lib_dir(toolDataDir), "model_teach.pcd"));
    }
}

}
