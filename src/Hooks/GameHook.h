#pragma once

#include "Core/utils.h"
#include "Game/Engine/CodeSignatures.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <cstdint>

template <typename Fn>
class GameHook
{
public:
	explicit GameHook(const char* label)
		: m_label(label)
	{
	}

	template <typename Handler>
	bool Install(void* target, Handler handler)
	{
		return Remember(target, HookManager::CreateAndEnableHook(target, reinterpret_cast<void*>(handler),
			reinterpret_cast<void**>(&m_original), m_label));
	}

	template <typename Handler>
	bool Create(void* target, Handler handler)
	{
		return Remember(target, HookManager::CreateHook(target, reinterpret_cast<void*>(handler),
			reinterpret_cast<void**>(&m_original), m_label));
	}

	template <typename Handler>
	bool InstallRva(uintptr_t rva, Handler handler)
	{
		return Install(GameTarget(rva), handler);
	}

	template <typename Handler>
	bool CreateRva(uintptr_t rva, Handler handler)
	{
		return Create(GameTarget(rva), handler);
	}

	template <typename Handler>
	bool InstallApi(const char* module, const char* function, Handler handler)
	{
		const HMODULE owner = GetModuleHandleA(module);
		void* const target = owner == nullptr ? nullptr : reinterpret_cast<void*>(GetProcAddress(owner, function));

		return Create(target, handler);
	}

	bool SetEnabled(bool enabled) const
	{
		return IsLive() && HookManager::SetHookEnabled(m_target, enabled);
	}

	Fn Original() const
	{
		return m_original;
	}

	bool IsLive() const
	{
		return m_original != nullptr;
	}

	long Calls() const
	{
		return m_target == nullptr ? 0 : HookManager::CallCount(m_target);
	}

	void* Target() const
	{
		return m_target;
	}

	const char* Label() const
	{
		return m_label;
	}

private:
	static void* GameTarget(uintptr_t rva)
	{
		const uintptr_t address = CodeSignatures::Address(rva);

		return IsAddressInGameModule(address) ? reinterpret_cast<void*>(address) : nullptr;
	}

	bool Remember(void* target, bool created)
	{
		if (!created)
		{
			m_original = nullptr;
			return false;
		}

		m_target = target;
		return true;
	}

	const char* m_label;
	void* m_target = nullptr;
	Fn m_original = nullptr;
};
