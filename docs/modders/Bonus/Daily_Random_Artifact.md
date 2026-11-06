# Daily Random Artifacts

This server extension grants random artifacts directly into hero backpacks at
the end of global new-day processing, starting on day 2. It creates no map objects
and uses the existing `NewArtifact` synchronization and artifact initialization.

## Registration

Use VCMI's existing mod-defined bonus registration, not a new built-in enum value.
Add a bonus definition file to your mod manifest's `bonuses` array:

```json
{
  "DAILY_RANDOM_ARTIFACT": {
    "hidden": true,
    "blockDescriptionPropagation": true,
    "description": "Grants ${val} random artifacts into the hero backpack each day."
  }
}
```

Add a specialty bonus referencing that type, qualified by its defining mod ID:

```json
{
  "dailyRandomArtifact": {
    "type": "extra creatures specialty:DAILY_RANDOM_ARTIFACT",
    "val": 1,
    "valueType": "BASE_NUMBER"
  }
}
```

All registered types named `DAILY_RANDOM_ARTIFACT` use this contract. Normal bonus
aggregation applies within each type; positive counts from separate registered
types are summed, capped at 100 grants per hero/day as a safety bound. No subtype,
radius or `addInfo` is used. Existing built-in bonus numbers and packet formats
are unchanged. An unmodified 1.7.4 engine can load the mod-defined bonus but has no
server handler for it, so it does not grant artifacts.

## Selection And Timing

- Ignore the map's artifact ban list intentionally, per the integration's rules.
- Include normal combination artifacts. Do not use `legalArtifact()` or
  `getDefaultAllowed()`, which would exclude them.
- Exclude artifacts marked `ART_SPECIAL`, except spell scrolls, and all war
  machines. SPECIAL is the engine's classification, not a reliable detector of
  narrative intent: a modded quest item mislabeled as normal remains eligible.
- Scrolls receive an initialized, nonspecial, non-creature hero spell of level
  1-5; spell bans are also ignored. Without any such spell, scrolls are excluded
  rather than creating an invalid scroll with no spell.
- The existing artifact randomizer balances prior allocation counts. It is not
  independent uniform sampling, and consumes the normal serialized game RNG.
- Empty pools and full backpacks skip the reward without a Grail fallback or a
  random draw. Grants are never equipped automatically and never replace items.
- Snapshot hero entitlements before any grants. Newly received bonus artifacts
  cannot recursively increase the current day's grant count.
- Sort by hero object ID for stable server-side processing. Only owned heroes of
  in-game players participate; imprisoned and tavern-pool heroes do not.
  Garrisoned, visiting and embarked owned heroes remain eligible.
- No grant on initial day, recruitment, movement, ownership changes or loading.
  Reloading a completed day does not re-run the new-day hook. There is no deferred
  debt: a skipped grant does not accumulate for a later day.

## Existing Saves And Deployment

Hero initialization copies specialty bonuses to the hero instance. Adding this
configuration does not migrate already initialized heroes in an existing save.
Use a newly initialized map/hero, or perform a separately reviewed save migration;
do not assume merely reloading an old save adds the ability. Do not invoke
`initHero()` on a loaded hero: it resets unrelated state.

The bonus is registered by the integration mod, so adding or removing its data
still changes the mod set/configuration. Do not promise that modified saves can
be opened safely with a different mod set or an older engine. All peers must use
matching game binaries and mod data.

Build the modified engine and deploy a matching binary set. Configuration alone
does not activate this feature in the currently installed 4dd431c build. Keep the
Viola `hypnotize` bonus and append this new bonus rather than replacing it.

## Verification

Run `python CI/check_daily_artifacts.py`; optionally pass `--game-dir` pointing
to the integration to validate its manifest, bonus definition and Viola patch.
The dedicated MSVC x64 workflow also compiles and runs
`CI/test_daily_artifact_policy.cpp` for the actual capacity/count/filter policy.
Source guards are not runtime tests.

Before deployment, verify a fresh Viola map across days 1-4, garrison/boat states,
full backpacks, empty candidate pools, banned normal artifacts, combinations,
scrolls, save/reload, all players and multiple recipients. Confirm only backpack
slots change, with matching artifact instance IDs on server and clients.
