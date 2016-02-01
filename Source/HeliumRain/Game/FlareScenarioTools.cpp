
#include "../Flare.h"
#include "../Game/FlareWorld.h"
#include "../Game/FlareGame.h"
#include "../Game/FlareSimulatedSector.h"
#include "../Spacecrafts/FlareSimulatedSpacecraft.h"

#include "FlareScenarioTools.h"


#define LOCTEXT_NAMESPACE "FlareScenarioToolsInfo"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareScenarioTools::UFlareScenarioTools(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/*----------------------------------------------------
	Public methods
----------------------------------------------------*/

void UFlareScenarioTools::Init(UFlareCompany* Company, FFlarePlayerSave* Player)
{
	Game = Cast<AFlareGame>(GetOuter());
	World = Game->GetGameWorld();
	PlayerCompany = Company;
	PlayerData = Player;

	Outpost = World->FindSector("outpost");
	MinerHome = World->FindSector("miners-home");
	FrozenRealm = World->FindSector("frozen-realm");

	MiningSyndicate = World->FindCompanyByShortName("MSY");
	HelixFoundries = World->FindCompanyByShortName("HFR");
	Sunwatch = World->FindCompanyByShortName("SUN");

	Water = Game->GetResourceCatalog()->Get("h2o");
	Food = Game->GetResourceCatalog()->Get("food");
	Fuel = Game->GetResourceCatalog()->Get("fuel");
	Plastics = Game->GetResourceCatalog()->Get("plastics");
	Hydrogen = Game->GetResourceCatalog()->Get("h2");
	Helium = Game->GetResourceCatalog()->Get("he3");
	Silica = Game->GetResourceCatalog()->Get("sio2");
	Steel= Game->GetResourceCatalog()->Get("steel");
	Tools= Game->GetResourceCatalog()->Get("tools");
	Tech= Game->GetResourceCatalog()->Get("tech");


}

void UFlareScenarioTools::GenerateEmptyScenario()
{

}

void UFlareScenarioTools::GenerateFighterScenario()
{
	SetupWorld();

	// Create player ship
	FLOG("UFlareScenarioTools::GenerateFighterScenario create initial ship");
	UFlareSimulatedSpacecraft* InitialShip = World->FindSector("first-light")->CreateShip("ship-ghoul", PlayerCompany, FVector::ZeroVector);
	PlayerData->LastFlownShipIdentifier = InitialShip->GetImmatriculation();
	PlayerData->SelectedFleetIdentifier = InitialShip->GetCurrentFleet()->GetIdentifier();
	PlayerCompany->DiscoverSector(Outpost);
	PlayerCompany->DiscoverSector(MinerHome);

	MiningSyndicate->GiveMoney(100000);
	HelixFoundries->GiveMoney(100000);
	Sunwatch->GiveMoney(100000);


	// Initial setup: miner home
	MinerHome->CreateStation("station-mine", MiningSyndicate, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[0].Identifier);
	MinerHome->CreateStation("station-mine", MiningSyndicate, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[1].Identifier);
	MinerHome->CreateStation("station-mine", MiningSyndicate, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[2].Identifier);
	MinerHome->CreateStation("station-mine", MiningSyndicate, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[3].Identifier);
	MinerHome->CreateStation("station-mine", MiningSyndicate, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[4].Identifier);
	MinerHome->CreateStation("station-mine", MiningSyndicate, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[5].Identifier);
	// Todo remove
	MinerHome->CreateStation("station-solar-plant", MiningSyndicate, FVector::ZeroVector)->GiveResources(Water, 100);


	for(int Index = 0; Index < 5; Index ++)
	{
		MinerHome->CreateStation("station-solar-plant", Sunwatch, FVector::ZeroVector)->GiveResources(Water, 100);
	}

	for(int Index = 0; Index < 5; Index ++)
	{
		Outpost->CreateShip("ship-omen", Sunwatch, FVector::ZeroVector)->AssignToSector(true);
	}

	// Initial setup: Outpost
	Outpost->CreateStation("station-steelworks", HelixFoundries, FVector::ZeroVector);
	Outpost->CreateStation("station-steelworks", HelixFoundries, FVector::ZeroVector);
	Outpost->CreateStation("station-steelworks", HelixFoundries, FVector::ZeroVector);
	Outpost->CreateStation("station-tool-factory", HelixFoundries, FVector::ZeroVector);
	Outpost->CreateStation("station-tool-factory", HelixFoundries, FVector::ZeroVector);

	// Todo remove
	Outpost->CreateStation("station-solar-plant", HelixFoundries, FVector::ZeroVector)->GiveResources(Water, 100);

	for(int Index = 0; Index < 6; Index ++)
	{
		Outpost->CreateStation("station-solar-plant", Sunwatch, FVector::ZeroVector)->GiveResources(Water, 100);
	}

	for(int Index = 0; Index < 5; Index ++)
	{
		Outpost->CreateShip("ship-omen", Sunwatch, FVector::ZeroVector)->AssignToSector(true);
	}
}

void UFlareScenarioTools::GenerateDebugScenario()
{
	SetupWorld();

	// Create player ship
	FLOG("UFlareScenarioTools::GenerateDebugScenario create initial ship");
	UFlareSimulatedSpacecraft* InitialShip = Outpost->CreateShip("ship-solen", PlayerCompany, FVector::ZeroVector);
	PlayerData->LastFlownShipIdentifier = InitialShip->GetImmatriculation();
	PlayerData->SelectedFleetIdentifier = InitialShip->GetCurrentFleet()->GetIdentifier();




	// Initial setup : "Outpost"
	for(int Index = 0; Index < 5; Index ++)
	{
		Outpost->CreateShip("ship-omen", PlayerCompany, FVector::ZeroVector)->AssignToSector(true);
	}

	for(int Index = 0; Index < 3; Index ++)
	{
		Outpost->CreateStation("station-solar-plant", PlayerCompany, FVector::ZeroVector)->GetCargoBay()->GiveResources(Water, 100);
	}

	Outpost->CreateStation("station-hub", PlayerCompany, FVector::ZeroVector);

	Outpost->CreateStation("station-habitation", PlayerCompany, FVector::ZeroVector)->GetCargoBay()->GiveResources(Food, 30);
	Outpost->CreateStation("station-farm", PlayerCompany, FVector::ZeroVector);
	UFlareSimulatedSpacecraft* Refinery = Outpost->CreateStation("station-refinery", PlayerCompany, FVector::ZeroVector);
	Refinery->GetFactories()[0]->SetOutputLimit(Plastics, 1);
	Refinery->GetCargoBay()->GiveResources(Fuel, 50);

	UFlareSimulatedSpacecraft* PompingStation = Outpost->CreateStation("station-pumping", PlayerCompany, FVector::ZeroVector);
	PompingStation->GetFactories()[0]->SetOutputLimit(Hydrogen, 1);
	PompingStation->GetFactories()[0]->SetOutputLimit(Helium, 1);
	PompingStation->GetCargoBay()->GiveResources(Fuel, 50);

	Outpost->GetPeople()->GiveBirth(1000);
	Outpost->GetPeople()->SetHappiness(1);

	// Initial setup : "Miner's home"
	for(int Index = 0; Index < 5; Index ++)
	{
		MinerHome->CreateShip("ship-omen", PlayerCompany, FVector::ZeroVector)->AssignToSector(true);
	}

	MinerHome->CreateStation("station-hub", PlayerCompany, FVector::ZeroVector);

	for(int Index = 0; Index < 2; Index ++)
	{
		MinerHome->CreateStation("station-solar-plant", PlayerCompany, FVector::ZeroVector)->GetCargoBay()->GiveResources(Water, 100);
	}

	UFlareSimulatedSpacecraft* Tokamak = MinerHome->CreateStation("station-tokamak", PlayerCompany, FVector::ZeroVector);
	Tokamak->GetCargoBay()->GiveResources(Water, 200);
	UFlareSimulatedSpacecraft* Steelworks = MinerHome->CreateStation("station-steelworks", PlayerCompany, FVector::ZeroVector);
	Steelworks->GetFactories()[0]->SetOutputLimit(Steel, 1);

	UFlareSimulatedSpacecraft* Mine = MinerHome->CreateStation("station-ice-mine", PlayerCompany, FVector::ZeroVector, FRotator::ZeroRotator, MinerHome->Save()->AsteroidData[0].Identifier);
	Mine->GetFactories()[0]->SetOutputLimit(Silica, 1);

	UFlareSimulatedSpacecraft* ToolFactory = MinerHome->CreateStation("station-tool-factory", PlayerCompany, FVector::ZeroVector);
	ToolFactory->GetFactories()[0]->SetOutputLimit(Tools, 1);

	UFlareSimulatedSpacecraft* Foundry = MinerHome->CreateStation("station-foundry", PlayerCompany, FVector::ZeroVector);
	Foundry->GetFactories()[0]->SetOutputLimit(Tech, 1);

	UFlareTradeRoute* OutpostToMinerHome =  PlayerCompany->CreateTradeRoute(FText::FromString(TEXT("Trade route 1")));
	OutpostToMinerHome->AddSector(Outpost);
	OutpostToMinerHome->AddSector(MinerHome);

	OutpostToMinerHome->SetSectorLoadOrder(0, Helium, 0);
	OutpostToMinerHome->SetSectorLoadOrder(0, Hydrogen, 0);
	OutpostToMinerHome->SetSectorLoadOrder(0, Plastics, 0);
	OutpostToMinerHome->SetSectorUnloadOrder(0, Water, 0);
	OutpostToMinerHome->SetSectorUnloadOrder(0, Tools, 0);
	OutpostToMinerHome->SetSectorUnloadOrder(0, Tech, 0);

	OutpostToMinerHome->SetSectorLoadOrder(1, Tools, 0);
	OutpostToMinerHome->SetSectorLoadOrder(1, Tech, 0);
	OutpostToMinerHome->SetSectorLoadOrder(1, Water, 250);
	OutpostToMinerHome->SetSectorUnloadOrder(1, Helium, 0);
	OutpostToMinerHome->SetSectorUnloadOrder(1, Hydrogen, 0);
	OutpostToMinerHome->SetSectorUnloadOrder(1, Plastics, 0);



	UFlareFleet* TradeFleet1 = PlayerCompany->CreateFleet(FText::FromString(TEXT("Trade fleet 1")), MinerHome);
	TradeFleet1->AddShip(MinerHome->CreateShip("ship-atlas", PlayerCompany, FVector::ZeroVector));

	OutpostToMinerHome->AddFleet(TradeFleet1);

	World->FindSector("frozen-realm")->CreateShip("ship-omen", PlayerCompany, FVector::ZeroVector);

	// Discover known sectors
	if (!PlayerData->QuestData.PlayTutorial)
	{
		for (int SectorIndex = 0; SectorIndex < World->GetSectors().Num(); SectorIndex++)
		{
			PlayerCompany->DiscoverSector(World->GetSectors()[SectorIndex]);
		}
	}
}

/*----------------------------------------------------
	Helpers
----------------------------------------------------*/
void UFlareScenarioTools::SetupWorld()
{
	// Create asteroids at "Outpost"
	for (int32 Index = 0; Index < 20; Index++)
	{
		FString AsteroidName = FString("asteroid") + FString::FromInt(Index);
		Outpost->CreateAsteroid(FMath::RandRange(0, 5), FName(*AsteroidName) , 200000 * FMath::VRand());
	}


	// Create asteroids at "Miner's home"
	for (int32 Index = 0; Index < 40; Index++)
	{
		FString AsteroidName = FString("asteroid") + FString::FromInt(Index);
		MinerHome->CreateAsteroid(FMath::RandRange(0, 5), FName(*AsteroidName) , 200000 * FMath::VRand());
	}

	// Create asteroids at "Frozen Realm"
	for (int32 Index = 0; Index < 50; Index++)
	{
		FString AsteroidName = FString("asteroid") + FString::FromInt(Index);
		World->FindSector("frozen-realm")->CreateAsteroid(FMath::RandRange(0, 5), FName(*AsteroidName), 200000 * FMath::VRand());
	}
}



#undef LOCTEXT_NAMESPACE