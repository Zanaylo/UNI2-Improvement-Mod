#include "Core/Harness/HarnessChannel.h"

#include "Core/Config/keycodes.h"
#include "Core/Harness/FrameGrab.h"
#include "Core/Harness/Harness.h"
#include "Core/Harness/InjectedKeys.h"
#include "Core/Harness/QuietWindow.h"
#include "Core/Harness/StageCommands.h"
#include "Core/logger.h"
#include "Game/Display/MovieWindow.h"
#include "Game/Engine/SceneWatch.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr DWORD kBufferBytes = 4096;
constexpr DWORD kGrabTimeoutMs = 5000;
constexpr DWORD kFrameBudgetMs = 100;
constexpr DWORD kKeysBaseTimeoutMs = 3000;
constexpr int kDefaultHoldFrames = 4;
constexpr int kDefaultGapFrames = 8;

using Words = std::vector<std::string>;

Words Split(const std::string& text, char separator)
{
	Words words;
	std::string word;
	std::istringstream stream(text);

	while (std::getline(stream, word, separator))
	{
		if (!word.empty())
			words.push_back(word);
	}

	return words;
}

int ParseKey(const std::string& name)
{
	if (name.empty())
		return 0;

	if (isdigit(static_cast<unsigned char>(name[0])) && name.size() > 1)
		return static_cast<int>(strtol(name.c_str(), nullptr, 0));

	return GetVirtualKeyFromName(name);
}

bool ParseStep(const std::string& token, int holdFrames, int gapFrames, InjectedKeys::Step& outStep,
	std::string& outError)
{
	const size_t star = token.find('*');
	const std::string chord = token.substr(0, star);

	outStep.holdFrames = star == std::string::npos ? holdFrames : atoi(token.c_str() + star + 1);
	outStep.gapFrames = gapFrames;
	outStep.keys.clear();

	for (const std::string& name : Split(chord, '+'))
	{
		const int key = ParseKey(name);

		if (key <= 0 || key > 255)
		{
			outError = "error unknown key " + name;
			return false;
		}

		outStep.keys.push_back(key);
	}

	if (outStep.keys.empty())
	{
		outError = "error empty key in " + token;
		return false;
	}

	return true;
}

bool ReadOption(const std::string& word, const char* name, int& value)
{
	const size_t length = strlen(name);

	if (word.compare(0, length, name) != 0 || word.size() <= length || word[length] != '=')
		return false;

	value = atoi(word.c_str() + length + 1);
	return true;
}

std::string RunKeys(const Words& words)
{
	int holdFrames = kDefaultHoldFrames;
	int gapFrames = kDefaultGapFrames;
	std::vector<InjectedKeys::Step> steps;
	DWORD frames = 0;

	for (size_t i = 1; i < words.size(); ++i)
	{
		if (ReadOption(words[i], "hold", holdFrames) || ReadOption(words[i], "gap", gapFrames))
			continue;

		for (const std::string& token : Split(words[i], ','))
		{
			InjectedKeys::Step step;
			std::string error;

			if (!ParseStep(token, holdFrames, gapFrames, step, error))
				return error;

			frames += step.holdFrames + step.gapFrames;
			steps.push_back(step);
		}
	}

	if (steps.empty())
		return "error keys needs at least one key";

	if (!InjectedKeys::Play(steps, kKeysBaseTimeoutMs + frames * kFrameBudgetMs))
		return "error the game stopped taking frames before the keys were played";

	return "ok " + std::to_string(steps.size());
}

std::string RunGrab(const std::string& line)
{
	const size_t space = line.find(' ');
	if (space == std::string::npos || space + 1 >= line.size())
		return "error grab needs a path";

	std::string reply;
	FrameGrab::Capture(line.substr(space + 1), kGrabTimeoutMs, reply);
	return reply;
}

std::string RunScene()
{
	char text[48] = {};
	sprintf_s(text, "ok %lu held %u", static_cast<unsigned long>(SceneWatch::Current()), SceneWatch::HeldFrames());
	return text;
}

std::string Execute(const std::string& line)
{
	const Words words = Split(line, ' ');

	if (words.empty())
		return "error empty command";

	const std::string& verb = words[0];

	if (verb == "ping")
		return "ok " + std::to_string(GetCurrentProcessId());

	if (verb == "keys")
		return RunKeys(words);

	if (verb == "grab")
		return RunGrab(line);

	if (verb == "scene")
		return RunScene();

	if (verb == "window")
		return std::string("ok ") + QuietWindow::StatusText() + "; " + MovieWindow::StatusText();

	std::string reply;

	if (StageCommands::Execute(words, line, reply))
		return reply;

	return "error unknown command " + verb;
}

bool Reply(HANDLE pipe, const std::string& text)
{
	const std::string line = text + "\n";
	DWORD written = 0;

	return WriteFile(pipe, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) &&
		written == line.size();
}

void Serve(HANDLE pipe)
{
	std::string pending;
	char buffer[kBufferBytes];
	DWORD read = 0;

	while (ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
	{
		pending.append(buffer, read);

		size_t end = 0;

		while ((end = pending.find('\n')) != std::string::npos)
		{
			std::string line = pending.substr(0, end);
			pending.erase(0, end + 1);

			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			if (!Reply(pipe, Execute(line)))
				return;
		}
	}
}

DWORD WINAPI ChannelThread(LPVOID)
{
	for (;;)
	{
		const HANDLE pipe = CreateNamedPipeA(Harness::kPipeName, PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, kBufferBytes,
			kBufferBytes, 0, nullptr);

		if (pipe == INVALID_HANDLE_VALUE)
		{
			LOG("[Harness] could not open %s (error %lu), the harness is off", Harness::kPipeName, GetLastError());
			return 0;
		}

		if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED)
			Serve(pipe);

		DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}
}

}

void HarnessChannel::Start()
{
	const HANDLE thread = CreateThread(nullptr, 0, &ChannelThread, nullptr, 0, nullptr);

	if (thread == nullptr)
	{
		LOG("[Harness] could not start the command thread (error %lu)", GetLastError());
		return;
	}

	CloseHandle(thread);
}
