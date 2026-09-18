#include "D3D9/SceneScale.h"
#include "D3D9/UltrawideRects.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kBaseWidth = 1280;
constexpr int kBaseHeight = 720;
constexpr int kSitesPerAxis = 4;
constexpr int kReassertInterval = 30;

struct Site
{
	uintptr_t address;
	uint32_t original;
};

Site g_width[kSitesPerAxis] = {};
Site g_height[kSitesPerAxis] = {};
int g_widthCount = 0;
int g_heightCount = 0;

constexpr int kStageSiteCount = 9;
constexpr uint32_t kStageViewportWidth = 0x44a00000;

Site g_stage[kStageSiteCount] = {};
int g_stageCount = 0;
uintptr_t g_stageViewport = 0;

constexpr int kBlockLength = GameOffsets::kRenderVirtualCopyBlockLength;

struct Block
{
	uintptr_t address;
	int length;
	uint8_t original[kBlockLength];
	uint8_t replacement[kBlockLength];
};

Block g_pins[3] = {};
int g_pinCount = 0;
bool g_pinsWritten = false;

struct Reference
{
	uintptr_t rva;
	float original;
	float base;
	bool reciprocal;
};

constexpr int kReferenceCount = 6;

Reference g_reference[kReferenceCount] = {};
int g_referenceCount = 0;
int g_referencesTaken = 0;
bool g_referenceWritten = false;
bool g_probed = false;

bool g_installed = false;
bool g_tried = false;
int g_appliedPercent = 100;
bool g_appliedUltrawide = false;
int g_framesUntilReassert = 0;

char g_status[192] = "off";

bool ReadCode(uintptr_t address, uint32_t& out)
{
	return TryReadUnaligned(reinterpret_cast<const void*>(address), out);
}

bool WriteCode(uintptr_t address, uint32_t value)
{
	DWORD previous = 0;
	if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), PAGE_EXECUTE_READWRITE,
		&previous))
	{
		return false;
	}

	memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));

	DWORD restored = 0;
	VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), previous, &restored);
	FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), sizeof(value));
	return true;
}

bool ValidateGlobalWrite(uintptr_t rva, uintptr_t globalRva, uint32_t expected, Site& out)
{
	const uintptr_t address = RvaToAddress(rva);

	uint16_t opcode = 0;
	if (!TryReadMemory(&opcode, reinterpret_cast<const void*>(address), sizeof(opcode)) ||
		opcode != 0x05c7)
	{
		return false;
	}

	uint32_t operand = 0;
	if (!ReadCode(address + 2, operand) ||
		operand != static_cast<uint32_t>(RvaToAddress(globalRva)))
	{
		return false;
	}

	uint32_t immediate = 0;
	if (!ReadCode(address + 6, immediate) || immediate != expected)
		return false;

	out.address = address + 6;
	out.original = immediate;
	return true;
}

bool ValidateLiteral(uintptr_t rva, uint8_t opcode, uint32_t expected, Site& out)
{
	const uintptr_t address = RvaToAddress(rva);

	uint8_t found = 0;
	if (!TryReadMemory(&found, reinterpret_cast<const void*>(address), sizeof(found)) ||
		found != opcode)
	{
		return false;
	}

	uint32_t immediate = 0;
	if (!ReadCode(address + 1, immediate) || immediate != expected)
		return false;

	out.address = address + 1;
	out.original = immediate;
	return true;
}

bool ValidateStore(uintptr_t rva, uint32_t expected, Site& out)
{
	const uintptr_t address = RvaToAddress(rva);

	uint16_t opcode = 0;
	if (!TryReadMemory(&opcode, reinterpret_cast<const void*>(address), sizeof(opcode)) ||
		opcode != 0x05c7)
	{
		return false;
	}

	uint32_t immediate = 0;
	if (!ReadCode(address + 6, immediate) || immediate != expected)
		return false;

	out.address = address + 6;
	out.original = immediate;
	return true;
}

void InstallStageSites()
{
	g_stageCount = 0;

	for (const uintptr_t rva : GameOffsets::kStageTargetWidthStores)
	{
		if (ValidateStore(rva, kBaseWidth, g_stage[g_stageCount]))
			++g_stageCount;
	}

	for (const uintptr_t rva : GameOffsets::kStageTargetWidthArgs)
	{
		if (ValidateLiteral(rva, 0xba, kBaseWidth, g_stage[g_stageCount]))
			++g_stageCount;
	}

	Site viewport = {};
	if (ValidateStore(GameOffsets::kStageViewportWidthStore, kStageViewportWidth, viewport))
	{
		g_stage[g_stageCount++] = viewport;
		g_stageViewport = viewport.address;
	}

	if (g_stageCount == kStageSiteCount)
		return;

	LOG("[SceneScale] %d of %d stage target sites matched, so the stage stays 1280 wide",
		g_stageCount, kStageSiteCount);
	g_stageCount = 0;
	g_stageViewport = 0;
}

void WriteStageSites(bool widened, int width)
{
	float viewportWidth = static_cast<float>(width);
	uint32_t viewportBits = 0;
	memcpy(&viewportBits, &viewportWidth, sizeof(viewportBits));

	for (int i = 0; i < g_stageCount; ++i)
	{
		const Site& site = g_stage[i];
		const uint32_t wanted = site.address == g_stageViewport ? viewportBits
			: static_cast<uint32_t>(width);

		WriteCode(site.address, widened ? wanted : site.original);
	}
}

bool AddPin(uintptr_t address, const uint8_t* expected, const uint8_t* replacement, int length)
{
	uint8_t found[kBlockLength] = {};

	if (g_pinCount >= 3 || !TryReadMemory(found, reinterpret_cast<const void*>(address), length) ||
		memcmp(found, expected, length) != 0)
	{
		return false;
	}

	Block& pin = g_pins[g_pinCount];
	pin.address = address;
	pin.length = length;
	memcpy(pin.original, found, length);
	memcpy(pin.replacement, replacement, length);
	++g_pinCount;
	return true;
}

uint32_t GlobalAt(uintptr_t rva)
{
	return static_cast<uint32_t>(RvaToAddress(rva));
}

bool AddOperandPin(uintptr_t operandRva, uintptr_t fromRva, uintptr_t toRva)
{
	const uint32_t expected = GlobalAt(fromRva);
	const uint32_t replacement = GlobalAt(toRva);

	return AddPin(RvaToAddress(operandRva), reinterpret_cast<const uint8_t*>(&expected),
		reinterpret_cast<const uint8_t*>(&replacement), sizeof(expected));
}

// mov eax,[physicalW] / mov [virtualW],eax and the same for the height, replaced by two
// mov dword ptr [virtual], imm32 of exactly the same twenty bytes - so the virtual pair keeps the
// 1280x720 the game's own coordinates are authored in while the targets grow.
bool AddVirtualPin()
{
	uint8_t expected[kBlockLength] = {};
	uint8_t replacement[kBlockLength] = {};

	const uint32_t physicalW = GlobalAt(GameOffsets::kRenderPhysicalWidth);
	const uint32_t physicalH = GlobalAt(GameOffsets::kRenderPhysicalHeight);
	const uint32_t virtualW = GlobalAt(GameOffsets::kRenderVirtualWidth);
	const uint32_t virtualH = GlobalAt(GameOffsets::kRenderVirtualHeight);

	expected[0] = 0xa1;
	memcpy(expected + 1, &physicalW, 4);
	expected[5] = 0xa3;
	memcpy(expected + 6, &virtualW, 4);
	expected[10] = 0xa1;
	memcpy(expected + 11, &physicalH, 4);
	expected[15] = 0xa3;
	memcpy(expected + 16, &virtualH, 4);

	const uint32_t baseWidth = kBaseWidth;
	const uint32_t baseHeight = kBaseHeight;

	replacement[0] = 0xc7;
	replacement[1] = 0x05;
	memcpy(replacement + 2, &virtualW, 4);
	memcpy(replacement + 6, &baseWidth, 4);
	replacement[10] = 0xc7;
	replacement[11] = 0x05;
	memcpy(replacement + 12, &virtualH, 4);
	memcpy(replacement + 16, &baseHeight, 4);

	return AddPin(RvaToAddress(GameOffsets::kRenderVirtualCopyBlock), expected, replacement,
		kBlockLength);
}

void WritePins(bool pinned)
{
	for (int i = 0; i < g_pinCount; ++i)
	{
		const Block& pin = g_pins[i];
		const uint8_t* bytes = pinned ? pin.replacement : pin.original;

		DWORD previous = 0;
		if (!VirtualProtect(reinterpret_cast<void*>(pin.address), pin.length,
			PAGE_EXECUTE_READWRITE, &previous))
		{
			continue;
		}

		memcpy(reinterpret_cast<void*>(pin.address), bytes, pin.length);

		DWORD restored = 0;
		VirtualProtect(reinterpret_cast<void*>(pin.address), pin.length, previous, &restored);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(pin.address),
			pin.length);
	}

	g_pinsWritten = pinned;
}

bool WriteFloatData(uintptr_t address, float value)
{
	DWORD previous = 0;
	if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), PAGE_READWRITE,
		&previous))
	{
		return false;
	}

	memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));

	DWORD restored = 0;
	VirtualProtect(reinterpret_cast<void*>(address), sizeof(value), previous, &restored);
	return true;
}

bool AddReference(uintptr_t rva, float base, bool reciprocal)
{
	if (g_referenceCount >= kReferenceCount)
		return false;

	uint32_t raw = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(RvaToAddress(rva)), raw))
		return false;

	float value = 0.0f;
	memcpy(&value, &raw, sizeof(value));

	if (value == 0.0f)
		return false;

	const float magnitude = reciprocal ? fabsf(1.0f / value) : fabsf(value);
	if (fabsf(magnitude - base) > 0.01f)
		return false;

	Reference& reference = g_reference[g_referenceCount];
	reference.rva = rva;
	reference.original = value;
	reference.base = base;
	reference.reciprocal = reciprocal;
	++g_referenceCount;
	return true;
}

bool ReferenceEnabled(const Reference& reference)
{
	return reference.reciprocal ? g_modVals.sceneReferenceScale
		: g_modVals.sceneReferenceLiterals;
}

bool ReadFloatData(uintptr_t address, float& out)
{
	uint32_t raw = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(address), raw))
		return false;

	memcpy(&out, &raw, sizeof(out));
	return true;
}

void WriteReferences(int width, int height)
{
	g_referencesTaken = 0;

	for (int i = 0; i < g_referenceCount; ++i)
	{
		const Reference& reference = g_reference[i];
		const bool vertical = reference.base == kBaseHeight / 2.0f;
		const int size = vertical ? height : width;
		const int base = vertical ? kBaseHeight : kBaseWidth;

		const float sign = reference.original < 0.0f ? -1.0f : 1.0f;
		const float scaled = static_cast<float>(size) * 0.5f;
		const float wanted = size == base || !ReferenceEnabled(reference)
			? reference.original
			: sign * (reference.reciprocal ? 1.0f / scaled : scaled);

		const uintptr_t address = RvaToAddress(reference.rva);

		float seen = 0.0f;
		if (WriteFloatData(address, wanted) && ReadFloatData(address, seen) && seen == wanted)
			++g_referencesTaken;
		else
			LOG("[SceneScale] 0x%x refused the write, wanted %g", reference.rva, wanted);
	}

	g_referenceWritten = (width != kBaseWidth || height != kBaseHeight) && g_referencesTaken > 0;
}

int ClampPercent(int percent)
{
	if (percent < 100)
		return 100;

	if (percent > 400)
		return 400;

	return percent;
}

void SizeFor(int percent, int& outWidth, int& outHeight)
{
	outWidth = (kBaseWidth * ClampPercent(percent) / 100 + 3) & ~3;
	outHeight = (kBaseHeight * ClampPercent(percent) / 100 + 3) & ~3;
}

constexpr float kNativeAspect = static_cast<float>(kBaseWidth) / static_cast<float>(kBaseHeight);

bool DesiredAspect(float& outAspect)
{
	int width = g_modVals.ultrawideWidth;
	int height = g_modVals.ultrawideHeight;

	if (width <= 0 || height <= 0)
	{
		width = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
	}

	if (width <= 0 || height <= 0)
		return false;

	outAspect = static_cast<float>(width) / static_cast<float>(height);
	return true;
}

bool WidenForAspect(int height, int& inOutWidth)
{
	float aspect = 0.0f;
	if (!DesiredAspect(aspect) || aspect <= kNativeAspect + 0.01f)
		return false;

	const int widened = (static_cast<int>(height * aspect) + 3) & ~3;
	if (widened <= inOutWidth)
		return false;

	inOutWidth = widened;
	return true;
}

bool WriteFloat(uintptr_t rva, float value)
{
	uint32_t raw = 0;
	memcpy(&raw, &value, sizeof(raw));

	return TryWriteDword(reinterpret_cast<void*>(RvaToAddress(rva)), raw);
}

void WriteDrawScale(float scale)
{
	const uint8_t flag = scale > 0.0f ? 1 : 0;

	if (flag == 0)
	{
		TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kDrawScaleEnabled)), &flag,
			sizeof(flag));
	}

	WriteFloat(GameOffsets::kDrawScaleX, scale);
	WriteFloat(GameOffsets::kDrawScaleY, scale);

	if (flag == 0)
		return;

	TryWriteMemory(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kDrawScaleEnabled)), &flag,
		sizeof(flag));
}

}

bool SceneScale::Install()
{
	if (g_installed)
		return true;

	if (g_tried)
		return false;

	g_tried = true;
	g_widthCount = 0;
	g_heightCount = 0;

	for (const uintptr_t rva : GameOffsets::kRenderSizeWidthWrites)
	{
		if (ValidateGlobalWrite(rva, GameOffsets::kRenderPhysicalWidth, kBaseWidth,
			g_width[g_widthCount]))
		{
			++g_widthCount;
		}
	}

	for (const uintptr_t rva : GameOffsets::kRenderSizeWidthLiterals)
	{
		if (ValidateLiteral(rva, 0xba, kBaseWidth, g_width[g_widthCount]))
			++g_widthCount;
	}

	for (const uintptr_t rva : GameOffsets::kRenderSizeHeightWrites)
	{
		if (ValidateGlobalWrite(rva, GameOffsets::kRenderPhysicalHeight, kBaseHeight,
			g_height[g_heightCount]))
		{
			++g_heightCount;
		}
	}

	for (const uintptr_t rva : GameOffsets::kRenderSizeHeightLiterals)
	{
		if (ValidateLiteral(rva, 0x68, kBaseHeight, g_height[g_heightCount]))
			++g_heightCount;
	}

	if (g_widthCount != kSitesPerAxis || g_heightCount != kSitesPerAxis)
	{
		snprintf(g_status, sizeof(g_status), "%d of %d width and %d of %d height sites matched, "
			"so nothing was changed", g_widthCount, kSitesPerAxis, g_heightCount, kSitesPerAxis);
		LOG("[SceneScale] %s", g_status);
		return false;
	}

	InstallStageSites();

	g_referenceCount = 0;

	for (const uintptr_t rva : GameOffsets::kReferenceHalfWidth)
		AddReference(rva, kBaseWidth / 2.0f, true);

	for (const uintptr_t rva : GameOffsets::kReferenceHalfHeight)
		AddReference(rva, kBaseHeight / 2.0f, true);

	AddReference(GameOffsets::kReferenceHalfWidthLiteral, kBaseWidth / 2.0f, false);
	AddReference(GameOffsets::kReferenceHalfHeightLiteral, kBaseHeight / 2.0f, false);

	if (g_referenceCount != kReferenceCount)
	{
		LOG("[SceneScale] %d of %d reference constants matched, so SceneReferenceScale is off",
			g_referenceCount, kReferenceCount);
		g_referenceCount = 0;
	}

	g_pinCount = 0;

	if (!AddVirtualPin() ||
		!AddOperandPin(GameOffsets::kRenderProjectionWidthOperand,
			GameOffsets::kRenderPhysicalWidth, GameOffsets::kRenderVirtualWidth) ||
		!AddOperandPin(GameOffsets::kRenderProjectionHeightOperand,
			GameOffsets::kRenderPhysicalHeight, GameOffsets::kRenderVirtualHeight))
	{
		LOG("[SceneScale] the projection pin sites did not match, so ScenePinProjection is off");
		g_pinCount = 0;
	}

	g_installed = true;
	return true;
}

void SceneScale::Apply()
{
	const int percent = ClampPercent(g_modVals.sceneScalePercent);
	const bool wantsUltrawide = g_modVals.ultrawideFov;

	if ((percent != 100 || wantsUltrawide) && !Install())
		return;

	if (!g_installed)
		return;

	int width = kBaseWidth;
	int height = kBaseHeight;
	SizeFor(percent, width, height);

	const bool ultrawide = wantsUltrawide && WidenForAspect(height, width);

	for (int i = 0; i < g_widthCount; ++i)
		WriteCode(g_width[i].address, static_cast<uint32_t>(width));

	for (int i = 0; i < g_heightCount; ++i)
		WriteCode(g_height[i].address, static_cast<uint32_t>(height));

	WriteStageSites(ultrawide, width);

	WritePins(!ultrawide && percent != 100 && g_modVals.scenePinProjection && g_pinCount == 3);

	const bool referencesMatched = g_referenceCount == kReferenceCount;
	WriteReferences(referencesMatched ? width : kBaseWidth,
		referencesMatched ? height : kBaseHeight);

	g_appliedPercent = percent;
	g_appliedUltrawide = ultrawide;
	g_framesUntilReassert = 0;

	UltrawideRects::Apply(ultrawide);

	snprintf(g_status, sizeof(g_status), "%dx%d (%d%%)%s%s%s, applies the next time the display "
		"is rebuilt", width, height, percent,
		ultrawide ? ", widened for the display's aspect ratio"
			: "",
		g_pinsWritten ? ", projection pinned to 1280x720" : "",
		g_referenceWritten ? ", reference space scaled with the targets" : "");

	if (g_referenceCount > 0)
	{
		LOG("[SceneScale] %d of %d reference constants took the write", g_referencesTaken,
			g_referenceCount);
	}

	LOG("[SceneScale] %s", g_status);
}

namespace {

void ProbeOnce()
{
	if (g_probed || (g_appliedPercent == 100 && !g_appliedUltrawide))
		return;

	float halfWidth = 0.0f;
	if (!ReadFloatData(RvaToAddress(GameOffsets::kCameraHalfWidth), halfWidth) ||
		halfWidth == kBaseWidth / 2.0f)
	{
		return;
	}

	g_probed = true;

	float values[6] = {};
	const uintptr_t rvas[6] = {
		GameOffsets::kCameraHalfWidth, GameOffsets::kCameraHalfHeight,
		GameOffsets::kCameraRcpHalfWidth, GameOffsets::kCameraRcpHalfHeight,
		GameOffsets::kReferenceHalfWidthLiteral, GameOffsets::kReferenceHalfHeightLiteral,
	};

	for (int i = 0; i < 6; ++i)
		ReadFloatData(RvaToAddress(rvas[i]), values[i]);

	LOG("[SceneScale] camera block halfW %g halfH %g rcpW %g (1/%g) rcpH %g (1/%g)",
		values[0], values[1], values[2], values[2] != 0.0f ? 1.0f / values[2] : 0.0f,
		values[3], values[3] != 0.0f ? 1.0f / values[3] : 0.0f);
	LOG("[SceneScale] literals 640.0 slot %g, 360.0 slot %g", values[4], values[5]);

	for (int i = 0; i < g_referenceCount; ++i)
	{
		float live = 0.0f;
		ReadFloatData(RvaToAddress(g_reference[i].rva), live);
		LOG("[SceneScale] reference 0x%x = %g (1/%g), was 1/%g", g_reference[i].rva, live,
			live != 0.0f ? 1.0f / live : 0.0f, 1.0f / g_reference[i].original);
	}
}

}

void SceneScale::OnFrame()
{
	ProbeOnce();

	if (--g_framesUntilReassert > 0)
		return;

	g_framesUntilReassert = kReassertInterval;

	const float wanted = g_appliedPercent == 100 || !g_modVals.sceneDrawScale ? 0.0f
		: g_appliedPercent / 100.0f;

	uint32_t raw = 0;
	float current = 0.0f;

	if (TryReadDword(reinterpret_cast<const void*>(RvaToAddress(GameOffsets::kDrawScaleX)), raw))
		memcpy(&current, &raw, sizeof(current));

	if (current == wanted)
		return;

	WriteDrawScale(wanted);
}

bool SceneScale::IsApplied()
{
	return g_installed && (g_appliedPercent != 100 || g_appliedUltrawide);
}

bool SceneScale::GetSize(int& outWidth, int& outHeight)
{
	uint32_t width = 0;
	uint32_t height = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(
			RvaToAddress(GameOffsets::kRenderPhysicalWidth)), width) ||
		!TryReadDword(reinterpret_cast<const void*>(
			RvaToAddress(GameOffsets::kRenderPhysicalHeight)), height))
	{
		return false;
	}

	outWidth = static_cast<int>(width);
	outHeight = static_cast<int>(height);
	return true;
}

const char* SceneScale::GetStatusText()
{
	return g_status;
}
