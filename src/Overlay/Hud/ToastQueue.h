#pragma once

#include <deque>
#include <string>

enum class ToastKind
{
	Info,
	Success,
	Warning,
	Error
};

class ToastQueue
{
public:
	static constexpr int kMaxShown = 3;
	static constexpr int kMaxQueued = 8;
	static constexpr float kShowSeconds = 5.0f;
	static constexpr float kFadeSeconds = 0.25f;

	struct Shown
	{
		const char* title;
		const char* text;
		ToastKind kind;
		float alpha;
		float progress;
	};

	void Push(ToastKind kind, const std::string& title, const std::string& text);
	void Update(float seconds);

	int Take(Shown* out, int capacity) const;
	bool IsEmpty() const;

private:
	struct Toast
	{
		ToastKind kind;
		std::string title;
		std::string text;
		float age;
	};

	static float AlphaAt(float age);
	int ShownCount() const;

	std::deque<Toast> m_toasts;
};
