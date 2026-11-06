#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace ScholarExperiencePolicy
{
struct Hero
{
	uint32_t level;
	int scholar;
	int64_t experience;
};

struct Transfer
{
	int teacher;
	int student;
	int64_t amount;
};

inline int percentage(int mastery)
{
	switch(mastery)
	{
		case 1: return 20;
		case 2: return 40;
		case 3: return 80;
		default: return 0;
	}
}

inline std::optional<Transfer> plan(const Hero & first, const Hero & second, int64_t experienceCap)
{
	if(first.level == second.level || first.experience < 0 || second.experience < 0 || experienceCap <= 0)
		return {};
	const bool firstTeaches = first.level > second.level;
	const auto & teacher = firstTeaches ? first : second;
	const auto & student = firstTeaches ? second : first;
	const int rate = percentage(teacher.scholar);
	if(rate == 0)
		return {};
	// Split quotient/remainder before multiplication to avoid signed 64-bit overflow.
	const int64_t target = std::min(experienceCap,
		teacher.experience / 100 * rate + teacher.experience % 100 * rate / 100);
	if(target <= student.experience)
		return {};
	return Transfer{firstTeaches ? 0 : 1, firstTeaches ? 1 : 0, target - student.experience};
}
}
