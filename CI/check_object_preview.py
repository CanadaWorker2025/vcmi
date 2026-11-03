"""Source/resource regression guards; not a substitute for client compilation or UI tests."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "client/windows/InfoWindows.cpp").read_text(encoding="utf-8")
PREVIEW = (ROOT / "client/windows/ObjectPreview.cpp").read_text(encoding="utf-8")


class ObjectPreviewRegression(unittest.TestCase):
    def test_translations(self):
        catalogs = []
        for language in ("english", "chinese"):
            pairs = []
            json.loads((ROOT / f"Mods/vcmi/Content/config/translations/{language}.json").read_text(
                encoding="utf-8"), object_pairs_hook=lambda values: pairs.extend(values) or dict(values))
            keys = [key for key, value in pairs]
            self.assertEqual(len(keys), len(set(keys)), "Duplicate translation key")
            catalog = dict(pairs)
            catalogs.append({k for k in catalog if k.startswith("vcmi.objectPreview.")})
            referenced = set(re.findall(r'previewLabel\("([^"]+)"\)', PREVIEW))
            referenced.update(re.findall(r'appendPreview(?:Value|Bonuses)\(text, "([^"]+)"', PREVIEW))
            referenced.update(re.findall(r'std::make_pair\("([^"]+)"', PREVIEW))
            for key in referenced:
                self.assertIn("vcmi.objectPreview." + key, catalog)
        self.assertEqual(*catalogs)

    def test_native_content_is_not_replaced(self):
        dispatch = SOURCE.split("void CRClickPopup::createAndPush(const CGObjectInstance *", 1)[1].split(
            "CRClickPopupInt::CRClickPopupInt", 1)[0]
        self.assertLess(dispatch.index("createCustomInfoWindow"), dispatch.index("makeObjectPreview"))
        for call in ("getPopupText(hero)", "getPopupText(GAME->interface()->playerID)",
                     "getPopupComponents", "CInfoWindow", "CRClickPopupInt>(native)", "pushWindow(native)"):
            self.assertIn(call, dispatch)
        composite = SOURCE.split("MapObjectDetailsPopup::MapObjectDetailsPopup", 1)[1].split(
            "CInfoBoxPopup::CInfoBoxPopup", 1)[0]
        self.assertIn("addChild(nativePopup.get())", composite)
        self.assertIn("pos = Rect(0, 0,", composite)

    def test_all_reward_events_are_labeled(self):
        for event in ("FIRST_VISIT", "ALREADY_VISITED", "NOT_AVAILABLE", "GUARDED"):
            self.assertIn(f"case Rewardable::EEventType::EVENT_{event}:", PREVIEW)
        self.assertNotIn('previewLabel("noReward")', PREVIEW)
        self.assertIn("displayedReward.loadComponents(components, hero)", PREVIEW)

    def test_preview_does_not_execute_visit_or_random_generation(self):
        preview = PREVIEW
        for token in ("onHeroVisit(", "giveReward(", "sendAndApply(", "getRNG(", "nextInt("):
            self.assertNotIn(token, preview)
        self.assertIn("isVisible(object)", preview)
        self.assertIn("isControlledByHuman()", preview)

    def test_extended_fields_are_not_silently_dropped(self):
        for field in ("availableSlots", "scrolls", "canLearnSpells", "canReceiveCreatures",
                      "heroClasses", "canLearnSkills", "commanderAlive", "hasExtraCreatures"):
            self.assertIn("limiter." + field, PREVIEW)
        for field in ("takenArtifacts", "takenArtifactSlots", "grantedScrolls", "takenScrolls",
                      "takenCreatures", "creaturesChange", "heroBonuses", "playerBonuses",
                      "commanderBonuses", "spellCast", "removeObject", "revealTiles",
                      "manaOverflowFactor", "moveOverflowFactor", "extraComponents"):
            self.assertIn("info.reward." + field, PREVIEW)

    def test_composite_owns_close_and_resize(self):
        composite = SOURCE.split("MapObjectDetailsPopup::MapObjectDetailsPopup", 1)[1].split(
            "CInfoBoxPopup::CInfoBoxPopup", 1)[0]
        self.assertIn("nativePopup->removeUsedEvents(DRAG_POPUP)", composite)
        self.assertIn("MapObjectDetailsPopup::onScreenResize()", composite)
        self.assertIn("rebuildLayout(ENGINE->screenDimensions() / 2)", composite)
        self.assertNotIn("assert(layout)", composite)
        self.assertIn('previewLabel("unknownVisitState")', PREVIEW)

    def test_preview_module_is_registered(self):
        cmake = (ROOT / "client/CMakeLists.txt").read_text(encoding="utf-8")
        for path in ("windows/ObjectPreview.cpp", "windows/ObjectPreview.h", "windows/ObjectPreviewLayout.h"):
            self.assertIn(path, cmake)


if __name__ == "__main__":
    unittest.main()
