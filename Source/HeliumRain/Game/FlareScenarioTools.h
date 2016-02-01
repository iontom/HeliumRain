#pragma once

#include "FlareScenarioTools.generated.h"

class UFlareCompany;
struct FFlarePlayerSave;

UCLASS()
class HELIUMRAIN_API UFlareScenarioTools : public UObject
{
public:

	GENERATED_UCLASS_BODY()


	/*----------------------------------------------------
		Public methods
	----------------------------------------------------*/

	void Init(UFlareCompany* Company, FFlarePlayerSave* Player);

	void GenerateEmptyScenario();

	void GenerateFighterScenario();

	void GenerateDebugScenario();

protected:

	/*----------------------------------------------------
		Helpers
	----------------------------------------------------*/

	/** Create asteroid, artefact and common things */
	void SetupWorld();

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/

	UFlareCompany*             PlayerCompany;

	FFlarePlayerSave*          PlayerData;

	AFlareGame*                                Game;
	UFlareWorld*                               World;

	UFlareSimulatedSector* Outpost;
	UFlareSimulatedSector* MinerHome;
	UFlareSimulatedSector* FrozenRealm;

	UFlareCompany* MiningSyndicate;
	UFlareCompany* HelixFoundries;
	UFlareCompany* Sunwatch;

	FFlareResourceDescription* Water;
	FFlareResourceDescription* Food;
	FFlareResourceDescription* Fuel;
	FFlareResourceDescription* Plastics;
	FFlareResourceDescription* Hydrogen;
	FFlareResourceDescription* Helium;
	FFlareResourceDescription* Silica;
	FFlareResourceDescription* Steel;
	FFlareResourceDescription* Tools;
	FFlareResourceDescription* Tech;


public:
	/*----------------------------------------------------
		Getter
	----------------------------------------------------*/

	/*AFlarePlayerController* GetPC() const;

	AFlareGame* GetGame() const;

	UFlareWorld* GetGameWorld() const;*/
};