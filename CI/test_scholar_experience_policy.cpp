#include "../server/processors/ScholarExperiencePolicy.h"

#include <limits>
#include <stdexcept>

using ScholarExperiencePolicy::Hero;
using ScholarExperiencePolicy::plan;

void require(bool condition)
{
	if(!condition)
		throw std::runtime_error("Scholar experience policy regression");
}

int main()
{
	constexpr int64_t cap = std::numeric_limits<int64_t>::max();
	for(int mastery = 1; mastery <= 3; ++mastery)
	{
		const int numerator = mastery == 1 ? 1 : mastery == 2 ? 2 : 4;
		const auto transfer = plan({20, mastery, 100000}, {10, 0, 10000}, cap);
		require(transfer && transfer->teacher == 0 && transfer->student == 1);
		require(transfer->amount == 100000 / 5 * numerator - 10000);
		require(!plan({20, mastery, 100000}, {10, 0, 10000 + transfer->amount}, cap));
		const auto reversed = plan({10, 0, 10000}, {20, mastery, 100000}, cap);
		require(reversed && reversed->teacher == 1 && reversed->student == 0 && reversed->amount == transfer->amount);
		const auto huge = plan({100, mastery, cap}, {1, 0, 0}, cap);
		require(huge && huge->amount == cap / 5 * numerator + cap % 5 * numerator / 5);
		for(int64_t experience = 1; experience < 1000; ++experience)
		{
			const int64_t expected = experience * numerator / 5;
			const auto small = plan({20, mastery, experience}, {1, 0, 0}, cap);
			require(expected == 0 ? !small : small && small->amount == expected);
		}
	}
	require(!plan({20, 0, 100000}, {1, 3, 1000}, cap)); // Student cannot teach upward.
	require(!plan({20, 3, 100000}, {20, 0, 1000}, cap));
	require(!plan({20, 4, 100000}, {1, 0, 0}, cap));
	require(!plan({20, -1, 100000}, {1, 0, 0}, cap));
	require(!plan({20, 3, -1}, {1, 0, 0}, cap));
	require(!plan({20, 3, 100000}, {1, 0, -1}, cap));
	require(!plan({20, 3, 100000}, {1, 0, 0}, 0));
	require(!plan({20, 3, 100000}, {1, 0, 0}, -1));
	require(!plan({20, 1, 100000}, {1, 0, 90000}, cap)); // Never subtract experience.
	const auto limited = plan({20, 3, 100000}, {5, 0, 10000}, 15000);
	require(limited && limited->amount == 5000);
	require(!plan({20, 3, 100000}, {5, 0, 15000}, 15000)); // No cap-message spam.
	require(!plan({20, 3, 100000}, {5, 0, 17000}, 15000));
	const auto later = plan({21, 3, 200000}, {10, 0, 80000}, cap);
	require(later && later->amount == 80000); // Teacher progresses: top up again.
}
