#pragma once

namespace Cards
{
	void Problem(const char* text);
	bool TitleWithToggle(const char* title, bool open);

	class Scope
	{
	public:
		explicit Scope(const char* id);
		~Scope();

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

		explicit operator bool() const { return m_visible; }

	private:
		bool m_visible;
	};
}
