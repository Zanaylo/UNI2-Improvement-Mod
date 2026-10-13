#include "Hooks/HookStub.h"

#include <cstring>

namespace {

constexpr uint8_t kJmpIndirect[] = { 0xFF, 0x25 };
constexpr uint8_t kPushEax = 0x50;
constexpr uint8_t kMovEaxImm32 = 0xB8;
constexpr uint8_t kLockIncAtEax[] = { 0xF0, 0xFF, 0x00 };
constexpr uint8_t kPopEax = 0x58;
constexpr uint8_t kJmpRel32 = 0xE9;

constexpr size_t kGateSlotAt = 2;
constexpr size_t kCountingAt = 6;
constexpr size_t kCounterAt = kCountingAt + 2;
constexpr size_t kLockIncAt = kCountingAt + 6;
constexpr size_t kPopAt = kLockIncAt + sizeof(kLockIncAtEax);
constexpr size_t kJumpAt = kPopAt + 1;
constexpr size_t kEnd = kJumpAt + 5;

static_assert(kEnd <= HookStub::kBytes, "the stub must fit its slot");

template <typename Value>
void Put(uint8_t* at, Value value)
{
	memcpy(at, &value, sizeof(value));
}

}

void HookStub::Write(uint8_t* stub, void* volatile* gate, volatile LONG* counter, void* handler)
{
	memcpy(stub, kJmpIndirect, sizeof(kJmpIndirect));
	Put(stub + kGateSlotAt, gate);

	stub[kCountingAt] = kPushEax;
	stub[kCountingAt + 1] = kMovEaxImm32;
	Put(stub + kCounterAt, counter);
	memcpy(stub + kLockIncAt, kLockIncAtEax, sizeof(kLockIncAtEax));
	stub[kPopAt] = kPopEax;

	stub[kJumpAt] = kJmpRel32;
	Put(stub + kJumpAt + 1, static_cast<int32_t>(static_cast<uint8_t*>(handler) - (stub + kEnd)));

	FlushInstructionCache(GetCurrentProcess(), stub, kBytes);
}

void* HookStub::CountingEntry(uint8_t* stub)
{
	return stub + kCountingAt;
}

bool HookStub::Contains(const uint8_t* stub, const void* address)
{
	const uint8_t* const at = static_cast<const uint8_t*>(address);

	return at >= stub && at < stub + kBytes;
}
