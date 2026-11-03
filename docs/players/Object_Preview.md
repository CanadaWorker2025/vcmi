# Detailed Object Previews

In Adventure Map options, enable **Detailed previews**. This is enabled by default
and applies only to visible objects in games with exactly one human player.
Other objects and multiplayer games retain their existing popups.

The native popup is retained in full, including its original text, component
icons and specialized town/army panels. Supplemental details are placed beside
it, or below it when horizontal space is insufficient. If neither layout fits,
only the original popup is shown; native content is never clipped to fit details.

Right-click a town to inspect generated Mage Guild spells by level. Only faction
supported guild levels and the current library capacity are included. Unbuilt
guild levels are marked separately.

Universities show their generated skills. Reward objects show their initialized
reward options, requirements, selection mode, guards and reset period. Seer huts
also show quest requirements, deadline and completion state. Taken items are
separate from granted items. The preview does not visit the object, accept a
quest, grant a reward or generate new random values.

Configured event options are not a guarantee that a particular hero can still
claim them. All event types are labeled: first visit, already visited, unavailable
and guarded. Reward values use the selected hero where supported, or explicitly
identified base values without a selected hero. Conditions and quest/visit state
still apply; listing an option does not select it or assert eligibility.

Scroll grants/removals, equipment-slot removals, creature transformations,
movement/mana rules, cast spells, reveal rules and object removal are explicit.
Hero/player/commander bonuses expose their serializable configuration instead of
being silently omitted. Extra display components are labeled as display-only.
An empty event list is unknown/no exposed entries, not proof of no reward.
Future town spell previews use the library capacity at the time of inspection.

The configuration setting is `adventure.detailedObjectPreview` in the user's
`config/settings.json`. Setting it to `false` restores the original popups.

Deploy the rebuilt client together with these matching resources:

- `config/schemas/settings.json`
- `config/widgets/settings/adventureOptionsTab.json`
- `Mods/vcmi/Content/config/translations/english.json`
- `Mods/vcmi/Content/config/translations/chinese.json`

Merge resource changes with the integration's custom configuration rather than
overwriting the entire game directory. No persistent save fields were added.

## Regression Checklist

Run `python CI/check_object_preview.py` for source/resource guards. The dedicated
MSVC x64 workflow also compiles and runs `CI/test_object_preview_layout.cpp`.
These checks do not replace building the client or in-game verification.

Before deployment, compare original and extended popups for towns, universities,
seer huts, scholars, primary-stat objects and treasure chests. Include unsupported
objects, visited/unvisited states, completed/unfinished quests, selected/no hero,
visible enemy towns, multiple human players and the disabled setting. Compare
all original text, icons and graphics, not just the new detail text. Exercise
random/all/first/player selection and every event type, negative skill changes,
scrolls versus spells, guards, special bonuses, resets and nested conditions.

Check at least 800x600 and 1920x1080, all screen edges, long-text scrolling,
right-button release and drag-to-close, and native-only fallback. Confirm that
repeated previews do not change resources, visit records, quests or RNG outcomes.
Record compilation, automated checks and in-game checks as separate results.

Repair status: source changes and regression guards added; this document does
not assert that the rebuilt client has passed in-game verification.

## Compatibility Audit

- Formatting is isolated in `client/windows/ObjectPreview.cpp`; native popup
  dispatch/composition remains in `InfoWindows.cpp`; geometry is handled by the
  independently testable `ObjectPreviewLayout.h`.
- The composite is the sole window-stack owner. Embedded native popups have
  drag-close events removed so they cannot attempt to pop a non-top window.
- Screen resize rebuilds the supplemental region, falling back to native-only
  when necessary; it does not close a window while the window stack is iterated.
- Hero-specific visit state is explicitly unknown without a selected hero.
- Minimum supplemental dimensions are enforced even on unusually small screens.
- The preview remains a configured-data inspection, not an eligibility simulator.
  Bonuses use technical serialized configuration, slot/class restrictions retain
  their IDs, and recursive limiter nesting beyond 16 levels is marked incomplete.
  These boundaries are intentional and must not be reported as fully interpreted
  natural-language descriptions of every mod effect.
- No bonus identifiers, shared state, save layout, server logic or gameplay rules
  are changed. Existing BattleAI fixes on the branch remain intact. Pre-existing
  user edits to the stat icon and its generator are outside this repair commit.

## Compatibility Audit

- Formatting is isolated in `client/windows/ObjectPreview.cpp`; native popup
  dispatch/composition remains in `InfoWindows.cpp`; geometry is handled by the
  independently testable `ObjectPreviewLayout.h`.
- The composite is the sole window-stack owner. Embedded native popups have
  drag-close events removed so they cannot attempt to pop a non-top window.
- Screen resize rebuilds the supplemental region, falling back to native-only
  when necessary; it does not close a window while the window stack is iterated.
- Hero-specific visit state is explicitly unknown without a selected hero.
- Minimum supplemental dimensions are enforced even on unusually small screens.
- The preview remains a configured-data inspection, not an eligibility simulator.
  Bonuses use technical serialized configuration, slot/class restrictions retain
  their IDs, and recursive limiter nesting beyond 16 levels is marked incomplete.
  These boundaries are intentional and must not be reported as fully interpreted
  natural-language descriptions of every mod effect.
- No bonus identifiers, shared state, save layout, server logic or gameplay rules
  are changed. Existing BattleAI fixes on the branch remain intact. Pre-existing
  user edits to the stat icon and its generator are outside this repair commit.
