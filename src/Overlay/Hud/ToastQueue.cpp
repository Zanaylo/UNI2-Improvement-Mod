#include "Overlay/Hud/ToastQueue.h"

#include <algorithm>

void ToastQueue::Push(ToastKind kind, const std::string& title, const std::string& text)
{
	if (text.empty())
		return;

	if (static_cast<int>(m_toasts.size()) >= kMaxQueued)
		m_toasts.pop_front();

	m_toasts.push_back({ kind, title, text, 0.0f });
}

void ToastQueue::Update(float seconds)
{
	const int shown = ShownCount();

	for (int index = 0; index < shown; ++index)
		m_toasts[index].age += seconds;

	m_toasts.erase(std::remove_if(m_toasts.begin(), m_toasts.begin() + shown,
		[](const Toast& toast) { return toast.age >= kShowSeconds; }), m_toasts.begin() + shown);
}

int ToastQueue::Take(Shown* out, int capacity) const
{
	const int count = (std::min)(ShownCount(), capacity);

	for (int index = 0; index < count; ++index)
	{
		const Toast& toast = m_toasts[index];
		out[index] = { toast.title.c_str(), toast.text.c_str(), toast.kind, AlphaAt(toast.age),
			(std::min)(toast.age / kShowSeconds, 1.0f) };
	}

	return count;
}

bool ToastQueue::IsEmpty() const
{
	return m_toasts.empty();
}

float ToastQueue::AlphaAt(float age)
{
	const float fadingIn = age / kFadeSeconds;
	const float fadingOut = (kShowSeconds - age) / kFadeSeconds;

	return std::clamp((std::min)(fadingIn, fadingOut), 0.0f, 1.0f);
}

int ToastQueue::ShownCount() const
{
	return (std::min)(static_cast<int>(m_toasts.size()), kMaxShown);
}
