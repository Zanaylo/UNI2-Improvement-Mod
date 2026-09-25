#include "Training/Meter/MeterTrace.h"

#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

DiagFrame g_tracePending[FrameMeter::kCapacity] = {};
int g_tracePendingLength = 0;

HANDLE g_traceThread = nullptr;
HANDLE g_traceSignal = nullptr;
volatile LONG g_traceBusy = 0;
volatile LONG g_traceStop = 0;

void WriteTraceCsv(const DiagFrame* frames, int length)
{
	if (length == 0)
		return;

	static int take = 0;

	char name[96] = {};
	sprintf_s(name, "UNI2_IM_meter_%s_%02d.csv", GetLogSessionStamp().c_str(), ++take);

	const std::string path = GetModLogPath(name);

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "w") != 0 || file == nullptr)
	{
		LOG("meter trace: could not open %s", path.c_str());
		return;
	}

	fprintf(file, "frame,bar,recorded,flash,player,pattern,state,actionable,hitstop,stun,"
		"attackBoxes,boxEtc,boxKas,boxNorm,projectile,teching,inReaction,invulnKind,mutekiS,mutekiT,"
		"actionLock,moveCode,command,freeFrom,moveEndAt,moveCodeEx1,moveCodeEx2,moveCodeEx3,moveCodeEx7,"
		"attrInvuln,invuln,atemiBox,hurtboxes,cancelFree,cancelN,cancelS,mvCount,"
		"stance,airJumpOK,posY\n");

	for (int i = 0; i < length; ++i)
	{
		const DiagFrame& d = frames[i];

		for (int p = 0; p < FrameMeter::kPlayers; ++p)
		{
			fprintf(file, "%d,%d,%d,%d,%d,%u,%s,%u,%u,%u,%d,%u,%u,%u,%d,%d,%d,%u,%u,%u,0x%x,0x%x,0x%x,%d,%d,"
				"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,%d,%u,%d,%u,%u,%u,%u,%d,%d\n",
				i, d.barIndex, d.recorded ? 1 : 0, d.flashing ? 1 : 0, p, d.pattern[p],
				FrameMeter::GetStateName(static_cast<FrameMeter::State>(d.state[p])),
				d.actionable[p], d.hitstop[p], d.stun[p], d.attackBoxes[p],
				d.counts[p][0], d.counts[p][1], d.counts[p][2],
				d.projectile[p] ? 1 : 0, d.teching[p] ? 1 : 0, d.inReaction[p] ? 1 : 0,
				d.invulnKind[p], d.muteki[p][0], d.muteki[p][1], d.actionLock[p], d.actionKind[p],
				d.command[p], d.freeFrom[p], d.moveEndAt[p],
				d.moveCodeEx1[p], d.moveCodeEx2[p], d.moveCodeEx3[p], d.moveCodeEx7[p],
				d.attrInvuln[p],
				d.invuln[p], d.atemiBox[p], d.hurtboxes[p], d.cancelFree[p] ? 1 : 0,
				d.cancelNormal[p], d.cancelSpecial[p], d.mvCountFrame[p],
				d.stance[p], d.airJumpOK[p] ? 1 : 0, d.positionY[p]);
		}
	}

	fclose(file);
	LOG("meter trace: wrote %s", path.c_str());
}

DWORD WINAPI TraceWriterThread(LPVOID)
{
	while (WaitForSingleObject(g_traceSignal, INFINITE) == WAIT_OBJECT_0)
	{
		if (InterlockedCompareExchange(&g_traceStop, 0, 0) != 0)
			break;

		WriteTraceCsv(g_tracePending, g_tracePendingLength);
		InterlockedExchange(&g_traceBusy, 0);
	}

	return 0;
}

bool EnsureTraceWriter()
{
	if (g_traceThread != nullptr)
		return true;

	g_traceSignal = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (g_traceSignal == nullptr)
		return false;

	g_traceThread = CreateThread(nullptr, 0, TraceWriterThread, nullptr, 0, nullptr);
	if (g_traceThread == nullptr)
	{
		CloseHandle(g_traceSignal);
		g_traceSignal = nullptr;
		return false;
	}

	return true;
}

}

void MeterTrace::Queue(const DiagFrame* frames, int length)
{
	if (!EnsureTraceWriter())
		return;

	if (InterlockedCompareExchange(&g_traceBusy, 1, 0) != 0)
	{
		LOG("meter trace: writer still busy, exchange dropped");
		return;
	}

	memcpy(g_tracePending, frames, sizeof(DiagFrame) * static_cast<size_t>(length));
	g_tracePendingLength = length;

	SetEvent(g_traceSignal);
}

void MeterTrace::Shutdown()
{
	if (g_traceThread == nullptr)
		return;

	InterlockedExchange(&g_traceStop, 1);
	SetEvent(g_traceSignal);
	WaitForSingleObject(g_traceThread, 300);

	CloseHandle(g_traceThread);
	CloseHandle(g_traceSignal);

	g_traceThread = nullptr;
	g_traceSignal = nullptr;
}
