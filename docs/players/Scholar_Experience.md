# Scholar Experience Instruction

This server extension preserves native Scholar spell exchange and adds experience
instruction between heroes belonging to the same player. It needs a rebuilt
engine; skill descriptions alone do not enable it.

## Rules

The hero with the strictly higher actual level is the teacher. Only that hero's
real Scholar skill level determines the rate: Basic 20%, Advanced 40%, Expert 80%.
Equal-level heroes do not teach, even if their total experience differs. The
student needs neither Scholar, Wisdom nor a spellbook. The teacher also does not
need a spellbook for experience instruction; native spell exchange is unchanged.

```text
target = min(map/engine experience cap, floor(teacher total experience * rate))
grant  = max(0, target - student current total experience)
```

The teacher loses nothing. No further Learning, Scholar XP aura or other XP
multiplier is applied to this grant. No new commander experience is awarded;
ordinary experience sources retain their existing commander behavior.
Repeated meetings at the same target award nothing and show no gain message.
Progress by the teacher or a higher Scholar mastery may allow another top-up.
A student above the target/cap never loses experience. The calculation uses
overflow-safe signed 64-bit integer arithmetic, including downward rounding.

Example: teacher has 100,000 XP, student has 10,000 XP. Basic grants 10,000,
Advanced 30,000, Expert 70,000, subject to the map level limit.

## Timing And Query Safety

Hero exchange places a single-owner `CScholarExperienceQuery` below the original
exchange query. It evaluates after the exchange has closed. Entering a town with
both garrison/visiting heroes places the same barrier below the town's reward
queries, so existing rewards and level-up choices finish first. Swapping the two
heroes in a town also checks instruction after the swap.

Only the dedicated barrier invokes instruction, once, after its own removal.
It stores object IDs, not extra raw hero pointers, and revalidates current owners
and hero existence before awarding. Its exposure/removal preserves the existing
object notification route when no new level-up query intervenes. Ordinary
garrison/NPC dialogs are not changed. Ally/enemy/different-player heroes never
receive this extension's XP. Native allied spell exchange retains its behavior.

Positive grants use the existing experience packs and `expGiven()` upgrade chain
through the normal gain helper. The existing two-argument `giveExperience()`
entry point retains its old commander behavior; instruction alone selects the
new hero-only overload. No skill choice is made on the player's behalf.

## Saves And Resources

No persistent teacher/student records, new bonus IDs or new network packs are
introduced. Existing heroes with Scholar can use the new logic after loading
with the rebuilt engine and meeting; unlike a newly added hero specialty, no
hero reinitialization or save migration is needed. Loading by itself does not
grant XP. Already-granted XP naturally prevents repeat awards after save/load.
Use consistent engine builds on all peers and back up saves before testing.

Merge the matching skill descriptions and the `vcmi.scholarExperience.learned`
message into the installed resources. The integration's Tides of War Scholar
descriptions retain spell exchange, Runic Magic prerequisite and the expert XP
aura while adding the 20/40/80% rule.

## Verification

- `python CI/check_scholar_experience.py` checks source/resource invariants.
  Pass `--game-dir` to include the integration's descriptions and existing aura.
- The dedicated x64 workflow compiles/runs `CI/test_scholar_experience_policy.cpp`
  for all mastery rates, both encounter directions, equal levels, absent Scholar,
  invalid values, rounding, repeated training, caps and 64-bit overflow boundaries.
- Neither check is an in-game verification of the query stack. Test real map
  exchange with consecutive upgrades, town entry rewards and both town hero
  positions; spellbook/no-spellbook cases; allied heroes; commander and XP aura
  behavior; repeated meetings; map caps; save/reload and automated heroes.

Implementation status: source/configuration and static checks complete. Full
engine compilation, runtime query-stack verification and deployment are pending.
