#include "Game/Stages/Bbtag/IntroCamera.h"

#include "Game/Stages/Bbtag/BbtagCamera.h"
#include "Game/Stages/Bbtag/BbtagMua.h"
#include "Game/Stages/Bbtag/BbtagPose.h"
#include "Game/Stages/Bbtag/MmotWriter.h"

#include <cstring>

namespace {

constexpr const char* kStagePrefix = "bg_";
constexpr const char* kFileSuffix = "_cam_000.mmot";
constexpr const char* kTakeSuffix = "_cam.DIG";
constexpr const char* kCameraBone = "Bone_camera001";
constexpr float kQuarterTurn = static_cast<float>(BbtagCamera::kPi / 2.0);

std::string Bare(const std::string& stage)
{
	const size_t prefix = strlen(kStagePrefix);

	return stage.compare(0, prefix, kStagePrefix) == 0 ? stage.substr(prefix) : stage;
}

BbtagPose::Pose BattlePose()
{
	BbtagPose::Pose pose = {};
	pose.translation[1] = static_cast<float>(BbtagCamera::kEyeHeight);
	pose.translation[2] = static_cast<float>(-BbtagCamera::kEyeDistance);
	pose.rotation[2] = -kQuarterTurn;
	pose.scale[0] = pose.scale[1] = pose.scale[2] = 1.0f;
	BbtagPose::Turned(pose);

	return pose;
}

MmotWriter::Bone CameraBone()
{
	const BbtagPose::Pose pose = BattlePose();

	BbtagPose::Pose bind = pose;
	memset(bind.translation, 0, sizeof(bind.translation));

	MmotWriter::Bone bone = {};
	BbtagPose::Compose(bind, bone.local);
	BbtagMua::Invert(bone.local, bone.unbind);
	BbtagMua::Identity(bone.parentUnbind);

	for (int frame : { 0, IntroCamera::kFrames })
		MmotWriter::Hold(bone, pose, frame);

	return bone;
}

}

std::string IntroCamera::FileName(const std::string& stage)
{
	return Bare(stage) + kFileSuffix;
}

std::vector<uint8_t> IntroCamera::Still(const std::string& stage)
{
	MmotWriter::Take take;
	take.name = Bare(stage) + kTakeSuffix;
	take.root = kCameraBone;
	take.frames = kFrames;
	take.bones.push_back(CameraBone());

	std::vector<uint8_t> out;
	MmotWriter::Build(take, out);

	return out;
}
