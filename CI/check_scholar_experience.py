"""Source/resource guards only; compile and exercise the query stack in-game too."""
import argparse
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
HANDLER = (ROOT / 'server/CGameHandler.cpp').read_text(encoding='utf-8')
QUERIES = (ROOT / 'server/queries/MapQueries.cpp').read_text(encoding='utf-8')
GAME = None


def load(path):
    content = path.read_text(encoding='utf-8-sig')
    tokens = r'"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/'
    content = re.sub(tokens, lambda m: '' if m[0].startswith('/') else m[0], content)
    return json.loads(re.sub(r',\s*([}\]])', r'\1', content))


def body(name, next_name):
    return HANDLER.split(name, 1)[1].split(next_name, 1)[0]


class ScholarExperienceChecks(unittest.TestCase):
    def test_native_scholar_preserved(self):
        original = body('void CGameHandler::useScholarSkill(', 'std::shared_ptr<CScholarExperienceQuery>')
        self.assertIn('LEARN_MEETING_SPELL_LIMIT', original)
        self.assertIn('hasSpellbook()', original)
        self.assertNotIn('useScholarExperience(', original)

    def test_same_owner_and_live_lookup(self):
        experience = body('void CGameHandler::useScholarExperience(', 'void CGameHandler::heroExchange(')
        self.assertIn('gameInfo().getHero(hero1)', experience)
        self.assertIn('hero1 == hero2', experience)
        self.assertIn('first->getOwner() != second->getOwner()', experience)
        self.assertIn('EPlayerStatus::INGAME', experience)
        self.assertNotIn('hasSpellbook', experience)
        self.assertNotIn('maxSpellLevel', experience)

    def test_existing_gain_path_and_caps(self):
        experience = body('void CGameHandler::useScholarExperience(', 'void CGameHandler::heroExchange(')
        self.assertIn('std::min(cap,', experience)
        self.assertIn('levelLimit', experience)
        self.assertIn('giveExperience(student, transfer->amount, false)', experience)
        self.assertNotIn('calculateXp(', experience)
        self.assertNotIn('giveExperience(teacher', experience)
        legacy = body('void CGameHandler::giveExperience(const CGHeroInstance * hero, TExpType amountToGain)',
                      'void CGameHandler::giveExperience(const CGHeroInstance * hero, TExpType amountToGain, bool')
        self.assertIn('giveExperience(hero, amountToGain, true)', legacy)
        self.assertIn('if (includeCommander &&', HANDLER)

    def test_exchange_barrier_precedes_dialog(self):
        exchange = body('void CGameHandler::heroExchange(', 'void CGameHandler::sendAndApply(')
        self.assertLess(exchange.index('queueScholarExperience('), exchange.index('ExchangeDialog hex'))
        self.assertIn('queries->addQuery(exchange)', exchange)
        self.assertNotIn('giveExperience(', exchange)

    def test_town_rewards_and_swap_are_covered(self):
        visit = body('void CGameHandler::heroVisitCastle(', 'void CGameHandler::visitCastleObjects(')
        self.assertLess(visit.index('queueScholarExperience('), visit.index('visitCastleObjects('))
        self.assertLess(visit.index('visitCastleObjects('), visit.index('popIfTop(scholar)'))
        swap = body('bool CGameHandler::garrisonSwap(', 'bool CGameHandler::moveArtifact(')
        self.assertIn('queueScholarExperience(intown.visiting, intown.garrison)', swap)

    def test_barrier_single_owner_and_once_only(self):
        query = QUERIES.split('CScholarExperienceQuery::CScholarExperienceQuery', 1)[1].split(
            'CGarrisonDialogQuery::CGarrisonDialogQuery', 1)[0]
        self.assertEqual(query.count('addPlayer(player)'), 1)
        self.assertIn('return true;', query)
        self.assertIn('if(completed)', query)
        self.assertLess(query.index('completed = true'), query.index('gh->useScholarExperience('))
        self.assertNotIn('giveExperience(', query)

    def test_original_owner_and_visit_callbacks(self):
        query = QUERIES.split('CScholarExperienceQuery::CScholarExperienceQuery', 1)[1].split(
            'CGarrisonDialogQuery::CGarrisonDialogQuery', 1)[0]
        self.assertIn('first->getOwner() != color', query)
        self.assertIn('CQuery::onExposure(topQuery)', query)
        self.assertIn('completedChild->notifyObjectAboutRemoval(visitedObject, visitingHero)', query)
        garrison = QUERIES.split('CGarrisonDialogQuery::CGarrisonDialogQuery', 1)[1].split(
            'CBlockingDialogQuery::', 1)[0]
        self.assertNotIn('useScholarExperience', garrison)

    def test_texts(self):
        for root in ([ROOT, GAME] if GAME else [ROOT]):
            for language in ('english', 'chinese'):
                data = load(root / f'Mods/vcmi/Content/config/translations/{language}.json')
                text = data['vcmi.scholarExperience.learned']
                self.assertEqual(text.count('%s'), 2)
                self.assertEqual(text.count('%d'), 1)

    def test_skill_description_and_existing_aura(self):
        core = load(ROOT / 'config/skills.json')['scholar']
        for mastery, percent in (('basic', 20), ('advanced', 40), ('expert', 80)):
            self.assertIn(f'{percent}%', core[mastery]['description'])
        if GAME is None:
            self.skipTest('Use --game-dir to check integration descriptions and aura')
        override = load(GAME / 'Mods/tidesOfWar/content/config/skills/scholar.json')['core:scholar']
        for mastery, percent in (('basic', 20), ('advanced', 40), ('expert', 80)):
            self.assertIn(f'{percent}%', override[mastery]['description'])
            self.assertIn('符文学', override[mastery]['description'])
        self.assertEqual(override['expert']['effects']['xpAura']['val'], 100)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-dir', type=Path)
    args, remainder = parser.parse_known_args()
    GAME = args.game_dir
    unittest.main(argv=[__file__] + remainder)
