#include "Training/Meter/FrameMeter.h"

#include "Game/Engine/PlayerState.h"

namespace {

constexpr uint32_t Argb(uint32_t r, uint32_t g, uint32_t b)
{
	return (255u << 24) | (r << 16) | (g << 8) | b;
}

}

const char* FrameMeter::GetStateName(State state)
{
	switch (state)
	{
	case State::Idle:      return "Free";
	case State::Startup:   return "Startup";
	case State::Armor:     return "Armor";
	case State::Active:    return "Active";
	case State::Recovery:  return "Recovery";
	case State::Cancellable: return "Cancellable";
	case State::Blockstun: return "Blockstun";
	case State::Hitstun:   return "Hitstun";
	case State::Parry:     return "Parry";
	case State::Movement:  return "Dash";
	case State::Backdash:  return "Backdash";
	case State::Jump:      return "Jump";
	case State::AirMovement: return "Air Movement";
	case State::Dodge:     return "Dodge";
	default:               return "";
	}
}

uint32_t FrameMeter::GetStateColor(State state)
{
	switch (state)
	{
	case State::Idle:      return Argb(64, 72, 86);
	case State::Startup:   return Argb(64, 182, 226);
	case State::Armor:     return Argb(158, 92, 214);
	case State::Active:    return Argb(214, 48, 52);
	case State::Recovery:  return Argb(46, 84, 176);

	case State::Cancellable: return Argb(64, 72, 86);
	case State::Blockstun: return Argb(86, 176, 126);
	case State::Hitstun:   return Argb(226, 132, 48);

	case State::Parry:     return Argb(96, 58, 146);

	case State::Movement:  return Argb(212, 184, 72);
	case State::Backdash:  return Argb(150, 200, 70);

	case State::Jump:        return Argb(36, 132, 180);
	case State::AirMovement: return Argb(112, 124, 236);

	case State::Dodge:       return Argb(186, 74, 168);
	default:               return Argb(20, 22, 28);
	}
}

const char* FrameMeter::GetMarkerName(Marker marker)
{
	switch (marker)
	{
	case Marker_Projectile:       return "Projectile Active";
	case Marker_Tech:             return "Teching";
	case Marker_FullInvuln:       return "Everything";
	case Marker_StrikeInvuln:     return "Strike";
	case Marker_ThrowInvuln:      return "Throw";
	case Marker_ProjectileInvuln: return "Projectile";
	case Marker_HeadInvuln:       return "Head";
	case Marker_BodyInvuln:       return "Body";
	case Marker_LegsInvuln:       return "Foot";
	case Marker_DiveInvuln:       return "Air";
	case Marker_HighMidInvuln:    return "High and Mid";
	case Marker_LowMidInvuln:     return "Low and Mid";
	default:                      return "";
	}
}

uint16_t FrameMeter::GetMarkerInvulnBit(Marker marker)
{
	switch (marker)
	{
	case Marker_FullInvuln:       return PlayerState::Invuln_Full;
	case Marker_StrikeInvuln:     return PlayerState::Invuln_Strike;
	case Marker_ThrowInvuln:      return PlayerState::Invuln_Throw;
	case Marker_ProjectileInvuln: return PlayerState::Invuln_Projectile;
	case Marker_HeadInvuln:       return PlayerState::Invuln_Head;
	case Marker_BodyInvuln:       return PlayerState::Invuln_Body;
	case Marker_LegsInvuln:       return PlayerState::Invuln_Legs;
	case Marker_DiveInvuln:       return PlayerState::Invuln_Dive;
	case Marker_HighMidInvuln:    return PlayerState::Invuln_HighMid;
	case Marker_LowMidInvuln:     return PlayerState::Invuln_LowMid;
	default:                      return 0;
	}
}

uint32_t FrameMeter::GetMarkerColor(Marker marker)
{
	switch (marker)
	{
	case Marker_Projectile:       return Argb(240, 170, 40);
	case Marker_Tech:             return Argb(120, 240, 190);

	case Marker_FullInvuln:       return Argb(255, 255, 255);

	case Marker_StrikeInvuln:     return Argb(60, 140, 255);
	case Marker_ThrowInvuln:      return Argb(255, 210, 74);

	case Marker_ProjectileInvuln: return Argb(0, 200, 170);
	case Marker_HeadInvuln:       return Argb(255, 122, 47);
	case Marker_BodyInvuln:       return Argb(124, 224, 74);
	case Marker_LegsInvuln:       return Argb(180, 90, 255);
	case Marker_DiveInvuln:       return Argb(255, 79, 208);
	case Marker_HighMidInvuln:    return Argb(90, 200, 220);
	case Marker_LowMidInvuln:     return Argb(220, 60, 60);
	default:                      return Argb(255, 255, 255);
	}
}

uint8_t FrameMeter::GetAttackMarkBit(AttackMark mark)
{
	return static_cast<uint8_t>(1u << mark);
}

const char* FrameMeter::GetAttackMarkName(AttackMark mark)
{
	switch (mark)
	{
	case AttackMark_Head: return "Head";
	case AttackMark_Foot: return "Foot";
	case AttackMark_Air:  return "Air";
	default:              return "";
	}
}

uint32_t FrameMeter::GetAttackMarkColor(AttackMark mark)
{
	switch (mark)
	{
	case AttackMark_Head: return GetMarkerColor(Marker_HeadInvuln);
	case AttackMark_Foot: return GetMarkerColor(Marker_LegsInvuln);
	case AttackMark_Air:  return GetMarkerColor(Marker_DiveInvuln);
	default:              return Argb(255, 255, 255);
	}
}
