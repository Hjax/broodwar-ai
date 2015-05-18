#include "ExampleAIModule.h"
#include <iostream>

using namespace BWAPI;
using namespace Filter;

void ExampleAIModule::onStart()
{
	// Hello World!
	Broodwar->sendText("Hello world!");
	Broodwar->setLocalSpeed(0);
	// init some variables
	Barracks_timer = 0;
	attack_timer = 0;
	supply_timer = 0;
	expo_timer = 0;

	// Print the map name.
	// BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
	Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

	// Enable the UserInput flag, which allows us to control the bot and type messages.
	Broodwar->enableFlag(Flag::UserInput);

	// Uncomment the following line and the bot will know about everything through the fog of war (cheat).
	Broodwar->enableFlag(Flag::CompleteMapInformation);

	// Set the command optimization level so that common commands can be grouped
	// and reduce the bot's APM (Actions Per Minute).
	Broodwar->setCommandOptimizationLevel(2);

	// Check if this is a replay
	if (Broodwar->isReplay())
	{

		// Announce the players in the replay
		Broodwar << "The following players are in this replay:" << std::endl;

		// Iterate all the players in the game using a std:: iterator
		Playerset players = Broodwar->getPlayers();
		for (auto p : players)
		{
			// Only print the player if they are not an observer
			if (!p->isObserver())
				Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
		}

	}
	else // if this is not a replay
	{
		// Retrieve you and your enemy's races. enemy() will just return the first enemy.
		// If you wish to deal with multiple enemies then you must use enemies().
		if (Broodwar->enemy()) // First make sure there is an enemy
			Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
	}

}

void ExampleAIModule::onEnd(bool isWinner)
{
	// Called when the game ends
	if (isWinner)
	{
		// Log your win here!
	}
}

void ExampleAIModule::onFrame()
{

	int free_mins = Broodwar->self()->minerals();
	Broodwar->drawTextScreen(200, 0, "FPS: %d", Broodwar->getFPS());
	Broodwar->drawTextScreen(200, 20, "Average FPS: %f", Broodwar->getAverageFPS());
	Broodwar->drawTextScreen(200, 40, "APM: %d", Broodwar->getAPM());
	int marines = Broodwar->self()->allUnitCount(UnitTypes::Terran_Marine);
	int scvs = Broodwar->self()->allUnitCount(UnitTypes::Terran_SCV);
	Broodwar->drawTextScreen(200, 60, "Marines: %d", marines);
	Broodwar->drawTextScreen(200, 80, "Workers: %d", scvs);
	Broodwar->drawTextScreen(200, 100, "Next Depot %d", ((2 * Broodwar->self()->allUnitCount(UnitTypes::Terran_Barracks))) + 2);
	Broodwar->drawTextScreen(100, 0, "Optimal CCs: %d", (scvs / 24) + 1);
	
	if (marines >= 20){
		Broodwar->setLocalSpeed(-1);
	}
	if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
		return;
	if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
		return;

	for (auto &u : Broodwar->self()->getUnits())
	{
		if (!u->exists())
			continue;

		if (u->isLockedDown() || u->isMaelstrommed() || u->isStasised())
			continue;

		if (u->isLoaded() || !u->isPowered() || u->isStuck())
			continue;

		if (!u->isCompleted() || u->isConstructing())
			continue;

		if (u->getType().isWorker()){
			if (u->isIdle() || u->isGatheringMinerals()){
				if ((Broodwar->self()->allUnitCount(UnitTypes::Terran_Command_Center) < ((scvs / 24) + 1)) &&
					(free_mins >= UnitTypes::Terran_Command_Center.mineralPrice()) &&
					expo_timer + 720 < Broodwar->getFrameCount())
				{
					expo_timer = Broodwar->getFrameCount();
					TilePosition buildPosition = Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Command_Center, u->getTilePosition());
					u->build(UnitTypes::Terran_Command_Center, buildPosition);
					free_mins -= 400;
				}
				else if ((Broodwar->self()->allUnitCount(UnitTypes::Terran_Barracks) < 3 || free_mins >= 800) &&
					(free_mins >= UnitTypes::Terran_Barracks.mineralPrice()) &&
					Barracks_timer + 200 < Broodwar->getFrameCount())
				{
					Barracks_timer = Broodwar->getFrameCount();
					TilePosition buildPosition = Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Barracks, u->getTilePosition());
					u->build(UnitTypes::Terran_Barracks, buildPosition);
					free_mins -= 150;
				}
				else if (u->isIdle()){
					if (u->isCarryingGas() || u->isCarryingMinerals())
					{
						u->returnCargo();
					}
					else if (!u->getPowerUp()){
						if (!u->gather(u->getClosestUnit(IsResourceDepot)->getClosestUnit(IsMineralField || IsRefinery)))
						{
							Broodwar << Broodwar->getLastError() << std::endl;
						}
					}
				} // closure: has no powerup
			} // closure: if idle

		}
		else if (u->getType() == UnitTypes::Terran_Command_Center)
		{
			if (u->isIdle() && scvs < 72 && free_mins >= 50 && scvs < 24 * Broodwar->self()->allUnitCount(UnitTypes::Terran_Command_Center)){
				u->train(u->getType().getRace().getWorker());
				free_mins -= 50;
			}

			UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
			if (Broodwar->self()->supplyTotal() - Broodwar->self()->supplyUsed() <= ((2 * Broodwar->self()->allUnitCount(UnitTypes::Terran_Barracks)) + 2) &&
				Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0 &&
				free_mins >= 100 &&
				Broodwar->getFrameCount() - supply_timer > 150)
			{
				Unit supplyBuilder = u->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
					(IsIdle || IsGatheringMinerals) &&
					IsOwned);
				if (supplyBuilder)
				{
					TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
					if (targetBuildLocation)
					{
						Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
						{
							Broodwar->drawBoxMap(Position(targetBuildLocation),
								Position(targetBuildLocation + supplyProviderType.tileSize()),
								Colors::Blue);
						},
							nullptr,  // condition
							supplyProviderType.buildTime() + 100);  // frames to run

						supplyBuilder->build(supplyProviderType, targetBuildLocation);
						supply_timer = Broodwar->getFrameCount();
						free_mins -= 100;
					}
				} // closure: supplyBuilder is valid
			} // closure: insufficient supply
		} // closure: failed to train idle unit


		else if (u->getType() == UnitTypes::Terran_Marine)
		{

			Unit closestEnemy = NULL;
			for (auto &e : Broodwar->enemy()->getUnits())
			{
				if ((closestEnemy == NULL) || (e->getDistance(u) < closestEnemy->getDistance(u)))
				{
					closestEnemy = e;
				}
			}
			if (u->getGroundWeaponCooldown() > 0 && (Broodwar->getFrameCount() - attack_timer) > 24 && closestEnemy->getDistance(u) < 50){
				u->move(u->getClosestUnit(IsResourceDepot && IsAlly)->getPosition());
				Broodwar->drawTextMap(u->getPosition(), "%c%s", Text::White, "Fleeing");   // action
			}
			else if ((marines >= 20 || closestEnemy->getDistance(u) < 200) && (Broodwar->getFrameCount() - attack_timer) > 24){
				u->attack(closestEnemy->getPosition(), false);
				Broodwar->drawTextMap(u->getPosition(), "%c%s", Text::White, "Attacking");
			}

		}
		else if ((u->getType() == UnitTypes::Terran_Barracks) && u->isIdle() && free_mins >= 50){
			u->train(UnitTypes::Terran_Marine);
			free_mins -= 50;
		}
	} // closure: unit iterator
	if (Broodwar->getFrameCount() - attack_timer > 24){
		attack_timer = Broodwar->getFrameCount();
	}
}

void ExampleAIModule::onSendText(std::string text)
{

	// Send the text to the game if it is not being processed.
	Broodwar->sendText("%s", text.c_str());


	// Make sure to use %s and pass the text as a parameter,
	// otherwise you may run into problems when you use the %(percent) character!

}

void ExampleAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
	// Parse the received text
	Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void ExampleAIModule::onPlayerLeft(BWAPI::Player player)
{
	// Interact verbally with the other players in the game by
	// announcing that the other player has left.
	Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void ExampleAIModule::onNukeDetect(BWAPI::Position target)
{

	// Check if the target is a valid position
	if (target)
	{
		// if so, print the location of the nuclear strike target
		Broodwar << "Nuclear Launch Detected at " << target << std::endl;
	}
	else
	{
		// Otherwise, ask other players where the nuke is!
		Broodwar->sendText("Where's the nuke?");
	}

	// You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void ExampleAIModule::onUnitDiscover(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitEvade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitShow(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitHide(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitCreate(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void ExampleAIModule::onUnitDestroy(BWAPI::Unit unit)
{
}

void ExampleAIModule::onUnitMorph(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		// if we are in a replay, then we will print out the build order of the structures
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void ExampleAIModule::onUnitRenegade(BWAPI::Unit unit)
{
}

void ExampleAIModule::onSaveGame(std::string gameName)
{
	Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void ExampleAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
