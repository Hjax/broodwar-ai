#include "ExampleAIModule.h"
#include <iostream>

using namespace BWAPI;
using namespace Filter;

void ExampleAIModule::onStart()
{
	// Hello World!
	Broodwar->sendText("Hello world!");

	// init some variables
	Barracks_count = 0;
	supply_inprogress = 0;

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
	bool done = false;
	Broodwar->drawTextScreen(200, 0, "FPS: %d", Broodwar->getFPS());
	Broodwar->drawTextScreen(200, 40, "Barracks: %d", Barracks_count);
	Broodwar->drawTextScreen(200, 40, "Supply: %d", supply_inprogress);
	Broodwar->drawTextScreen(200, 20, "Average FPS: %f", Broodwar->getAverageFPS());
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
				static int lastChecked = 0;
				if ((Barracks_count < 3 || free_mins >= 500) &&
					(free_mins >= UnitTypes::Terran_Barracks.mineralPrice()) &&
					lastChecked + 400 < Broodwar->getFrameCount())
				{
					lastChecked = Broodwar->getFrameCount();
					TilePosition buildPosition = Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Barracks, u->getTilePosition());
					u->build(UnitTypes::Terran_Barracks, buildPosition);
					free_mins -= 150;
					Barracks_count += 1;
					done = true;
				}
				if (u->isIdle()){
					if (u->isCarryingGas() || u->isCarryingMinerals())
					{
						u->returnCargo();
					}
					else if (!u->getPowerUp()){
						if (!u->gather(u->getClosestUnit(IsMineralField || IsRefinery)))
						{
							Broodwar << Broodwar->getLastError() << std::endl;
						}
					}
				} // closure: has no powerup
			} // closure: if idle

		}
		if (u->isIdle() && free_mins >= 50 && !u->train(u->getType().getRace().getWorker()))
		{
			Position pos = u->getPosition(); // draw errors
			Error lastErr = Broodwar->getLastError();
			Broodwar->registerEvent([pos, lastErr](Game*){ Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
				nullptr,
				Broodwar->getLatencyFrames());

			UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
			if ((supply_inprogress * supplyProviderType.supplyProvided() + 10) - Broodwar->self()->supplyUsed() <= 2 &&
				Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0 &&
				free_mins >= 100)
			{
				Unit supplyBuilder = u->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
					(IsIdle || IsGatheringMinerals) &&
					IsOwned);
				if (supplyBuilder)
				{
					if (supplyProviderType.isBuilding())
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
							supply_inprogress += 1;
							free_mins -= 100;
						}
					}
				} // closure: supplyBuilder is valid
			} // closure: insufficient supply
		} // closure: failed to train idle unit

	}


	else if ((u->getType() == UnitTypes::Terran_Marine) && u->isIdle())
	{
		Unit closestEnemy = NULL;
		for (auto &e : Broodwar->enemy()->getUnits())
		{
			if ((closestEnemy == NULL) || (e->getDistance(u) < closestEnemy->getDistance(u)))
			{
				closestEnemy = e;
			}
		}
		u->attack(closestEnemy, false);
	}
	else if ((u->getType() == UnitTypes::Terran_Barracks) && u->isIdle() && free_mins >= 50){
		u->train(UnitTypes::Terran_Marine);
		free_mins -= 50;
	}
} // closure: unit iterator
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
