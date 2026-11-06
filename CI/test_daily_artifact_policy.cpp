#include "../server/processors/DailyArtifactPolicy.h"

#include <climits>
#include <initializer_list>
#include <stdexcept>

int main()
{
	for(const bool special : {false, true})
		for(const bool machine : {false, true})
			for(const bool scroll : {false, true})
				for(const bool spells : {false, true})
				{
					const bool expected = !machine && ((scroll && spells) || (!scroll && !special));
					if(DailyArtifactPolicy::eligible(special, machine, scroll, spells) != expected)
						throw std::runtime_error("Wrong special/war-machine filter or uninitialized scroll");
				}
	for(const int day : {0, 1})
	{
		int calls = 0;
		if(DailyArtifactPolicy::grant(day, 1, [&]() { ++calls; return true; }, [&]() { ++calls; return true; }) || calls)
			throw std::runtime_error("Initialization day must not check capacity or consume RNG");
	}
	for(const int requested : {INT_MIN, -1, 0, 1, 2, 100, INT_MAX})
		for(const int capacity : {0, 1, 3, 1000})
		{
			int space = capacity;
			int draws = 0;
			const int granted = DailyArtifactPolicy::grant(2, requested,
				[&]() { return space > 0; }, [&]() { ++draws; --space; return true; });
			const int expected = std::min(std::clamp(requested, 0, DailyArtifactPolicy::maxPerHero), capacity);
			if(granted != expected || draws != expected || space != capacity - expected)
				throw std::runtime_error("Incorrect count, capacity handling, safety cap, or RNG consumption");
		}
	int attempts = 0;
	if(DailyArtifactPolicy::grant(2, 100, []() { return true; }, [&]() { ++attempts; return false; }) != 0 || attempts != 1)
		throw std::runtime_error("Failed grants must stop without retry loops");
	int awarded = 0;
	for(int day = 1; day <= 4; ++day)
		DailyArtifactPolicy::grant(day, 1, []() { return true; }, [&]() { ++awarded; return true; });
	if(awarded != 3)
		throw std::runtime_error("One artifact per new day from day two expected");
}
