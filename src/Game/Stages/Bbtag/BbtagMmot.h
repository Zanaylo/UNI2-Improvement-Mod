#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace BbtagMmot
{
	class Motion
	{
	public:
		bool Read(const std::vector<uint8_t>& blob);

		const std::string& Target() const { return m_target; }
		int Bones() const { return m_bones; }
		int Frames() const { return m_frames; }

		void Sample(int bone, int frame, const float restTranslate[3], const float restRotate[3],
			const float restScale[3], float translate[3], float rotate[3], float scale[3]) const;

		enum class Kind { Translation, Rotation, Scale };

		void Reset(int bones, int frames);
		void Add(int bone, Kind kind, const float value[3], int frame);

	private:
		struct Track
		{
			std::map<int, std::vector<float> > key;
		};

		struct BoneTracks
		{
			Track translation;
			Track rotation;
			Track scale;
		};

		void Value(const Track& track, int frame, const float fallback[3], float out[3]) const;

		std::string m_target;
		int m_bones = 0;
		int m_frames = 0;
		std::vector<BoneTracks> m_track;
	};

	void Compose(const float translate[3], const float rotate[3], const float scale[3],
		float out[16]);
}
