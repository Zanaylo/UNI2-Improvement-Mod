#pragma once

#include <cstdint>
#include <vector>

namespace BbtagArt
{
	struct Size
	{
		int width;
		int height;
	};

	bool Measure(const std::vector<uint8_t>& blob, Size& out);

	bool Transparent(const std::vector<uint8_t>& blob);

	class Sheet
	{
	public:
		explicit Sheet(const std::vector<uint8_t>& blob);

		bool Lit() const { return m_lit; }
		bool Dark() const { return m_dark; }

		double Peak(double u, double w) const;

	private:
		const uint8_t* m_body;
		size_t m_bytes;
		size_t m_step;
		size_t m_colour;
		bool m_explicit;
		int m_wide;
		int m_high;
		bool m_lit;
		bool m_dark;
	};
}
