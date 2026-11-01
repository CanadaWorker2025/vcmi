/*
 * InfoWindows.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "InfoWindows.h"

#include "../CPlayerInterface.h"
#include "../PlayerLocalState.h"

#include "../adventureMap/AdventureMapInterface.h"
#include "../adventureMap/CMinimap.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/CursorHandler.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../widgets/Buttons.h"
#include "../widgets/CComponent.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/Images.h"
#include "../widgets/MiscWidgets.h"
#include "../widgets/TextControls.h"
#include "../widgets/Slider.h"
#include "../windows/CMessage.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/gameState/InfoAboutArmy.h"
#include "../../lib/mapObjects/CGCreature.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/mapObjects/CGMarket.h"
#include "../../lib/mapObjects/army/CStackInstance.h"
#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/mapObjects/MiscObjects.h"
#include "../../lib/ConditionalWait.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/StartInfo.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/entities/faction/CTown.h"

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
		boost::replace_all(line, "\n", " ");
		if(!line.empty())
			text += "  " + line + "\n";
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
	displayLimiter.loadComponents(components, nullptr);
	appendPreviewComponents(text, components);
	if(limiter.heroLevel)
		text += "  " + previewLabel("level") + " " + std::to_string(limiter.heroLevel) + "\n";
	if(limiter.daysPassed)
		text += "  " + previewLabel("daysPassed") + " " + std::to_string(limiter.daysPassed) + "\n";
	if(limiter.dayOfWeek)
		text += "  " + previewLabel("dayOfWeek") + " " + std::to_string(limiter.dayOfWeek) + "\n";
	if(limiter.movePoints || limiter.movePercentage || limiter.manaPercentage || limiter.canLearnSkills
		|| limiter.commanderAlive || limiter.hasExtraCreatures || !limiter.heroClasses.empty()
		|| !limiter.canLearnSpells.empty() || !limiter.canReceiveCreatures.empty()
		|| !limiter.availableSlots.empty() || !limiter.scrolls.empty())
		text += "  " + previewLabel("additionalConditions") + "\n";
	for(const auto & group : {std::make_pair("allOf", &limiter.allOf),
		std::make_pair("anyOf", &limiter.anyOf), std::make_pair("noneOf", &limiter.noneOf)})
	{
		if(group.second->empty())
			continue;
		text += "  " + previewLabel(group.first) + "\n";
		for(const auto & child : *group.second)
			if(child)
				appendPreviewLimiter(text, *child, depth + 1);
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
	text = "{" + object->getObjectName() + "}\n\n";
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
	if(rewardable->configuration.visitMode != Rewardable::VISIT_UNLIMITED)
		text += previewLabel((hero ? rewardable->wasVisited(hero) : rewardable->wasVisited(GAME->interface()->playerID))
			? "visited" : "notVisited") + "\n";
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
	text += "\n" + previewLabel(Rewardable::SelectModeString.at(config.selectMode).data()) + "\n";
	int index = 0;
	for(const auto & info : config.info)
	{
		if(info.visitType != Rewardable::EEventType::EVENT_FIRST_VISIT)
			continue;
		text += "\n{" + previewLabel("reward") + " " + std::to_string(++index) + "}\n";
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
		displayedReward.loadComponents(components, nullptr);
		appendPreviewComponents(text, components);
		std::vector<Component> removals;
		for(const auto & artifact : info.reward.takenArtifacts)
			removals.emplace_back(ComponentType::ARTIFACT, artifact);
		for(const auto & scroll : info.reward.takenScrolls)
			removals.emplace_back(ComponentType::SPELL_SCROLL, scroll);
		for(const auto & creature : info.reward.takenCreatures)
			removals.emplace_back(ComponentType::CREATURE, creature.getId(), creature.getCount());
		if(!removals.empty())
		{
			text += previewLabel("removed") + ":\n";
			appendPreviewComponents(text, removals);
		}
		if(info.reward.movePoints || info.reward.movePercentage >= 0 || info.reward.manaPercentage >= 0
			|| !info.reward.heroBonuses.empty() || !info.reward.playerBonuses.empty()
			|| !info.reward.commanderBonuses.empty() || !info.reward.creaturesChange.empty()
			|| !info.reward.takenCreatures.empty() || !info.reward.takenArtifactSlots.empty()
			|| info.reward.spellCast.first.hasValue() || info.reward.revealTiles)
			text += previewLabel("additionalEffects") + "\n";
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
		text += previewLabel("noReward") + "\n";
	if(config.resetParameters.period)
		text += "\n" + previewLabel("resetDays") + " " + std::to_string(config.resetParameters.period) + "\n";
	return true;
}
}

CSelWindow::CSelWindow( const std::string & Text, PlayerColor player, int charperline, const std::vector<std::shared_ptr<CSelectableComponent>> & comps, const std::vector<std::pair<AnimationPath, CFunctionList<void()>>> & Buttons, QueryID askID)
{
	OBJECT_CONSTRUCTION;

	backgroundTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DiBoxBck"), pos);

	ID = askID;
	for(int i = 0; i < Buttons.size(); i++)
	{
		buttons.push_back(std::make_shared<CButton>(Point(0, 0), Buttons[i].first, CButton::tooltip(), Buttons[i].second));
		if(!i && askID.getNum() >= 0)
			buttons.back()->addCallback(std::bind(&CSelWindow::madeChoice, this));
		buttons[i]->addCallback(std::bind(&CInfoWindow::close, this)); //each button will close the window apart from call-defined actions
	}

	text = std::make_shared<CTextBox>(Text, Rect(0, 0, 250, 100), 0, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE);

	if(buttons.size() > 1 && askID.getNum() >= 0) //cancel button functionality
		buttons.back()->addCallback([askID](){GAME->interface()->cb->selectionMade(0, askID);});

	if(buttons.size() == 1)
		buttons.front()->assignedKey = EShortcut::GLOBAL_RETURN;

	if(buttons.size() == 2)
	{
		buttons.front()->assignedKey = EShortcut::GLOBAL_ACCEPT;
		buttons.back()->assignedKey = EShortcut::GLOBAL_CANCEL;
	}

	if(!comps.empty())
	{
		components = std::make_shared<CComponentBox>(comps, Rect(0,0,0,0));
		for (auto & comp : comps)
			comp->onChoose = [this](){ madeChoiceAndClose(); };
	}

	CMessage::drawIWindow(this, Text, player);
}

void CSelWindow::madeChoice()
{
	if(ID.getNum() < 0)
		return;
	int ret = -1;
	if(components)
		ret = components->selectedIndex();

	GAME->interface()->cb->selectionMade(ret + 1, ID);
}

void CSelWindow::madeChoiceAndClose()
{
	madeChoice();
	close();
}

CInfoWindow::CInfoWindow(const std::string & Text, PlayerColor player, const TCompsInfo & comps, const TButtonsInfo & Buttons)
{
	OBJECT_CONSTRUCTION;

	backgroundTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DiBoxBck"), pos);

	ID = QueryID(-1);
	for(const auto & Button : Buttons)
	{
		auto button = std::make_shared<CButton>(Point(0, 0), Button.first, CButton::tooltip(), std::bind(&CInfoWindow::close, this));
		button->setBorderColor(Colors::METALLIC_GOLD);
		button->addCallback(Button.second); //each button will close the window apart from call-defined actions
		buttons.push_back(button);
	}

	text = std::make_shared<CTextBox>(Text, Rect(0, 0, 250, 100), 0, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE);
	if(!text->slider)
	{
		int finalWidth = std::min(250, text->label->textSize.x + 32);
		int finalHeight = text->label->textSize.y;
		text->resize(Point(finalWidth, finalHeight));
	}

	if(buttons.size() == 1)
		buttons.front()->assignedKey = EShortcut::GLOBAL_RETURN;

	if(buttons.size() == 2)
	{
		buttons.front()->assignedKey = EShortcut::GLOBAL_ACCEPT;
		buttons.back()->assignedKey = EShortcut::GLOBAL_CANCEL;
	}

	if(!comps.empty())
		components = std::make_shared<CComponentBox>(comps, Rect(0,0,0,0));

	CMessage::drawIWindow(this, Text, player);
}

CInfoWindow::CInfoWindow()
{
	ID = QueryID(-1);
}

void CInfoWindow::close()
{
	WindowBase::close();

	if(GAME->interface())
		GAME->interface()->showingDialog->setFree();
}

void CInfoWindow::showAll(Canvas & to)
{
	CIntObject::showAll(to);
	CMessage::drawBorder(GAME->interface() ? GAME->interface()->playerID : PlayerColor(1), to, pos.w+28, pos.h+29, pos.x-14, pos.y-15);
}

CInfoWindow::~CInfoWindow() = default;

void CInfoWindow::showInfoDialog(const std::string & text, const TCompsInfo & components, PlayerColor player)
{
	ENGINE->windows().pushWindow(CInfoWindow::create(text, player, components));
}

void CInfoWindow::showYesNoDialog(const std::string & text, const TCompsInfo & components, const CFunctionList<void()> & onYes, const CFunctionList<void()> & onNo, PlayerColor player)
{
	assert(!GAME->interface() || GAME->interface()->showingDialog->isBusy());
	std::vector<std::pair<AnimationPath, CFunctionList<void()>>> pom;
	pom.emplace_back(AnimationPath::builtin("IOKAY.DEF"), nullptr);
	pom.emplace_back(AnimationPath::builtin("ICANCEL.DEF"), nullptr);
	auto temp = std::make_shared<CInfoWindow>(text, player, components, pom);

	temp->buttons[0]->addCallback(onYes);
	temp->buttons[1]->addCallback(onNo);

	ENGINE->windows().pushWindow(temp);
}

std::shared_ptr<CInfoWindow> CInfoWindow::create(const std::string & text, PlayerColor playerID, const TCompsInfo & components)
{
	std::vector<std::pair<AnimationPath, CFunctionList<void()>>> pom;
	pom.emplace_back(AnimationPath::builtin("IOKAY.DEF"), nullptr);
	return std::make_shared<CInfoWindow>(text, playerID, components, pom);
}

std::string CInfoWindow::genText(const std::string & title, const std::string & description)
{
	return std::string("{") + title + "}" + "\n\n" + description;
}

bool CRClickPopup::isPopupWindow() const
{
	return true;
}

void CRClickPopup::createAndPush(const std::string & txt, const CInfoWindow::TCompsInfo & comps)
{
	PlayerColor player = GAME->interface() ? GAME->interface()->playerID : PlayerColor(1); //if no player, then use blue
	if(settings["session"]["spectate"].Bool()) //TODO: there must be better way to implement this
		player = PlayerColor(1);

	auto temp = std::make_shared<CInfoWindow>(txt, player, comps);
	temp->center(ENGINE->getCursorPosition()); //center on mouse
#ifdef VCMI_MOBILE
	temp->moveBy({0, -temp->pos.h / 2});
#endif
	temp->fitToScreen(10);

	ENGINE->windows().createAndPushWindow<CRClickPopupInt>(temp);
}

void CRClickPopup::createAndPush(const std::string & txt, const std::shared_ptr<CComponent> & component)
{
	CInfoWindow::TCompsInfo intComps;
	intComps.push_back(component);

	createAndPush(txt, intComps);
}

void CRClickPopup::createAndPush(const CGObjectInstance * obj, const Point & p, ETextAlignment alignment)
{
	std::string details;
	if(makeObjectPreview(obj, details))
	{
		ENGINE->windows().createAndPushWindow<MapObjectDetailsPopup>(p, details);
		return;
	}
	auto iWin = createCustomInfoWindow(p, obj); //try get custom infowindow for this obj
	if(iWin)
	{
		ENGINE->windows().pushWindow(iWin);
	}
	else
	{
		std::vector<Component> components;
		if(settings["general"]["enableUiEnhancements"].Bool())
		{
			if(GAME->interface()->localState->getCurrentHero())
				components = obj->getPopupComponents(GAME->interface()->localState->getCurrentHero());
			else
				components = obj->getPopupComponents(GAME->interface()->playerID);
		}

		std::vector<std::shared_ptr<CComponent>> guiComponents;
		for(auto & component : components)
			guiComponents.push_back(std::make_shared<CComponent>(component, CComponent::medium));

		if(GAME->interface()->localState->getCurrentHero())
			CRClickPopup::createAndPush(obj->getPopupText(GAME->interface()->localState->getCurrentHero()), guiComponents);
		else
			CRClickPopup::createAndPush(obj->getPopupText(GAME->interface()->playerID), guiComponents);
	}
}

CRClickPopupInt::CRClickPopupInt(const std::shared_ptr<CIntObject> & our) :
	dragDistance(Point(0, 0))
{
	addUsedEvents(DRAG_POPUP);

	ENGINE->cursor().hide();
	inner = our;
	addChild(our.get(), false);
}

CRClickPopupInt::~CRClickPopupInt()
{
	ENGINE->cursor().show();
}

void CRClickPopupInt::mouseDraggedPopup(const Point & cursorPosition, const Point & lastUpdateDistance)
{
	if(!settings["adventure"]["rightButtonDrag"].Bool())
		return;
	
	dragDistance += lastUpdateDistance;
	
	if(dragDistance.length() > 16)
		close();
}

template<typename... Args>
AdventureMapPopup::AdventureMapPopup(Args&&... args) :
	CWindowObject(std::forward<Args>(args)...), dragDistance(Point(0, 0))
{
	addUsedEvents(DRAG_POPUP);
}

void AdventureMapPopup::mouseDraggedPopup(const Point & cursorPosition, const Point & lastUpdateDistance)
{
	if(!settings["adventure"]["rightButtonDrag"].Bool())
		return;
	
	dragDistance += lastUpdateDistance;
	
	if(dragDistance.length() > 16)
		close();
}

MapObjectDetailsPopup::MapObjectDetailsPopup(const Point & position, const std::string & text)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	const auto screen = ENGINE->screenDimensions();
	pos.w = std::min(520, screen.x - 40);
	pos.h = std::min(520, screen.y - 60);
	texture = std::make_shared<CFilledTexture>(ImagePath::builtin("DiBoxBck"), Rect(0, 0, pos.w, pos.h));
	details = std::make_shared<CTextBox>(text, Rect(14, 12, pos.w - 28, pos.h - 24), 0, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE);
	center(position);
	fitToScreen(10);
}


CInfoBoxPopup::CInfoBoxPopup(Point position, const CGTownInstance * town)
	: AdventureMapPopup(RCLICK_POPUP | PLAYER_COLORED, ImagePath::builtin("TOWNQVBK"), position)
{
	InfoAboutTown iah;
	GAME->interface()->cb->getTownInfo(town, iah, GAME->interface()->localState->getCurrentArmy()); //todo: should this be nearest hero?

	OBJECT_CONSTRUCTION;
	tooltip = std::make_shared<CTownTooltip>(Point(9, 10), iah);

	if(settings["general"]["enableUiEnhancements"].Bool())
		background->setPlayerColor(town->getOwner());

	addUsedEvents(DRAG_POPUP);

	fitToScreen(10);
}

CInfoBoxPopup::CInfoBoxPopup(Point position, const CGHeroInstance * hero)
	: AdventureMapPopup(RCLICK_POPUP | PLAYER_COLORED, ImagePath::builtin("HEROQVBK"), position)
{
	InfoAboutHero iah;
	GAME->interface()->cb->getHeroInfo(hero, iah, GAME->interface()->localState->getCurrentArmy()); //todo: should this be nearest hero?

	OBJECT_CONSTRUCTION;
	tooltip = std::make_shared<CHeroTooltip>(Point(9, 10), iah);

	if(settings["general"]["enableUiEnhancements"].Bool())
		background->setPlayerColor(hero->getOwner());
	
	addUsedEvents(DRAG_POPUP);

	fitToScreen(10);
}

CInfoBoxPopup::CInfoBoxPopup(Point position, const CGGarrison * garr)
	: AdventureMapPopup(RCLICK_POPUP | PLAYER_COLORED, ImagePath::builtin(settings["general"]["enableUiEnhancements"].Bool() ? "GARRIPOP" : "TOWNQVBK"), position)
{
	InfoAboutTown iah;
	GAME->interface()->cb->getTownInfo(garr, iah);

	OBJECT_CONSTRUCTION;

	if(settings["general"]["enableUiEnhancements"].Bool())
	{
        tooltip = std::make_shared<CGarrisonTooltip>(Point(9, 10), iah);
	}
	else
	{
        tooltip = std::make_shared<CArmyTooltip>(Point(9, 10), iah);
	}

	if(settings["general"]["enableUiEnhancements"].Bool())
		background->setPlayerColor(garr->getOwner());

	addUsedEvents(DRAG_POPUP);

	fitToScreen(10);
}

CInfoBoxPopup::CInfoBoxPopup(Point position, const CGCreature * creature)
	: AdventureMapPopup(RCLICK_POPUP | BORDERED, ImagePath::builtin("DIBOXBCK"), position)
{
	OBJECT_CONSTRUCTION;
	tooltip = std::make_shared<CreatureTooltip>(Point(9, 10), creature);
	
	addUsedEvents(DRAG_POPUP);

	fitToScreen(10);
}

MinimapWithIcons::MinimapWithIcons(const Point & position)
{
	OBJECT_CONSTRUCTION;
	pos += position;

	recreate();
}

void MinimapWithIcons::recreate()
{
	OBJECT_CONSTRUCTION;

	Rect area1(11, 41, 144, 144);
	Rect area2(167, 41, 144, 144);

	Rect border1(10, 40, 147, 147);
	Rect border2(166, 40, 147, 147);

	int levels = GAME->interface()->cb->getMapSize().z;
	int currentLevel = slider ? slider->getValue() : 0;
	bool singleLevelMap = levels == 1;

	if(levels > 2)
	{
		slider = std::make_shared<CSlider>(Point(10, 192), 303, [this](int value){ recreate(); setRedrawParent(true); redraw(); }, 2, levels, currentLevel, Orientation::HORIZONTAL);
		slider->setPanningStep(147);
	}

	if (singleLevelMap)
	{
		area1.x += 78;
		border1.x += 78;
	}

	background1 = std::make_shared<TransparentFilledRectangle>(border1, Colors::TRANSPARENCY, Colors::YELLOW);
	map1 = std::make_shared<CMinimapInstance>(area1.topLeft(), area1.dimensions(), currentLevel);

	if (!singleLevelMap)
	{
		background2 = std::make_shared<TransparentFilledRectangle>(border2, Colors::TRANSPARENCY, Colors::YELLOW);
		map2 = std::make_shared<CMinimapInstance>(area2.topLeft(), area2.dimensions(), currentLevel + 1);
	}

	iconsOverlay.clear();
	for(auto & icon : icons)
	{
		int positionX = 144 * icon.first.x / GAME->interface()->cb->getMapSize().x;
		int positionY = 144 * icon.first.y / GAME->interface()->cb->getMapSize().y;

		Point iconPosition(positionX, positionY);

		iconPosition -= Point(8,8); // compensate for 16x16 icon half-size

		if (icon.first.z == currentLevel)
			iconPosition += area1.topLeft();
		else if (icon.first.z == currentLevel + 1)
			iconPosition += area2.topLeft();
		else
			continue;

		iconsOverlay.push_back(std::make_shared<CPicture>(icon.second, iconPosition));
	}
}

void MinimapWithIcons::addIcon(const int3 & coordinates, const ImagePath & image )
{
	icons.push_back({coordinates, image});
}

TeleporterPopup::TeleporterPopup(const Point & position, const CGTeleport * teleporter)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 200 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, teleporter->getPopupText(GAME->interface()->playerID));
	minimap = std::make_shared<MinimapWithIcons>(Point(0,0));

	const auto & entrances = teleporter->getAllEntrances();
	const auto & exits = teleporter->getAllExits();

	std::set<ObjectInstanceID> allTeleporters;
	allTeleporters.insert(entrances.begin(), entrances.end());
	allTeleporters.insert(exits.begin(), exits.end());

	for (const auto exit : allTeleporters)
	{
		const auto * exitObject = GAME->interface()->cb->getObj(exit, false);

		if (!exitObject)
			continue;

		int3 position = exitObject->visitablePos();
		ImagePath image;

		if (!vstd::contains(entrances, exit))
			image = ImagePath::builtin("minimapIcons/portalExit");
		else if (!vstd::contains(exits, exit))
			image = ImagePath::builtin("minimapIcons/portalEntrance");
		else
			image = ImagePath::builtin("minimapIcons/portalBidirectional");

		minimap->addIcon(position, image);
	}
	minimap->recreate();
	center(position);
	fitToScreen(10);
}

KeymasterPopup::KeymasterPopup(const Point & position, const CGKeys * keymasterOrGuard)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 220 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, keymasterOrGuard->getObjectName());
	labelDescription = std::make_shared<CLabel>(pos.w / 2, 40, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE, keymasterOrGuard->getObjectDescription(GAME->interface()->playerID));
	minimap = std::make_shared<MinimapWithIcons>(Point(0,20));

	const auto allObjects = GAME->interface()->cb->getAllVisitableObjs();

	for (const auto mapObject : allObjects)
	{
		if (!mapObject)
			continue;

		switch (mapObject->ID)
		{
			case Obj::KEYMASTER:
				if (mapObject->subID == keymasterOrGuard->subID)
					minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/keymaster"));
				break;
			case Obj::BORDERGUARD:
				if (mapObject->subID == keymasterOrGuard->subID)
					minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/borderguard"));
				break;
			case Obj::BORDER_GATE:
				if (mapObject->subID == keymasterOrGuard->subID)
					minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/bordergate"));
				break;
		}
	}
	minimap->recreate();
	center(position);
	fitToScreen(10);
}

ObeliskPopup::ObeliskPopup(const Point & position, const CGObelisk * obelisk)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 220 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, obelisk->getObjectName());
	labelDescription = std::make_shared<CLabel>(pos.w / 2, 40, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE, obelisk->getObjectDescription(GAME->interface()->playerID));
	minimap = std::make_shared<MinimapWithIcons>(Point(0,20));

	const auto allObjects = GAME->interface()->cb->getAllVisitableObjs();

	for (const auto mapObject : allObjects)
	{
		if (!mapObject)
			continue;

		if (mapObject->ID != Obj::OBELISK)
			continue;

		if (mapObject->wasVisited(GAME->interface()->playerID))
			minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/obeliskVisited"));
		else
			minimap->addIcon(mapObject->visitablePos(), ImagePath::builtin("minimapIcons/obelisk"));
	}
	minimap->recreate();
	center(position);
	fitToScreen(10);
}

SearchPopup::SearchPopup(std::vector<const CGObjectInstance *> objs)
	: AdventureMapPopup(BORDERED | RCLICK_POPUP)
{
	OBJECT_CONSTRUCTION;
	pos.w = 322;
	pos.h = 220 + (GAME->interface()->cb->getMapSize().z > 2 ? 21 : 0);

	if(!objs.size())
		return;

	auto name = GAME->interface()->cb->getObjInstance(objs.at(0)->id)->getObjectName();

	filledBackground = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	labelTitle = std::make_shared<CLabel>(pos.w / 2, 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE, name);
	minimap = std::make_shared<MinimapWithIcons>(Point(0,20));

	for (const auto obj : objs)
		minimap->addIcon(obj->visitablePos(), ImagePath::builtin("minimapIcons/generic"));
	
	minimap->recreate();
	center();
	fitToScreen(10);
}

std::shared_ptr<WindowBase>
CRClickPopup::createCustomInfoWindow(Point position, const CGObjectInstance * specific) //specific=0 => draws info about selected town/hero
{
	if(nullptr == specific)
		specific = GAME->interface()->localState->getCurrentArmy();

	if(nullptr == specific)
	{
		logGlobal->error("createCustomInfoWindow: no object to describe");
		return nullptr;
	}

	switch(specific->ID)
	{
		case Obj::HERO:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGHeroInstance *>(specific));
		case Obj::TOWN:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGTownInstance *>(specific));
		case Obj::MONSTER:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGCreature *>(specific));
		case Obj::GARRISON:
		case Obj::GARRISON2:
			return std::make_shared<CInfoBoxPopup>(position, dynamic_cast<const CGGarrison *>(specific));
		case Obj::MONOLITH_ONE_WAY_ENTRANCE:
		case Obj::MONOLITH_ONE_WAY_EXIT:
		case Obj::MONOLITH_TWO_WAY:
		case Obj::SUBTERRANEAN_GATE:
		case Obj::WHIRLPOOL:
			return std::make_shared<TeleporterPopup>(position, dynamic_cast<const CGTeleport *>(specific));
		case Obj::KEYMASTER:
		case Obj::BORDERGUARD:
		case Obj::BORDER_GATE:
			return std::make_shared<KeymasterPopup>(position, dynamic_cast<const CGKeys *>(specific));
		case Obj::OBELISK:
			return std::make_shared<ObeliskPopup>(position, dynamic_cast<const CGObelisk *>(specific));
		default:
			return std::shared_ptr<WindowBase>();
	}
}
