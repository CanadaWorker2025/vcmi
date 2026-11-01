# Detailed Object Previews

In Adventure Map options, enable **Detailed previews**. This is enabled by default
and applies only to visible objects in games with exactly one human player.
Other objects and multiplayer games retain their existing popups.

Right-click a town to inspect generated Mage Guild spells by level. Only faction
supported guild levels and the current library capacity are included. Unbuilt
guild levels are marked separately.

Universities show their generated skills. Reward objects show their initialized
reward options, requirements, selection mode, guards and reset period. Seer huts
also show quest requirements, deadline and completion state. Taken items are
separate from granted items. The preview does not visit the object, accept a
quest, grant a reward or generate new random values.

Configured rewards are not a guarantee that a particular hero can still claim
them. Some advanced mod effects/conditions have a notice rather than a complete
description; the visit dialog remains authoritative. Future town spell previews
use the library capacity at the time of inspection.

The configuration setting is `adventure.detailedObjectPreview` in the user's
`config/settings.json`. Setting it to `false` restores the original popups.

Deploy the rebuilt client together with these matching resources:

- `config/schemas/settings.json`
- `config/widgets/settings/adventureOptionsTab.json`
- `Mods/vcmi/Content/config/translations/english.json`
- `Mods/vcmi/Content/config/translations/chinese.json`

Merge resource changes with the integration's custom configuration rather than
overwriting the entire game directory. No persistent save fields were added.
