#include "Overlay/Guides/FrameMeterLegend.h"

#include "Training/Meter/FrameMeter.h"

namespace {

using FrameMeter::State;

uint32_t Band(State state)
{
	return FrameMeter::GetStateColor(state);
}

uint32_t Mark(FrameMeter::Marker marker)
{
	return FrameMeter::GetMarkerColor(marker);
}

uint32_t Attack(FrameMeter::AttackMark mark)
{
	return FrameMeter::GetAttackMarkColor(mark);
}

template <int N>
GuidePage Page(const char* title, const char* heading, const GuideRow (&rows)[N])
{
	return { title, heading, rows, N };
}

struct Legend
{
	GuideRow overview[6];
	GuideRow bar[15];
	GuideRow attack[4];
	GuideRow invuln[11];
	GuideRow numbers[13];
	GuidePage pages[5];
	GuideContent content;

	Legend()
		: overview{
			{ kGuideNoSwatch, "Turning it on", "This menu, the hotkey or the F1 overlay",
				"Turn it on here, with the frame meter hotkey (F3 unless it was changed) or from the "
				"Training section of the F1 overlay. Offline only: it turns itself off online." },
			{ kGuideNoSwatch, "One exchange", "Recorded, then held on screen",
				"The meter does not scroll. It records one exchange, from the moment either player stops "
				"being free until both are free again, and holds it until the next one starts." },
			{ kGuideNoSwatch, "Bar", "One cell per frame",
				"The middle row of each player. One cell per frame, coloured by what the character is "
				"doing. The Bar page lists every colour." },
			{ kGuideNoSwatch, "Attack Row", "Above the bar, active frames only",
				"Head, Foot and Air: the properties of the attack out on that frame. The Attack Row page "
				"lists them." },
			{ kGuideNoSwatch, "Invincibility Row", "Under the bar",
				"What cannot hit the character on that frame. It shares the attack row's colours, so a "
				"Head attack whiffs on a Head cell." },
			{ kGuideNoSwatch, "Numbers", "Above and below the bars",
				"Startup, total and advantage of the last move, then the stun, gap, flash and damage "
				"lines. The Numbers page explains each one." } },
		bar{
			{ Band(State::Startup), FrameMeter::GetStateName(State::Startup), "Cannot connect yet",
				"The move has committed and its attack cannot connect yet." },
			{ Band(State::Active), FrameMeter::GetStateName(State::Active), "An attack box is out",
				"An attack box is out. The only band that can connect." },
			{ Band(State::Recovery), FrameMeter::GetStateName(State::Recovery), "Over, not free yet",
				"The attack is over and the move cannot be left yet." },
			{ Band(State::Cancellable), FrameMeter::GetStateName(State::Cancellable),
				"Recovery that can be cancelled",
				"The tail of a recovery the move can be cancelled out of. Drawn in the Free colour, but "
				"still counted as part of the move." },
			{ Band(State::Blockstun), FrameMeter::GetStateName(State::Blockstun), "Held after blocking",
				"Held in block after a guarded hit." },
			{ Band(State::Hitstun), FrameMeter::GetStateName(State::Hitstun), "Held in a hit reaction",
				"Held in a hit reaction." },
			{ Band(State::Parry), FrameMeter::GetStateName(State::Parry), "Counter or shield window",
				"A counter or shield window is open." },
			{ Band(State::Dodge), FrameMeter::GetStateName(State::Dodge), "Creeping Edge",
				"Creeping Edge." },
			{ Band(State::Movement), FrameMeter::GetStateName(State::Movement), "Forward dash",
				"A forward dash on the ground." },
			{ Band(State::Backdash), FrameMeter::GetStateName(State::Backdash), "Ground or air backdash",
				"A backdash, on the ground or in the air." },
			{ Band(State::Jump), FrameMeter::GetStateName(State::Jump), "Off the ground",
				"Off the ground under their own power." },
			{ Band(State::AirMovement), FrameMeter::GetStateName(State::AirMovement),
				"Assault or air dash", "An assault, an air dash or an air backdash." },
			{ Band(State::Idle), FrameMeter::GetStateName(State::Idle), "Free to act",
				"Free to act: nothing is running and every cancel is open." },
			{ Mark(FrameMeter::Marker_Projectile), FrameMeter::GetMarkerName(FrameMeter::Marker_Projectile),
				"A projectile of theirs is out",
				"A projectile this character owns has an attack box out. The owner's own band shows as "
				"a strip on top of the cell." },
			{ Mark(FrameMeter::Marker_Tech), FrameMeter::GetMarkerName(FrameMeter::Marker_Tech),
				"Recovering", "Air, ground or emergency recovery. Fills the whole cell." } },
		attack{
			{ Attack(FrameMeter::AttackMark_Head), FrameMeter::GetAttackMarkName(FrameMeter::AttackMark_Head),
				"Whiffs on head invincibility",
				"The attack whiffs against a character with head invincibility." },
			{ Attack(FrameMeter::AttackMark_Foot), FrameMeter::GetAttackMarkName(FrameMeter::AttackMark_Foot),
				"Whiffs on foot invincibility",
				"The attack whiffs against a character with foot invincibility." },
			{ Attack(FrameMeter::AttackMark_Air), FrameMeter::GetAttackMarkName(FrameMeter::AttackMark_Air),
				"Whiffs on air invincibility",
				"The attack whiffs against a character with air invincibility." },
			{ kGuideNoSwatch, "Two or more", "Split the cell",
				"An attack with two or more properties splits the cell between their colours." } },
		invuln{
			{ Mark(FrameMeter::Marker_FullInvuln), FrameMeter::GetMarkerName(FrameMeter::Marker_FullInvuln),
				"Nothing can hit", "Nothing can connect at all. Drawn alone." },
			{ Mark(FrameMeter::Marker_StrikeInvuln),
				FrameMeter::GetMarkerName(FrameMeter::Marker_StrikeInvuln), "Strikes whiff",
				"Strikes cannot connect." },
			{ Mark(FrameMeter::Marker_ThrowInvuln), FrameMeter::GetMarkerName(FrameMeter::Marker_ThrowInvuln),
				"Throws whiff", "Throws cannot connect." },
			{ Mark(FrameMeter::Marker_ProjectileInvuln),
				FrameMeter::GetMarkerName(FrameMeter::Marker_ProjectileInvuln), "Projectiles whiff",
				"Projectiles cannot connect." },
			{ Mark(FrameMeter::Marker_HeadInvuln), FrameMeter::GetMarkerName(FrameMeter::Marker_HeadInvuln),
				"Head attacks whiff", "Attacks marked Head cannot connect." },
			{ Mark(FrameMeter::Marker_BodyInvuln), FrameMeter::GetMarkerName(FrameMeter::Marker_BodyInvuln),
				"Body hits whiff", "Hits at body height cannot connect." },
			{ Mark(FrameMeter::Marker_LegsInvuln), FrameMeter::GetMarkerName(FrameMeter::Marker_LegsInvuln),
				"Foot attacks whiff", "Attacks marked Foot cannot connect." },
			{ Mark(FrameMeter::Marker_DiveInvuln), FrameMeter::GetMarkerName(FrameMeter::Marker_DiveInvuln),
				"Air attacks whiff", "Attacks marked Air cannot connect." },
			{ Mark(FrameMeter::Marker_HighMidInvuln),
				FrameMeter::GetMarkerName(FrameMeter::Marker_HighMidInvuln), "High and mid hits whiff",
				"Partial invincibility the frame data declares: high and mid hits cannot connect." },
			{ Mark(FrameMeter::Marker_LowMidInvuln),
				FrameMeter::GetMarkerName(FrameMeter::Marker_LowMidInvuln), "Low and mid hits whiff",
				"Partial invincibility the frame data declares: low and mid hits cannot connect." },
			{ kGuideNoSwatch, "Two or three at once", "Split the cell",
				"Up to three categories share one cell, split evenly." } },
		numbers{
			{ kGuideNoSwatch, "Startup", "Frames until the move can hit",
				"Frames until the move can hit, the first active frame included." },
			{ kGuideNoSwatch, "Total", "Start of the move to its end",
				"From the move starting to it ending." },
			{ kGuideNoSwatch, "Advantage", "Who acts first",
				"Who acts first after the exchange. Positive means you." },
			{ kGuideNoSwatch, "Advantage in brackets", "Before the opponent teched",
				"The advantage before the opponent teched. It can be off." },
			{ kGuideNoSwatch, "Blockstun", "Stun of the last hit blocked",
				"The stun of the last hit blocked." },
			{ kGuideNoSwatch, "Hitstun", "Stun of the last hit taken", "The stun of the last hit taken." },
			{ kGuideNoSwatch, "Gap", "Free frames before the last hit",
				"Free frames right before the last hit. None means it was airtight." },
			{ kGuideNoSwatch, "Flash", "Super flash length",
				"The super flash inside the move shown. It counts up while the flash is on screen and is "
				"never drawn on the bar." },
			{ kGuideNoSwatch, "Damage", "Damage of the last combo",
				"Hit damage of the last combo. Starts over with the next combo." },
			{ kGuideNoSwatch, "Burn / Poison", "Wagner's burn, Uzuki's poison",
				"Everything Wagner's burn or Uzuki's poison did, in a combo, on block or in neutral. "
				"Starts over when a new one is applied." },
			{ kGuideNoSwatch, "Chip", "Damage from blocked hits",
				"Damage from blocked hits. Starts over when either side gets hit." },
			{ kGuideNoSwatch, "Self", "HP Carmine spent",
				"HP Carmine spent on his own moves. Starts over after 3 seconds without a change." },
			{ kGuideNoSwatch, "Heal", "HP Carmine got back",
				"HP Carmine got back. Training mode's refill is not counted. Starts over after 3 seconds "
				"without a change." } },
		pages{
			Page("Overview", "How to read the meter", overview),
			Page("Bar", "What the character is doing", bar),
			Page("Attack Row", "The attack's properties", attack),
			Page("Invincibility Row", "What cannot hit them", invuln),
			Page("Numbers", "Above and below the bars", numbers) },
		content{ "FRAME METER", pages, static_cast<int>(sizeof(pages) / sizeof(pages[0])) }
	{
	}
};

}

const GuideContent& FrameMeterLegend::Get()
{
	static const Legend legend;
	return legend.content;
}
