#include "Core/ThreadRole.h"

#include "Core/logger.h"

namespace {

thread_local ThreadRole::Role t_role = ThreadRole::Role_Unknown;

const char* const kNames[ThreadRole::Role_COUNT] = { "an unmarked thread", "the game thread", "Present",
	"the game's loader thread", "a mod worker" };

}

void ThreadRole::Mark(Role role)
{
	t_role = role;
}

ThreadRole::Role ThreadRole::Current()
{
	return t_role;
}

const char* ThreadRole::Name(Role role)
{
	return role >= 0 && role < Role_COUNT ? kNames[role] : kNames[Role_Unknown];
}

void ThreadRole::Expect(Role role, Site& site)
{
	if (t_role == role || t_role == Role_Unknown || site.reported)
		return;

	site.reported = true;
	LOG("ThreadRole: %s is meant for %s and was reached from %s", site.where, Name(role), Name(t_role));
}
