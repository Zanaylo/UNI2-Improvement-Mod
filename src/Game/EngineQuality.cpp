#include "Game/EngineQuality.h"

#include "Core/interfaces.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <cstdio>

namespace {

constexpr int kPollInterval = 60;

uint32_t g_userValue = 1;
bool g_forcing = false;
int g_framesUntilPoll = 0;

char g_status[192] = "not read yet";

uintptr_t FilterAddress()
{
	return RvaToAddress(GameOffsets::kDisplayCharacterQualityUp);
}

bool ReadFilter(uint32_t& out)
{
	return TryReadDword(reinterpret_cast<const void*>(FilterAddress()), out);
}

bool WriteFilter(uint32_t value)
{
	uint32_t current = 0;
	if (ReadFilter(current) && current == value)
		return true;

	return TryWriteDword(reinterpret_cast<void*>(FilterAddress()), value);
}

void FollowUserValue()
{
	uint32_t value = 0;
	if (!ReadFilter(value))
		return;

	g_userValue = value;
}

void SetStatus(const char* text)
{
	snprintf(g_status, sizeof(g_status), "%s", text);
}

}

bool EngineQuality::WantsCharacterFilter()
{
	return !g_modVals.disableCharacterFilter;
}

bool EngineQuality::ReadCharacterFilter(bool& outEnabled)
{
	uint32_t value = 0;
	if (!ReadFilter(value))
		return false;

	outEnabled = value != 0;
	return true;
}

void EngineQuality::Apply()
{
	g_framesUntilPoll = kPollInterval;

	if (WantsCharacterFilter())
	{
		Restore();
		return;
	}

	if (!g_forcing)
		FollowUserValue();

	if (!WriteFilter(0))
	{
		SetStatus("could not write Character Visual Improvements");
		LOG("[EngineQuality] %s", g_status);
		return;
	}

	g_forcing = true;

	SetStatus("Character Visual Improvements held off");
	LOG("[EngineQuality] %s", g_status);
}

void EngineQuality::Restore()
{
	if (!g_forcing)
	{
		SetStatus("Character Visual Improvements left as the game has it");
		return;
	}

	WriteFilter(g_userValue);
	g_forcing = false;

	SetStatus("Character Visual Improvements put back");
	LOG("[EngineQuality] %s", g_status);
}

void EngineQuality::OnFrame()
{
	if (--g_framesUntilPoll > 0)
		return;

	g_framesUntilPoll = kPollInterval;

	if (!g_forcing)
	{
		FollowUserValue();
		return;
	}

	WriteFilter(0);
}

const char* EngineQuality::GetStatusText()
{
	return g_status;
}
