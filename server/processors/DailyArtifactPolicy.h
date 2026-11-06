#pragma once

#include <algorithm>

namespace DailyArtifactPolicy
{
inline constexpr const char * bonusName = "DAILY_RANDOM_ARTIFACT";
inline constexpr int maxPerHero = 100;

inline bool eligible(bool special, bool warMachine, bool scroll, bool spellsAvailable)
{
	return !warMachine && (scroll ? spellsAvailable : !special);
}

// Capacity is checked before drawing an artifact, so a full backpack consumes no RNG.
template<typename HasSpace, typename GrantOne>
int grant(int day, int requested, HasSpace hasSpace, GrantOne grantOne)
{
	if(day <= 1)
		return 0;
	const int count = std::clamp(requested, 0, maxPerHero);
	int granted = 0;
	while(granted < count && hasSpace())
	{
		if(!grantOne())
			break;
		++granted;
	}
	return granted;
}
}
