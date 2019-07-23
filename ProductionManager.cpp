#include "ProductionManager.h"

using namespace BWAPI;

ProductionManager::ProductionManager()
{
}


ProductionManager::~ProductionManager()
{
}

void ProductionManager::initialize()
{
	// INITIALIZE BUILD ORDER 
	buildOrder = StrictBuildOrder();
	// We will use the 8 rax build order (https://liquipedia.net/starcraft/Bunker_Rush_(vs._Zerg)) 
	buildOrder.AddItem(MetaType(BWAPI::UnitTypes::Terran_Barracks), 8);
	buildOrder.AddItem(MetaType(BWAPI::UnitTypes::Terran_Supply_Depot), 9);
	// SCOUT at 10
	// Begin marine construction as soon as barracks finish
	buildOrder.AddItem(MetaType(BWAPI::UnitTypes::Terran_Supply_Depot), 15);
	buildOrderActive = true;

	saving = false; 
	saveList = std::vector<MetaType>();

	passiveBuildUnit = BWAPI::UnitTypes::Terran_SCV;
	currentLowBound = 0;
}
//void setBuildOrder();

void ProductionManager::update()
{
	// Sends idle workers to mine
	// if our worker is idle
	for (auto &u : allWorkers)
	{
		if ((*u)->isIdle())
		{
			// Order workers carrying a resource to return them to the center,
			// otherwise find a mineral patch to harvest.
			if ((*u)->isCarryingGas() || (*u)->isCarryingMinerals())
			{
				(*u)->returnCargo();
			}
			else if (!(*u)->getPowerUp())  // The worker cannot harvest anything if it
			{                             // is carrying a powerup such as a flag
			  // Harvest from the nearest mineral patch or gas refinery
				if (!(*u)->gather((*u)->getClosestUnit(BWAPI::Filter::IsMineralField)))
				{
					// If the call fails, then print the last error message
					BWAPI::Broodwar << BWAPI::Broodwar->getLastError() << std::endl;
				}

			} // closure: has no powerup
		} // closure: if idle
	} // closure: for allWorkers

	// Check for supply-based timing 
	if (buildOrderActive)
	{
		MetaType next = buildOrder.getItemBySupply(BWAPI::Broodwar->self()->supplyUsed());

		if (next.type == MetaType::Default) // Assuming meta type default is default for not found 
		{
			// nothing happens 
		}
		else 
		{
			// If we're currently not saving for anything, then we should begin saving for this buildorder item 
			saving = true;
			saveList.emplace_back(next);

			// Set the item as built 
			buildOrder.setBuilt(next);
			BWAPI::Broodwar->sendText("Added structure to save queue");
		}

		// If we're saving and have enough minerals to build from the queue, then we build it ... 
		if (saving)
		{
			if (saveList.size() <= 0)
			{
				//something is wrong. 
				BWAPI::Broodwar->sendText("Save list is empty but we are currently saving. Something is wrong.");
				return;
			}
			if (BWAPI::Broodwar->self()->minerals() >= saveList[0].mineralPrice())
			{
				currentLowBound = saveList[0].mineralPrice(); // We shouldn't spend too much money while our worker is travelling

				BWAPI::Unit * u = getBuilder();

				// Build spawning pool => Find location and construct it
				BWAPI::TilePosition buildPosition = BWAPI::Broodwar->getBuildLocation(saveList[0].unitType, (*u)->getTilePosition());
				(*u)->build(BWAPI::UnitTypes::Zerg_Spawning_Pool, buildPosition);
				saving = false;
			} // closing: if we have enough minerals
		} // closing: if saving



	}

	// Passive building of SCVs or Marines
	if (currentLowBound < Broodwar->self()->minerals() - passiveBuildUnit.mineralPrice())
	{
		
		for (auto b : productionBuildings)
		{
			if (passiveBuildUnit == BWAPI::UnitTypes::Terran_SCV)
			{
				if ((*b)->getType().isResourceDepot())
				{
					// Code for building SCVs
					// COPIED FROM EXAMPLE
					if ((*b)->isIdle() && !(*b)->train((*b)->getType().getRace().getWorker()))
					{
						// If that fails, draw the error at the location so that you can visibly see what went wrong!
						// However, drawing the error once will only appear for a single frame
						// so create an event that keeps it on the screen for some frames
						Position pos = (*b)->getPosition();
						Error lastErr = Broodwar->getLastError();
						Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
							nullptr,    // condition
							Broodwar->getLatencyFrames());  // frames to run

	// Retrieve the supply provider type in the case that we have run out of supplies
						UnitType supplyProviderType = (*b)->getType().getRace().getSupplyProvider();
						static int lastChecked = 0;

						// If we are supply blocked and haven't tried constructing more recently
						if (lastErr == Errors::Insufficient_Supply &&
							lastChecked + 400 < Broodwar->getFrameCount() &&
							Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0)
						{
							lastChecked = Broodwar->getFrameCount();

							// Retrieve a unit that is capable of constructing the supply needed
							Unit supplyBuilder = *getBuilder();
							// If a unit was found
							if (supplyBuilder)
							{
								if (supplyProviderType.isBuilding())
								{
									TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
									if (targetBuildLocation)
									{
										// Register an event that draws the target build location
										Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
										{
											Broodwar->drawBoxMap(Position(targetBuildLocation),
												Position(targetBuildLocation + supplyProviderType.tileSize()),
												Colors::Blue);
										},
											nullptr,  // condition
											supplyProviderType.buildTime() + 100);  // frames to run

					// Order the builder to construct the supply structure
										supplyBuilder->build(supplyProviderType, targetBuildLocation);
									}
								}
								else
								{
									// Train the supply provider (Overlord) if the provider is not a structure
									supplyBuilder->train(supplyProviderType);
								}
							} // closure: supplyBuilder is valid
						} // closure: insufficient supply
					} // closure: failed to train idle unit
				}
			}

			if (passiveBuildUnit == BWAPI::UnitTypes::Terran_Marine)
			{
				if ((*b)->getType() == BWAPI::UnitTypes::Terran_Barracks)
				{
					if ((*b)->isIdle() && !(*b)->train(passiveBuildUnit))
					{
						// If that fails, draw the error at the location so that you can visibly see what went wrong!
						// However, drawing the error once will only appear for a single frame
						// so create an event that keeps it on the screen for some frames
						Position pos = (*b)->getPosition();
						Error lastErr = Broodwar->getLastError();
						Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); },   // action
							nullptr,    // condition
							Broodwar->getLatencyFrames());  // frames to run

	// Retrieve the supply provider type in the case that we have run out of supplies
						UnitType supplyProviderType = (*b)->getType().getRace().getSupplyProvider();
						static int lastChecked = 0;

						// If we are supply blocked and haven't tried constructing more recently
						if (lastErr == Errors::Insufficient_Supply &&
							lastChecked + 400 < Broodwar->getFrameCount() &&
							Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0)
						{
							lastChecked = Broodwar->getFrameCount();

							// Retrieve a unit that is capable of constructing the supply needed
							Unit supplyBuilder = *getBuilder();
							// If a unit was found
							if (supplyBuilder)
							{
								if (supplyProviderType.isBuilding())
								{
									TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
									if (targetBuildLocation)
									{
										// Register an event that draws the target build location
										Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
										{
											Broodwar->drawBoxMap(Position(targetBuildLocation),
												Position(targetBuildLocation + supplyProviderType.tileSize()),
												Colors::Blue);
										},
											nullptr,  // condition
											supplyProviderType.buildTime() + 100);  // frames to run

					// Order the builder to construct the supply structure
										supplyBuilder->build(supplyProviderType, targetBuildLocation);
									}
								}
								else
								{
									// Train the supply provider (Overlord) if the provider is not a structure
									supplyBuilder->train(supplyProviderType);
								}
							} // closure: supplyBuilder is valid
						} // closure: insufficient supply
					} // closure: failed to train idle unit
				}
			}
		}

	}

	// THIS IS PRETTY HACKY CODE NGL 
	if (BWAPI::Broodwar->self()->minerals() < currentLowBound)
		currentLowBound = 0; // we spent the money presumably on the building 
}

void ProductionManager::addBuilding(BWAPI::Unit* building)
{
	productionBuildings.emplace_back(building);
}
void ProductionManager::removeBuilding(BWAPI::Unit* building)
{
	for (auto it = begin(productionBuildings); it != end(productionBuildings); ++it) {
		if (building == *it)
		{
			productionBuildings.erase(it);
			break;
		}
	}
}

void ProductionManager::addWorker(BWAPI::Unit* worker)
{
	allWorkers.emplace_back(worker);
}
void ProductionManager::removeWorker(BWAPI::Unit* worker)
{
	for (auto it = begin(allWorkers); it != end(allWorkers); ++it) {
		if (worker == *it)
		{
			allWorkers.erase(it);
			break;
		}
	}
}

void ProductionManager::deactivateBuildOrder()
{
	buildOrderActive = false;
}

void ProductionManager::beginSaving()
{
	saving = true;
}
void ProductionManager::beginSpending()
{
	saving = false;
}

void ProductionManager::changePassiveProduction(BWAPI::UnitType u)
{
	passiveBuildUnit = u;
}

BWAPI::Unit * ProductionManager::getBuilder()
{
	for (auto &it : allWorkers)
	{
		if ((*it)->isIdle() || (*it)->isGatheringMinerals())
			return it;
	}
	return NULL; 
}