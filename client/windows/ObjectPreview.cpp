#include "StdInc.h"
#include "ObjectPreview.h"

#include "../CPlayerInterface.h"
#include "../PlayerLocalState.h"
#include "../GameInstance.h"
#include "../widgets/CComponent.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/StartInfo.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/mapObjects/CGMarket.h"
#include "../../lib/mapObjects/army/CStackInstance.h"
#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/entities/faction/CTown.h"
#include "../../lib/entities/hero/CHero.h"

namespace ObjectPreview
{
namespace
{
std::string previewLabel(const std::string & key)
{
	return LIBRARY->generaltexth->translate("vcmi.objectPreview." + key);
}

void appendPreviewComponents(std::string & text, const std::vector<Component> & components)
{
	for(const auto & component : components)
	{
		if(component.type == ComponentType::NONE)
			continue;
		CComponent display(component, CComponent::tiny);
		auto line = display.getSubtitle();
		if(component.type == ComponentType::RESOURCE)
			line += " " + LIBRARY->generaltexth->restypes[component.subType.getNum()];
		else if(component.type == ComponentType::EXPERIENCE)
			line += " " + previewLabel("experience");
		else if(component.type == ComponentType::MORALE || component.type == ComponentType::LUCK)
			line = previewLabel(component.type == ComponentType::MORALE ? "morale" : "luck") + " " + std::to_string(component.value.value_or(0));
		else if(component.type == ComponentType::HERO_PORTRAIT)
			line = component.subType.as<HeroTypeID>().toEntity(LIBRARY)->getNameTranslated();
		boost::replace_all(line, "\n", " ");
		if(!line.empty())
			text += "  " + line + "\n";
	}
}

void appendPreviewValue(std::string & text, const std::string & key, int value)
{
	text += "  " + previewLabel(key) + ": " + std::to_string(value) + "\n";
}

// Expose serializable bonus configuration instead of silently omitting mod-specific effects.
void appendPreviewBonuses(std::string & text, const std::string & key, const std::vector<std::shared_ptr<Bonus>> & bonuses)
{
	if(bonuses.empty())
		return;
	text += previewLabel(key) + ":\n";
	for(const auto & bonus : bonuses)
		if(bonus)
		{
			auto data = bonus->toJsonNode().toString();
			boost::replace_all(data, "{", "(");
			boost::replace_all(data, "}", ")");
			text += data + "\n";
		}
}

void appendPreviewLimiter(std::string & text, const Rewardable::Limiter & limiter, int depth = 0)
{
	if(depth > 16)
	{
		text += previewLabel("additionalConditions") + "\n";
		return;
	}
	std::vector<Component> components;
	// Level is not experience: the standard limiter component helper conflates them.
	auto displayLimiter = limiter;
	displayLimiter.heroLevel = 0;
	displayLimiter.manaPoints = 0;
	displayLimiter.manaPercentage = 0;
	displayLimiter.loadComponents(components, nullptr);
	appendPreviewComponents(text, components);
	if(limiter.heroLevel)
		text += "  " + previewLabel("level") + " " + std::to_string(limiter.heroLevel) + "\n";
	if(limiter.daysPassed)
		text += "  " + previewLabel("daysPassed") + " " + std::to_string(limiter.daysPassed) + "\n";
	if(limiter.dayOfWeek)
		text += "  " + previewLabel("dayOfWeek") + " " + std::to_string(limiter.dayOfWeek) + "\n";
	for(const auto & field : {std::make_pair("manaPoints", limiter.manaPoints),
		std::make_pair("manaPercentage", limiter.manaPercentage), std::make_pair("movePoints", limiter.movePoints),
		std::make_pair("movePercentage", limiter.movePercentage)})
		if(field.second)
			appendPreviewValue(text, field.first, field.second);
	for(const auto & field : {std::make_pair("canLearnSkills", limiter.canLearnSkills),
		std::make_pair("commanderAlive", limiter.commanderAlive), std::make_pair("hasExtraCreatures", limiter.hasExtraCreatures)})
		if(field.second)
			text += "  " + previewLabel(field.first) + "\n";
	for(const auto & id : limiter.heroClasses)
		appendPreviewValue(text, "heroClassId", id.getNum());
	for(const auto & slot : limiter.availableSlots)
		appendPreviewValue(text, "availableSlot", slot.getNum());
	for(const auto & spell : limiter.scrolls)
	{
		text += previewLabel("requiredScroll") + ":\n";
		appendPreviewComponents(text, {Component(ComponentType::SPELL_SCROLL, spell)});
	}
	for(const auto & spell : limiter.canLearnSpells)
	{
		text += previewLabel("canLearnSpell") + ":\n";
		appendPreviewComponents(text, {Component(ComponentType::SPELL, spell)});
	}
	for(const auto & creature : limiter.canReceiveCreatures)
	{
		text += previewLabel("canReceiveCreatures") + ":\n";
		appendPreviewComponents(text, {Component(ComponentType::CREATURE, creature.getId(), creature.getCount())});
	}
	for(const auto & group : {std::make_pair("allOf", &limiter.allOf),
		std::make_pair("anyOf", &limiter.anyOf), std::make_pair("noneOf", &limiter.noneOf)})
	{
		if(group.second->empty())
			continue;
		text += "  " + previewLabel(group.first) + "\n";
		for(const auto & child : *group.second)
			if(child)
			{
				std::string nested;
				appendPreviewLimiter(nested, *child, depth + 1);
				boost::replace_all(nested, "\n", "\n    ");
				text += "    (\n    " + nested + "\n    )\n";
			}
			else
				text += previewLabel("additionalConditions") + "\n";
	}
}

}

bool makeObjectPreview(const CGObjectInstance * object, std::string & text)
{
	const auto & enabled = settings["adventure"]["detailedObjectPreview"];
	if((!enabled.isNull() && !enabled.Bool()) || !object || !GAME->interface()->cb->isVisible(object))
		return false;
	const auto * start = GAME->interface()->cb->getStartInfo();
	if(!start || std::count_if(start->playerInfos.begin(), start->playerInfos.end(),
		[](const auto & entry) { return entry.second.isControlledByHuman(); }) != 1)
		return false;
	text = "{" + previewLabel("supplement") + "}\n\n" + previewLabel("configuredOptions") + "\n\n";
	if(const auto * town = dynamic_cast<const CGTownInstance *>(object))
	{
		text += previewLabel("guildSpells") + "\n";
		for(size_t i = 0; i < town->spells.size() && i < town->getTown()->mageLevel && i < 5; ++i)
		{
			const auto count = std::min(town->spells[i].size(), static_cast<size_t>(std::max(0, town->spellsAtLevel(i + 1, false))));
			if(count == 0)
				continue;
			text += "\n{" + previewLabel("level") + " " + std::to_string(i + 1) + " - "
				+ previewLabel(town->mageGuildLevel() >= i + 1 ? "built" : "notBuilt") + "}\n";
			std::vector<Component> spells;
			for(size_t j = 0; j < count; ++j)
				spells.emplace_back(ComponentType::SPELL, town->spells[i][j]);
			appendPreviewComponents(text, spells);
		}
		return true;
	}
	if(const auto * university = dynamic_cast<const CGUniversity *>(object))
	{
		text += previewLabel("skills") + "\n";
		std::vector<Component> skills;
		for(const auto & skill : university->skills)
			skills.emplace_back(ComponentType::SEC_SKILL, skill.as<SecondarySkill>());
		appendPreviewComponents(text, skills);
		return true;
	}
	const auto * rewardable = dynamic_cast<const CRewardableObject *>(object);
	if(!rewardable)
		return false;
	const auto * hero = GAME->interface()->localState->getCurrentHero();
	text += hero ? previewLabel("heroContext") + " " + hero->getNameTranslated() + "\n" : previewLabel("noHeroContext") + "\n";
	if(rewardable->configuration.visitMode != Rewardable::VISIT_UNLIMITED)
	{
		const auto mode = rewardable->configuration.visitMode;
		const bool requiresHero = mode == Rewardable::VISIT_HERO || mode == Rewardable::VISIT_BONUS || mode == Rewardable::VISIT_LIMITER;
		if(!hero && requiresHero)
			text += previewLabel("unknownVisitState") + "\n";
		else
			text += previewLabel((hero ? rewardable->wasVisited(hero) : rewardable->wasVisited(GAME->interface()->playerID))
				? "visited" : "notVisited") + "\n";
	}
	if(const auto * seer = dynamic_cast<const CGSeerHut *>(object))
	{
		const auto & quest = seer->getQuest();
		text += "\n{" + previewLabel("requirements") + "}\n";
		appendPreviewLimiter(text, quest.mission);
		if(quest.lastDay >= 0)
			text += previewLabel("deadline") + " " + std::to_string(quest.lastDay + 1) + "\n";
		if(quest.killTarget.hasValue())
		{
			text += previewLabel("defeatTarget") + " " + quest.heroName + "\n";
			if(quest.stackToKill.hasValue())
				appendPreviewComponents(text, {Component(ComponentType::CREATURE, quest.stackToKill)});
		}
		if(quest.isCompleted)
			text += previewLabel("completed") + "\n";
	}
	std::vector<Component> guards;
	for(const auto & slot : rewardable->Slots())
		if(slot.second)
			guards.emplace_back(ComponentType::CREATURE, slot.second->getCreatureID(), slot.second->getCount());
	if(!guards.empty())
	{
		text += "\n{" + previewLabel("guards") + "}\n";
		appendPreviewComponents(text, guards);
	}
	const auto & config = rewardable->configuration;
	if(static_cast<size_t>(config.selectMode) < Rewardable::SelectModeString.size())
		text += "\n" + previewLabel(Rewardable::SelectModeString[config.selectMode].data()) + "\n";
	else
		text += previewLabel("additionalConditions") + "\n";
	if(config.visitMode == Rewardable::VISIT_LIMITER)
	{
		text += "\n" + previewLabel("visitLimiter") + ":\n";
		appendPreviewLimiter(text, config.visitLimiter);
	}
	int index = 0;
	for(const auto & info : config.info)
	{
		text += "\n{" + previewLabel("reward") + " " + std::to_string(++index) + "}\n";
		switch(info.visitType)
		{
			case Rewardable::EEventType::EVENT_FIRST_VISIT: text += previewLabel("firstVisit"); break;
			case Rewardable::EEventType::EVENT_ALREADY_VISITED: text += previewLabel("repeatVisit"); break;
			case Rewardable::EEventType::EVENT_NOT_AVAILABLE: text += previewLabel("unavailableVisit"); break;
			case Rewardable::EEventType::EVENT_GUARDED: text += previewLabel("guardedVisit"); break;
			default: text += previewLabel("invalidEvent"); break;
		}
		text += "\n";
		if(!info.description.empty())
			text += info.description.toString() + "\n";
		if(!info.message.empty())
			text += info.message.toString() + "\n";
		text += previewLabel("requirements") + ":\n";
		appendPreviewLimiter(text, info.limiter);
		text += previewLabel("contents") + ":\n";
		std::vector<Component> components;
		// Separate removals: the standard component list includes both granted and taken items.
		auto displayedReward = info.reward;
		displayedReward.extraComponents.clear();
		displayedReward.takenArtifacts.clear();
		displayedReward.takenArtifactSlots.clear();
		displayedReward.takenScrolls.clear();
		displayedReward.grantedScrolls.clear();
		displayedReward.loadComponents(components, hero);
		appendPreviewComponents(text, components);
		for(const auto & scroll : info.reward.grantedScrolls)
		{
			text += previewLabel("grantedScroll") + ":\n";
			appendPreviewComponents(text, {Component(ComponentType::SPELL_SCROLL, scroll)});
		}
		for(const auto & skill : info.reward.secondary)
		{
			appendPreviewComponents(text, {Component(ComponentType::SEC_SKILL, skill.first)});
			appendPreviewValue(text, "skillLevelDelta", skill.second);
		}
		std::vector<Component> removals;
		for(const auto & artifact : info.reward.takenArtifacts)
			removals.emplace_back(ComponentType::ARTIFACT, artifact);
		for(const auto & scroll : info.reward.takenScrolls)
		{
			text += previewLabel("removedScroll") + ":\n";
			appendPreviewComponents(text, {Component(ComponentType::SPELL_SCROLL, scroll)});
		}
		for(const auto & slot : info.reward.takenArtifactSlots)
			appendPreviewValue(text, "takenSlot", slot.getNum());
		for(const auto & creature : info.reward.takenCreatures)
			removals.emplace_back(ComponentType::CREATURE, creature.getId(), creature.getCount());
		if(!removals.empty())
		{
			text += previewLabel("removed") + ":\n";
			appendPreviewComponents(text, removals);
		}
		if(info.reward.manaDiff || info.reward.manaPercentage >= 0)
		{
			appendPreviewValue(text, "manaDiff", info.reward.manaDiff);
			appendPreviewValue(text, "manaOverflowFactor", info.reward.manaOverflowFactor);
		}
		if(info.reward.movePoints || info.reward.movePercentage >= 0)
		{
			appendPreviewValue(text, "movePointsDelta", info.reward.movePoints);
			appendPreviewValue(text, "moveOverflowFactor", info.reward.moveOverflowFactor);
		}
		if(info.reward.manaPercentage >= 0)
			appendPreviewValue(text, "manaSetPercentage", info.reward.manaPercentage);
		if(info.reward.movePercentage >= 0)
			appendPreviewValue(text, "moveSetPercentage", info.reward.movePercentage);
		if(hero && (info.reward.movePoints || info.reward.movePercentage >= 0))
			appendPreviewValue(text, "resultMovePoints", info.reward.calculateMovePoints(hero));
		appendPreviewBonuses(text, "heroBonuses", info.reward.heroBonuses);
		appendPreviewBonuses(text, "playerBonuses", info.reward.playerBonuses);
		appendPreviewBonuses(text, "commanderBonuses", info.reward.commanderBonuses);
		for(const auto & change : info.reward.creaturesChange)
		{
			text += previewLabel("creatureChange") + ":\n";
			appendPreviewComponents(text, {Component(ComponentType::CREATURE, change.first)});
			text += " ->\n";
			appendPreviewComponents(text, {Component(ComponentType::CREATURE, change.second)});
		}
		if(info.reward.spellCast.first.hasValue())
		{
			text += previewLabel("spellCast") + ":\n";
			appendPreviewComponents(text, {Component(ComponentType::SPELL, info.reward.spellCast.first)});
			appendPreviewValue(text, "spellMastery", info.reward.spellCast.second);
		}
		if(info.reward.removeObject)
			text += previewLabel("removeObject") + "\n";
		if(info.reward.revealTiles)
		{
			const auto & reveal = *info.reward.revealTiles;
			text += previewLabel(reveal.hide ? "hideTiles" : "revealTiles") + "\n";
			appendPreviewValue(text, "radius", reveal.radius);
			appendPreviewValue(text, "scoreSurface", reveal.scoreSurface);
			appendPreviewValue(text, "scoreSubterra", reveal.scoreSubterra);
			appendPreviewValue(text, "scoreWater", reveal.scoreWater);
			appendPreviewValue(text, "scoreRock", reveal.scoreRock);
		}
		if(!info.reward.extraComponents.empty())
		{
			text += previewLabel("displayOnly") + ":\n";
			appendPreviewComponents(text, info.reward.extraComponents);
		}
		if(!info.reward.guards.empty())
		{
			text += previewLabel("guards") + ":\n";
			std::vector<Component> rewardGuards;
			for(const auto & guard : info.reward.guards)
				rewardGuards.emplace_back(ComponentType::CREATURE, guard.getId(), guard.getCount());
			appendPreviewComponents(text, rewardGuards);
		}
	}
	if(index == 0)
		text += previewLabel("noConfiguredEvents") + "\n";
	if(config.resetParameters.period)
	{
		text += "\n" + previewLabel("resetDays") + " " + std::to_string(config.resetParameters.period) + "\n";
		if(config.resetParameters.visitors)
			text += previewLabel("resetVisitors") + "\n";
		if(config.resetParameters.rewards)
			text += previewLabel("resetRewards") + "\n";
	}
	return true;
}
}
