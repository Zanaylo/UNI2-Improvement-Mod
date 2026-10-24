#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace ConfigMigrations
{
	constexpr int kCurrent = 6;

	enum class Kind
	{
		Set,
		SetIfEmpty,
		Remove,
		Rename,
	};

	struct Step
	{
		int revision;
		Kind kind;
		const char* section;
		const char* key;
		const char* value;
		const char* toSection;
		const char* toKey;
		const char* reason;
	};

	const Step* Steps(size_t& outCount);

	inline std::string Note(const Step& step)
	{
		return "[" + std::string(step.section) + "] " + step.key + ": " + step.reason;
	}

	template <typename Store>
	bool RunStep(Store& store, const Step& step)
	{
		std::string value;
		const bool present = store.Find(step.section, step.key, value);

		if (step.kind == Kind::Remove)
		{
			if (!present)
				return false;

			store.Remove(step.section, step.key);
			return true;
		}

		if (step.kind == Kind::Rename)
		{
			if (!present)
				return false;

			std::string existing;

			if (!store.Find(step.toSection, step.toKey, existing))
				store.Set(step.toSection, step.toKey, value);

			store.Remove(step.section, step.key);
			return true;
		}

		if (step.kind == Kind::SetIfEmpty && present && !value.empty())
			return false;

		if (present && value == step.value)
			return false;

		store.Set(step.section, step.key, step.value);
		return true;
	}

	template <typename Store>
	int Run(Store& store, int from, const Step* steps, size_t count, std::vector<std::string>& notes)
	{
		int reached = from;

		for (size_t index = 0; index < count; ++index)
		{
			const Step& step = steps[index];

			if (step.revision > reached)
				reached = step.revision;

			if (step.revision <= from || !RunStep(store, step))
				continue;

			notes.push_back(Note(step));
		}

		return reached;
	}

	inline bool OnlyTidies(const Step& step)
	{
		return step.kind == Kind::Remove || step.kind == Kind::Rename;
	}

	template <typename Store>
	void Cleanup(Store& store, const Step* steps, size_t count, std::vector<std::string>& notes)
	{
		for (size_t index = 0; index < count; ++index)
		{
			const Step& step = steps[index];

			if (!OnlyTidies(step) || !RunStep(store, step))
				continue;

			notes.push_back(Note(step));
		}
	}

	template <typename Store>
	void CleanupAll(Store& store, std::vector<std::string>& notes)
	{
		size_t count = 0;
		const Step* const steps = Steps(count);
		Cleanup(store, steps, count, notes);
	}

	template <typename Store>
	int Apply(Store& store, int from, std::vector<std::string>& notes)
	{
		size_t count = 0;
		const Step* const steps = Steps(count);
		return Run(store, from, steps, count, notes);
	}
}
