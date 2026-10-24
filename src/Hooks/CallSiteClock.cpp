#include "Hooks/CallSiteClock.h"

#include <cstddef>
#include <cstring>

namespace {

constexpr uint8_t kPushEax = 0x50;
constexpr uint8_t kPushEcx = 0x51;
constexpr uint8_t kPushEdx = 0x52;
constexpr uint8_t kPopEax = 0x58;
constexpr uint8_t kPopEcx = 0x59;
constexpr uint8_t kPopEdx = 0x5A;
constexpr uint8_t kRet = 0xC3;
constexpr uint8_t kNop = 0x90;
constexpr uint8_t kCallRel32 = 0xE8;
constexpr uint8_t kJmpRel32 = 0xE9;
constexpr uint8_t kJneRel8 = 0x75;
constexpr uint8_t kMovEaxThreadId[] = { 0x64, 0xA1, 0x24, 0x00, 0x00, 0x00 };
constexpr uint8_t kCmpEaxMemory[] = { 0x3B, 0x05 };
constexpr uint8_t kCmpMemoryImm8[] = { 0x83, 0x3D };
constexpr uint8_t kMovMemoryImm32[] = { 0xC7, 0x05 };
constexpr uint8_t kRdtsc[] = { 0x0F, 0x31 };
constexpr uint8_t kStoreEax = 0xA3;
constexpr uint8_t kStoreEdx[] = { 0x89, 0x15 };
constexpr uint8_t kPopMemory[] = { 0x8F, 0x05 };
constexpr uint8_t kPushMemory[] = { 0xFF, 0x35 };
constexpr uint8_t kCallMemory[] = { 0xFF, 0x15 };
constexpr uint8_t kJmpMemory[] = { 0xFF, 0x25 };
constexpr uint8_t kSubEaxMemory[] = { 0x2B, 0x05 };
constexpr uint8_t kSbbEdxMemory[] = { 0x1B, 0x15 };
constexpr uint8_t kAddMemoryEax[] = { 0x01, 0x05 };
constexpr uint8_t kAdcMemoryEdx[] = { 0x11, 0x15 };
constexpr uint8_t kIncMemory[] = { 0xFF, 0x05 };
constexpr size_t kRel32Bytes = 4;
constexpr size_t kCallRel32Bytes = 1 + kRel32Bytes;
constexpr ptrdiff_t kLongestShortJump = 127;

class Emitter
{
public:
	explicit Emitter(uint8_t* at)
		: m_start(at)
		, m_at(at)
	{
	}

	template <size_t N>
	void Bytes(const uint8_t (&bytes)[N])
	{
		memcpy(m_at, bytes, N);
		m_at += N;
	}

	void Byte(uint8_t value)
	{
		*m_at++ = value;
	}

	void Dword(uint32_t value)
	{
		memcpy(m_at, &value, sizeof(value));
		m_at += sizeof(value);
	}

	void Address(const volatile void* address)
	{
		Dword(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(address)));
	}

	void Relative(uintptr_t target)
	{
		Dword(static_cast<uint32_t>(target - (reinterpret_cast<uintptr_t>(m_at) + kRel32Bytes)));
	}

	uint8_t* ShortJumpIfNotEqual()
	{
		Byte(kJneRel8);
		Byte(0);
		return m_at;
	}

	bool Land(uint8_t* after)
	{
		const ptrdiff_t distance = m_at - after;
		after[-1] = static_cast<uint8_t>(distance);

		return distance <= kLongestShortJump;
	}

	size_t Size() const
	{
		return static_cast<size_t>(m_at - m_start);
	}

private:
	uint8_t* m_start;
	uint8_t* m_at;
};

void Transfer(Emitter& emit, CallSiteScan::Kind kind, uintptr_t operand, bool isCall)
{
	if (kind == CallSiteScan::Kind_Import)
	{
		if (isCall)
			emit.Bytes(kCallMemory);
		else
			emit.Bytes(kJmpMemory);
		emit.Dword(static_cast<uint32_t>(operand));
		return;
	}

	emit.Byte(isCall ? kCallRel32 : kJmpRel32);
	emit.Relative(operand);
}

void EmitEnter(Emitter& emit, CallSiteClock::Clock* clock)
{
	emit.Bytes(kMovMemoryImm32);
	emit.Address(&clock->busy);
	emit.Dword(1);
	emit.Byte(kPushEdx);
	emit.Bytes(kRdtsc);
	emit.Byte(kStoreEax);
	emit.Address(&clock->start);
	emit.Bytes(kStoreEdx);
	emit.Address(reinterpret_cast<uint8_t*>(&clock->start) + sizeof(uint32_t));
	emit.Byte(kPopEdx);
	emit.Byte(kPopEax);
	emit.Bytes(kPopMemory);
	emit.Address(&clock->returnTo);
}

void EmitCallback(Emitter& emit, CallSiteClock::LeaveCallback onLeave)
{
	if (onLeave == nullptr)
		return;

	emit.Byte(kPushEcx);
	emit.Byte(kCallRel32);
	emit.Relative(reinterpret_cast<uintptr_t>(onLeave));
	emit.Byte(kPopEcx);
}

void EmitLeave(Emitter& emit, CallSiteClock::Clock* clock, CallSiteClock::LeaveCallback onLeave)
{
	uint8_t* const startHigh = reinterpret_cast<uint8_t*>(&clock->start) + sizeof(uint32_t);
	uint8_t* const ticksHigh = reinterpret_cast<uint8_t*>(&clock->ticks) + sizeof(uint32_t);

	emit.Bytes(kPushMemory);
	emit.Address(&clock->returnTo);
	emit.Byte(kPushEax);
	emit.Byte(kPushEdx);
	emit.Bytes(kRdtsc);
	emit.Bytes(kSubEaxMemory);
	emit.Address(&clock->start);
	emit.Bytes(kSbbEdxMemory);
	emit.Address(startHigh);
	emit.Bytes(kAddMemoryEax);
	emit.Address(&clock->ticks);
	emit.Bytes(kAdcMemoryEdx);
	emit.Address(ticksHigh);
	emit.Bytes(kIncMemory);
	emit.Address(&clock->calls);
	emit.Bytes(kMovMemoryImm32);
	emit.Address(&clock->busy);
	emit.Dword(0);
	EmitCallback(emit, onLeave);
	emit.Byte(kPopEdx);
	emit.Byte(kPopEax);
	emit.Byte(kRet);
}

bool WriteCode(uint8_t* at, const uint8_t* bytes, size_t length)
{
	DWORD oldProtect = 0;

	if (!VirtualProtect(at, length, PAGE_EXECUTE_READWRITE, &oldProtect))
		return false;

	memcpy(at, bytes, length);
	VirtualProtect(at, length, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), at, length);
	return true;
}

}

size_t CallSiteClock::Write(uint8_t* thunk, Clock* clock, const volatile DWORD* threadId,
	const CallSiteScan::Site& site, LeaveCallback onLeave)
{
	if (thunk == nullptr || clock == nullptr || threadId == nullptr)
		return 0;

	Emitter emit(thunk);

	emit.Byte(kPushEax);
	emit.Bytes(kMovEaxThreadId);
	emit.Bytes(kCmpEaxMemory);
	emit.Address(threadId);
	uint8_t* const otherThread = emit.ShortJumpIfNotEqual();
	emit.Bytes(kCmpMemoryImm8);
	emit.Address(&clock->busy);
	emit.Byte(0);
	uint8_t* const reentered = emit.ShortJumpIfNotEqual();

	EmitEnter(emit, clock);
	Transfer(emit, site.kind, site.operand, true);
	EmitLeave(emit, clock, onLeave);

	const bool landed = emit.Land(otherThread) && emit.Land(reentered);
	emit.Byte(kPopEax);
	Transfer(emit, site.kind, site.operand, false);

	if (!landed || emit.Size() > kThunkBytes)
		return 0;

	FlushInstructionCache(GetCurrentProcess(), thunk, emit.Size());
	return emit.Size();
}

bool CallSiteClock::Redirect(uint8_t* site, const CallSiteScan::Site& found, const uint8_t* thunk)
{
	if (site == nullptr || thunk == nullptr || found.length < kCallRel32Bytes)
		return false;

	uint8_t code[8] = {};
	code[0] = kCallRel32;

	const uint32_t relative = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(thunk) -
		(reinterpret_cast<uintptr_t>(site) + kCallRel32Bytes));
	memcpy(code + 1, &relative, sizeof(relative));

	for (size_t i = kCallRel32Bytes; i < found.length; ++i)
		code[i] = kNop;

	return WriteCode(site, code, found.length);
}
