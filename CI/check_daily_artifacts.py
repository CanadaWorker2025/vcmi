"""Source guards and optional installed-mod checks; not runtime game verification."""
import argparse
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROCESSOR = (ROOT / 'server/processors/NewTurnProcessor.cpp').read_text(encoding='utf-8')
HANDLER = (ROOT / 'server/CGameHandler.cpp').read_text(encoding='utf-8')
DAILY = PROCESSOR.split('void NewTurnProcessor::grantDailyArtifacts()', 1)[1].split(
    'void NewTurnProcessor::handleTimeEvents', 1)[0]
GAME = None
TOKENS = re.compile(r'"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/')


def load(path):
    content = path.read_text(encoding='utf-8-sig')
    content = TOKENS.sub(lambda m: '' if m[0].startswith('/') else m[0], content)
    return json.loads(re.sub(r',\s*([}\]])', r'\1', content))


class DailyArtifactChecks(unittest.TestCase):
    def test_single_global_daily_hook(self):
        self.assertEqual(HANDLER.count('newTurnProcessor->grantDailyArtifacts();'), 1)
        daily = HANDLER.split('void CGameHandler::onNewTurn()', 1)[1].split('void CGameHandler::start(', 1)[0]
        self.assertLess(daily.index('elem->newTurn('), daily.index('grantDailyArtifacts()'))
        start = HANDLER.split('void CGameHandler::start(bool resume)', 1)[1]
        self.assertIn('if (!resume)', start)
        self.assertIn('onNewTurn();', start.split('else', 1)[0])

    def test_mod_bonus_without_changing_builtin_ids(self):
        enum = (ROOT / 'lib/bonuses/BonusEnum.h').read_text(encoding='utf-8')
        self.assertNotIn('DAILY_RANDOM_ARTIFACT', enum)
        self.assertIn('getAllObjets()', DAILY)
        self.assertIn('bonusToString(type)', DAILY)

    def test_banned_and_combined_items_are_not_filtered(self):
        for prohibited in ('isAllowed(', 'legalArtifact(', 'getDefaultAllowed(', 'isCombined('):
            self.assertNotIn(prohibited, DAILY)
        self.assertIn('LIBRARY->arth->objects', DAILY)
        self.assertIn('EArtifactClass::ART_SPECIAL', DAILY)
        self.assertIn('getWarMachine().hasValue()', DAILY)

    def test_no_grail_fallback(self):
        self.assertLess(DAILY.index('if(pool.empty())'), DAILY.index('rollArtifact(pool)'))

    def test_backpack_not_autoequip(self):
        self.assertIn('isBackpackFreeSlots(hero)', DAILY)
        self.assertIn('getArtBackpackPosition(hero, artifact)', DAILY)
        self.assertIn('isSlotBackpack(position)', DAILY)
        self.assertNotIn('FIRST_AVAILABLE', DAILY)
        self.assertIn('giveHeroNewArtifact(hero, artifact, position)', DAILY)

    def test_scrolls_have_a_real_spell(self):
        self.assertIn('!scrollSpells.empty()', DAILY)
        self.assertIn('!spell->isSpecial()', DAILY)
        self.assertIn('!spell->isCreatureAbility()', DAILY)
        self.assertIn('giveHeroNewScroll(hero,', DAILY)

    def test_snapshot_and_stable_order(self):
        self.assertLess(DAILY.index('recipients.emplace_back'), DAILY.index('rollArtifact(pool)'))
        self.assertIn('std::sort(recipients.begin()', DAILY)
        self.assertIn('hero->ID != Obj::HERO', DAILY)
        self.assertIn('EPlayerStatus::INGAME', DAILY)

    def test_uses_existing_sync_not_direct_mutation(self):
        for prohibited in ('createArtifact(', 'artifactsInBackpack.push', 'addNewObject(', 'NewObject '):
            self.assertNotIn(prohibited, DAILY)
        grant = HANDLER.split('const CGHeroInstance * h, const CArtifact * artType,', 1)[1].split(
            'bool CGameHandler::giveHeroNewArtifact', 1)[0]
        self.assertIn('NewArtifact na;', grant)
        self.assertIn('sendAndApply(na)', grant)

    def test_viola_registration_and_original_specialty(self):
        if GAME is None:
            self.skipTest('Use --game-dir for installed-mod configuration checks')
        mod = GAME / 'Mods/extra creatures specialty'
        manifest = load(mod / 'mod.json')
        self.assertIn('config/bonuses/dailyArtifacts.json', manifest['bonuses'])
        definition = load(mod / 'content/config/bonuses/dailyArtifacts.json')
        self.assertIn('DAILY_RANDOM_ARTIFACT', definition)
        hero = load(mod / 'content/config/heroes/spellHero.json')['asylum:k_viola']
        bonuses = hero['specialty']['bonuses']
        self.assertEqual(bonuses['hypnotize']['val'], 10)
        self.assertEqual(bonuses['dailyRandomArtifact']['val'], 1)
        self.assertEqual(bonuses['dailyRandomArtifact']['type'], 'extra creatures specialty:DAILY_RANDOM_ARTIFACT')
        self.assertIn('背包', hero['texts']['specialty']['description'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-dir', type=Path)
    args, remainder = parser.parse_known_args()
    GAME = args.game_dir
    unittest.main(argv=[__file__] + remainder)
