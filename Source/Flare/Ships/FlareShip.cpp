
#include "../Flare.h"

#include "FlareShip.h"
#include "FlareAirframe.h"
#include "FlareOrbitalEngine.h"
#include "FlareRCS.h"
#include "FlareWeapon.h"

#include "Particles/ParticleSystemComponent.h"

#include "../Stations/FlareStation.h"
#include "../Player/FlarePlayerController.h"
#include "../Game/FlareGame.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

AFlareShip::AFlareShip(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	, AngularDeadAngle(0.5)
	, AngularInputDeadRatio(0.025)
	, LinearDeadDistance(10)
	, NegligibleSpeedRatio(0.05)
	, Status(EFlareShipStatus::SS_Manual)
	, FakeThrust(false)
{	
	// Create static mesh component
	Airframe = PCIP.CreateDefaultSubobject<UFlareAirframe>(this, TEXT("Airframe"));
	Airframe->SetSimulatePhysics(true);
	RootComponent = Airframe;

	// Camera settings
	CameraContainerYaw->AttachTo(Airframe);
	CameraMaxPitch = 80;
	CameraPanSpeed = 2;

	// Dock info
	ShipData.DockedTo = NAME_None;
	ShipData.DockedAt = -1;
}


/*----------------------------------------------------
	Gameplay events
----------------------------------------------------*/

void AFlareShip::BeginPlay()
{
	Super::BeginPlay();
	AngularVelocity = NULL_QUAT;
	TArray<UActorComponent*> ActorComponents;
	GetComponents(ActorComponents);

	// Check which moves are allowed
	for (TArray<UActorComponent*>::TIterator ComponentIt(ActorComponents); ComponentIt; ++ComponentIt)
	{
		// RCS
		UFlareRCS* RCS = Cast<UFlareRCS>(*ComponentIt);
		if (RCS)
		{
			if (RCS->CanMoveVertical())
			{
				CanMoveVertical = true;
			}
		}

		// If this is a weapon, reinitialize it directly so that it updates its properties
		UFlareWeapon* Weapon = Cast<UFlareWeapon>(*ComponentIt);
		if (Weapon && WeaponList.Num() < WeaponDescriptionList.Num())
		{
			ReloadPart(Weapon, WeaponDescriptionList[WeaponList.Num()]);
			WeaponList.Add(Weapon);
		}
	}
}

void AFlareShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UE_LOG(LogTemp, Warning, TEXT("Tick"));
	// Attitude control
	if (Airframe && !FakeThrust)
	{
		// Manual pilot
		if (IsManualPilot())
		{
			UpdateLinearAttitudeManual(DeltaSeconds);
			UpdateAngularAttitudeManual(DeltaSeconds);
		}

		// Autopilot
		else if (IsAutoPilot())
		{
			FFlareShipCommandData Temp;
			if (CommandData.Peek(Temp))
			{
				if (Temp.Type == EFlareCommandDataType::CDT_Location)
				{
					UpdateLinearAttitudeAuto(DeltaSeconds);
				}
				else if (Temp.Type == EFlareCommandDataType::CDT_BrakeLocation)
				{
					UpdateLinearBraking(DeltaSeconds);
				}
				else if (Temp.Type == EFlareCommandDataType::CDT_Rotation)
				{
					UpdateAngularAttitudeAuto(DeltaSeconds);
				}
				else if (Temp.Type == EFlareCommandDataType::CDT_BrakeRotation)
				{
					UpdateAngularBraking(DeltaSeconds);
				}
				else if (Temp.Type == EFlareCommandDataType::CDT_Dock)
				{
					ConfirmDock(Cast<IFlareStationInterface>(Temp.ActionTarget), Temp.ActionTargetParam);
				}
			}
		}
				// Physics
		if (!IsDocked())
		{
		  
			COM = Airframe->GetBodyInstance()->GetCOMPosition();

			UE_LOG(LogTemp, Warning, TEXT("Not docked"));

			LowLevelAutoPilotSubTick(DeltaSeconds);

			// Tick Modules
			TArray<UActorComponent*> Modules = GetComponentsByClass(UFlareShipModule::StaticClass());
			for (int32 i = 0; i < Modules.Num(); i++) {
				UFlareShipModule* Module = Cast<UFlareShipModule>(Modules[i]);
				Module->TickModule(DeltaSeconds);
			}

			//UpdateLinearPhysics(DeltaSeconds);

			PhysicSubTick(DeltaSeconds);
		}
	}
}

void AFlareShip::ReceiveHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::ReceiveHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
}

void AFlareShip::Destroyed()
{
	if (Company)
	{
		Company->Unregister(this);
	}
}


/*----------------------------------------------------
	Player interface
----------------------------------------------------*/

void AFlareShip::SetExternalCamera(bool NewState)
{
	// Stop firing
	if (NewState)
	{
		StopFire();
	}

	// Reset rotations
	ExternalCamera = NewState;
	SetCameraPitch(0);
	SetCameraYaw(0);

	// Reset controls
	ManualLinearVelocity = FVector::ZeroVector;
	ManualAngularVelocity = FVector::ZeroVector;

	// Put the camera at the right spot
	if (ExternalCamera)
	{
		SetCameraDistance(CameraMaxDistance * GetMeshScale());
	}
	else
	{
		FVector CameraDistance = WorldToLocal(Airframe->GetSocketLocation(FName("Camera")) - GetActorLocation());
		SetCameraDistance(-CameraDistance.Size());
	}
}


/*----------------------------------------------------
	Ship interface
----------------------------------------------------*/

void AFlareShip::Load(const FFlareShipSave& Data)
{
	// Update local data
	ShipData = Data;
	ShipData.Name = FName(*GetName());

	// Look for parent company
	SetOwnerCompany(GetGame()->FindCompany(Data.CompanyIdentifier));

	// Load ship description
	UFlareShipPartsCatalog* Catalog = GetGame()->GetShipPartsCatalog();
	FFlareShipDescription* Desc = GetGame()->GetShipCatalog()->Get(Data.Identifier);
	SetShipDescription(Desc);

	// Load component descriptions 
	SetOrbitalEngineDescription(Catalog->Get(Data.OrbitalEngineIdentifier));
	SetRCSDescription(Catalog->Get(Data.RCSIdentifier));

	// Load weapon descriptions
	WeaponDescriptionList.Empty();
	for (int32 i = 0; i < Data.WeaponIdentifiers.Num(); i++)
	{
		WeaponDescriptionList.Add(Catalog->Get(Data.WeaponIdentifiers[i]));
	}

	// Customization
	UpdateCustomization();

	// Re-dock if we were docked
	if (ShipData.DockedTo != NAME_None)
	{
		FLOGV("AFlareShip::Load : Looking for station '%s'", *ShipData.DockedTo.ToString());
		for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
		{
			AFlareStation* Station = Cast<AFlareStation>(*ActorItr);
			if (Station && *Station->GetName() == ShipData.DockedTo)
			{
				FLOGV("AFlareShip::Load : Found dock station '%s'", *Station->GetName());
				ConfirmDock(Station, ShipData.DockedAt);
				break;
			}
		}
	}
}

FFlareShipSave* AFlareShip::Save()
{
	// Physical data
	ShipData.Location = GetActorLocation();
	ShipData.Rotation = GetActorRotation();

	// Engines
	ShipData.OrbitalEngineIdentifier = OrbitalEngineDescription->Identifier;
	ShipData.RCSIdentifier = RCSDescription->Identifier;

	// Weapons
	ShipData.WeaponIdentifiers.Empty();
	for (int32 i = 0; i < WeaponDescriptionList.Num(); i++)
	{
		ShipData.WeaponIdentifiers.Add(WeaponDescriptionList[i]->Identifier);
	}

	return &ShipData;
}

void AFlareShip::SetOwnerCompany(UFlareCompany* NewCompany)
{
	SetCompany(NewCompany);
	ShipData.CompanyIdentifier = NewCompany->GetIdentifier();
	Airframe->Initialize(NULL, Company, this);
	NewCompany->Register(this);
}

UFlareCompany* AFlareShip::GetCompany()
{
	return Company;
}

bool AFlareShip::NavigateTo(FVector TargetLocation)
{
	// Pathfinding data
	TArray<FVector> Path;
	FVector Unused;
	FVector ShipExtent;
	FVector Temp = GetActorLocation();

	// Prepare data
	FLOG("AFlareShip::NavigateTo");
	GetActorBounds(true, Unused, ShipExtent);
	UpdateColliders();

	// Compute path
	if (ComputePath(Path, PathColliders, Temp, TargetLocation, ShipExtent.Size()))
	{
		FLOGV("AFlareShip::NavigateTo : generating path (%d stops)", Path.Num());

		// Generate commands for travel
		for (int32 i = 0; i < Path.Num(); i++)
		{
			PushCommandRotation((Path[i] - Temp).Rotation().Quaternion());
			PushCommandLocation(Path[i]);
			Temp = Path[i];
		}

		// Move toward objective for pre-final approach
		PushCommandRotation((TargetLocation - Temp).Rotation().Quaternion());
		PushCommandLocation(TargetLocation);
		return true;
	}

	// Failed
	FLOG("AFlareShip::NavigateTo failed : no path found");
	return false;
}

bool AFlareShip::IsManualPilot()
{
	return (Status == EFlareShipStatus::SS_Manual || Status == EFlareShipStatus::SS_Gliding);
}

bool AFlareShip::IsGliding()
{
	return (Status == EFlareShipStatus::SS_Gliding);
}

bool AFlareShip::IsAutoPilot()
{
	return (Status == EFlareShipStatus::SS_AutoPilot);
}

bool AFlareShip::IsDocked()
{
	return (Status == EFlareShipStatus::SS_Docked);
}


/*----------------------------------------------------
	Docking
----------------------------------------------------*/

bool AFlareShip::DockAt(IFlareStationInterface* TargetStation)
{
	FLOG("AFlareShip::DockAt");
	FFlareDockingInfo DockingInfo = TargetStation->RequestDock(this);

	// Try to dock
	if (DockingInfo.Granted)
	{
		FVector ShipDockOffset = GetDockLocation();
		DockingInfo.EndPoint += DockingInfo.Rotation.RotateVector(ShipDockOffset);
		DockingInfo.StartPoint += 5000 * DockingInfo.Rotation.RotateVector(FVector(1, 0, 0));

		// Dock
		if (NavigateTo(DockingInfo.StartPoint))
		{
			PushCommandRotation((DockingInfo.EndPoint - DockingInfo.StartPoint).Rotation().Quaternion());
			PushCommandLocation(DockingInfo.EndPoint);
			PushCommandDock(DockingInfo);

			FLOG("AFlareShip::DockAt : navigation sent");
			return true;
		}
	}

	// Failed
	FLOG("AFlareShip::DockAt failed");
	return false;
}

void AFlareShip::ConfirmDock(IFlareStationInterface* DockStation, int32 DockId)
{
	FLOG("AFlareShip::ConfirmDock");
	ClearCurrentCommand();

	// Signal the PC
	AFlarePlayerController* PC = GetPC();
	if (PC && !ExternalCamera)
	{
		PC->SetExternalCamera(true);
	}

	// Set as docked
	DockStation->Dock(this, DockId);
	Status = EFlareShipStatus::SS_Docked;
	ShipData.DockedTo = *DockStation->_getUObject()->GetName();
	ShipData.DockedAt = DockId;
}

bool AFlareShip::Undock()
{
	FLOG("AFlareShip::Undock");
	FFlareShipCommandData Head;

	// Try undocking
	if (IsDocked())
	{
		// Evacuate
		ClearCurrentCommand();
		GetDockStation()->ReleaseDock(this, ShipData.DockedAt);
		PushCommandLocation(RootComponent->GetComponentTransform().TransformPositionNoScale(5000 * FVector(-1, 0, 0)));

		// Update data
		Status = EFlareShipStatus::SS_AutoPilot;
		ShipData.DockedTo = NAME_None;
		ShipData.DockedAt = -1;

		FLOG("AFlareShip::Undock successful");
		return true;
	}

	// Failed
	FLOG("AFlareShip::Undock failed");
	return false;
}

IFlareStationInterface* AFlareShip::GetDockStation()
{
	if (IsDocked())
	{
		for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
		{
			AFlareStation* Station = Cast<AFlareStation>(*ActorItr);
			if (Station && *Station->GetName() == ShipData.DockedTo)
			{
				FLOGV("AFlareShip::GetDockStation : Found dock station '%s'", *Station->GetName());
				return Station;
			}
		}
	}
	return NULL;
}


/*----------------------------------------------------
	Navigation commands and helpers
----------------------------------------------------*/

void AFlareShip::PushCommandLinearBrake()
{
	FFlareShipCommandData Data;
	Data.Type = EFlareCommandDataType::CDT_Location;
	PushCommand(Data);
}

void AFlareShip::PushCommandAngularBrake()
{
	FFlareShipCommandData Data;
	Data.Type = EFlareCommandDataType::CDT_BrakeRotation;
	PushCommand(Data);
}

void AFlareShip::PushCommandLocation(const FVector& Location)
{
	FFlareShipCommandData Data;
	Data.Type = EFlareCommandDataType::CDT_Location;
	Data.LocationTarget = Location;
	PushCommand(Data);
}

void AFlareShip::PushCommandRotation(const FQuat& Rotation)
{
	FFlareShipCommandData Data;
	Data.Type = EFlareCommandDataType::CDT_Rotation;
	Data.RotationTarget = Rotation;
	PushCommand(Data);
}

void AFlareShip::PushCommandDock(const FFlareDockingInfo& DockingInfo)
{
	FFlareShipCommandData Data;
	Data.Type = EFlareCommandDataType::CDT_Dock;
	Data.ActionTarget = Cast<AFlareStation>(DockingInfo.Station);
	Data.ActionTargetParam = DockingInfo.DockId;
	PushCommand(Data);
}

void AFlareShip::PushCommand(const FFlareShipCommandData& Command)
{
	Status = EFlareShipStatus::SS_AutoPilot;
	CommandData.Enqueue(Command);

	FLOGV("Pushed command '%s'", *EFlareCommandDataType::ToString(Command.Type));
}

void AFlareShip::ClearCurrentCommand()
{
	FFlareShipCommandData Command;
	CommandData.Dequeue(Command);

	FLOGV("Cleared command '%s'", *EFlareCommandDataType::ToString(Command.Type));
}

FVector AFlareShip::GetDockLocation()
{
	FVector WorldLocation = RootComponent->GetSocketLocation(FName("Dock"));
	return RootComponent->GetComponentTransform().InverseTransformPosition(WorldLocation);
}

bool AFlareShip::ComputePath(TArray<FVector>& Path, TArray<AActor*>& PossibleColliders, FVector OriginLocation, FVector TargetLocation, float ShipSize)
{
	// Travel information
	float TravelLength;
	FVector TravelDirection;
	FVector Travel = TargetLocation - OriginLocation;
	Travel.ToDirectionAndLength(TravelDirection, TravelLength);

	for (int32 i = 0; i < PossibleColliders.Num(); i++)
	{
		// Get collider info
		FVector ColliderLocation;
		FVector ColliderExtent;
		PossibleColliders[i]->GetActorBounds(true, ColliderLocation, ColliderExtent);
		float ColliderSize = ShipSize + ColliderExtent.Size();

		// Colliding : split the travel
		if (FMath::LineSphereIntersection(OriginLocation, TravelDirection, TravelLength, ColliderLocation, ColliderSize))
		{
			DrawDebugSphere(GetWorld(), ColliderLocation, ColliderSize, 12, FColor::Blue, true);

			// Get an orthogonal plane
			FPlane TravelOrthoPlane = FPlane(ColliderLocation, TargetLocation - ColliderLocation);
			FVector IntersectedLocation = FMath::LinePlaneIntersection(OriginLocation, TargetLocation, TravelOrthoPlane);

			// Relocate intersection inside the sphere
			FVector Intersector = IntersectedLocation - ColliderLocation;
			Intersector.Normalize();
			IntersectedLocation = ColliderLocation + Intersector * ColliderSize;

			// Collisions
			bool IsColliding = IsPointColliding(IntersectedLocation, PossibleColliders[i]);
			DrawDebugPoint(GetWorld(), IntersectedLocation, 8, IsColliding ? FColor::Red : FColor::Green, true);

			// Dead end, go back
			if (IsColliding)
			{
				return false;
			}

			// Split travel
			else
			{
				Path.Add(IntersectedLocation);
				PossibleColliders.RemoveAt(i, 1);
				bool FirstPartOK = ComputePath(Path, PossibleColliders, OriginLocation, IntersectedLocation, ShipSize);
				bool SecondPartOK = ComputePath(Path, PossibleColliders, IntersectedLocation, TargetLocation, ShipSize);
				return FirstPartOK && SecondPartOK;
			}

		}
	}

	// No collision found
	return true;
}

void AFlareShip::UpdateColliders()
{
	PathColliders.Empty();
	for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		FVector Unused;
		FVector ColliderExtent;
		ActorItr->GetActorBounds(true, Unused, ColliderExtent);

		if (ColliderExtent.Size() < 100000 && ActorItr->IsRootComponentMovable())
		{
			PathColliders.Add(*ActorItr);
		}
	}
}

bool AFlareShip::IsPointColliding(FVector Candidate, AActor* Ignore)
{
	for (int32 i = 0; i < PathColliders.Num(); i++)
	{
		FVector ColliderLocation;
		FVector ColliderExtent;
		PathColliders[i]->GetActorBounds(true, ColliderLocation, ColliderExtent);

		if ((Candidate - ColliderLocation).Size() < ColliderExtent.Size() && PathColliders[i] != Ignore)
		{
			return true;
		}
	}

	return false;
}


/*----------------------------------------------------
	Attitude control : linear version
----------------------------------------------------*/

void AFlareShip::UpdateLinearAttitudeManual(float DeltaSeconds)
{
  
	// RCS impulse
	LinearTargetVelocity = ManualLinearVelocity;

	FLOGV("UpdateLinearAttitudeManual LinearTargetVelocity=%s", *LinearTargetVelocity.ToString());
	// Orbital impulse
	if (ManualOrbitalBoost)
	{
		FVector OrbitalPushDirection = GetActorQuat().RotateVector(FVector(1, 0, 0));
		// TODO remove and add in global engine system
		//Airframe->AddForce(LinearOrbitalThrust * OrbitalPushDirection);
	}
}

void AFlareShip::UpdateLinearAttitudeAuto(float DeltaSeconds)
{
	// Location data
	FFlareShipCommandData Data;
	CommandData.Peek(Data);
	FVector LocalCommand = WorldToLocal(Data.LocationTarget - GetActorLocation());

	// Check legit movements
	if (!CanMoveVertical)
	{
		LocalCommand.Z = 0;
	}
	float RemainingTravelDistance = LocalCommand.Size();
		
	// Under this angle we consider the variation negligible, and ensure null delta + null speed
	if (RemainingTravelDistance < LinearDeadDistance && LinearVelocity.Size() < NegligibleSpeedRatio * LinearMaxVelocity)
	{
		Airframe->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		SetActorLocation(Data.LocationTarget, true);
		LinearTargetVelocity = FVector::ZeroVector;
		ClearCurrentCommand();
	}

	// Brake if "close" to target and not going away from it
	else if (RemainingTravelDistance <= LinearStopDistance)
	{
		LinearTargetVelocity = FVector::ZeroVector;
	}

	// The rest of the time we try to follow the command
	else
	{
		LinearTargetVelocity = LocalCommand;
	}
}

void AFlareShip::UpdateLinearBraking(float DeltaSeconds)
{
	LinearTargetVelocity = FVector::ZeroVector;
	FVector LinearVelocity = WorldToLocal(Airframe->GetPhysicsLinearVelocity());

	// Null speed detection
	if (LinearVelocity.Size() < NegligibleSpeedRatio * LinearMaxVelocity)
	{
		Airframe->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		ClearCurrentCommand();
	}
}

/*----------------------------------------------------
	Attitude control : angular version
----------------------------------------------------*/

void AFlareShip::UpdateAngularAttitudeManual(float DeltaSeconds)
{
	AngularTargetVelocity = ManualAngularVelocity;
}

void AFlareShip::UpdateAngularAttitudeAuto(float DeltaSeconds)
{
	// Rotation data
	FFlareShipCommandData Data;
	CommandData.Peek(Data);
	FQuat LocalCommand = WorldToLocal(Data.RotationTarget);
	float RemainingTravelDistance = GetRotationAmount(LocalCommand, true);

	// Detect if we are going away from the target
	static float PreviousRemainingTravelDistance = 360;
	bool PilotError = (RemainingTravelDistance > PreviousRemainingTravelDistance);
	PreviousRemainingTravelDistance = RemainingTravelDistance;

	// Under this angle we consider the variation negligible, and ensure null delta + null speed
	if (RemainingTravelDistance < AngularDeadAngle && AngularStopDistance < AngularDeadAngle)
	{
		AngularVelocity = NULL_QUAT;
		AngularTargetVelocity = FVector::ZeroVector;
		PreviousRemainingTravelDistance = 360;
		AddActorLocalRotation(LocalCommand.Rotator(), true);
		ClearCurrentCommand();
	}

	// Brake if "close" to target or going away from it
	else if (RemainingTravelDistance <= AngularStopDistance || PilotError)
	{
		AngularTargetVelocity = FVector::ZeroVector;
	}

	// The rest of the time we try to follow the command
	else
	{
		//TODO Repair
		//AngularTargetVelocity = LocalCommand;
	}
}

void AFlareShip::UpdateAngularBraking(float DeltaSeconds)
{
	AngularTargetVelocity = FVector::ZeroVector;

	// Null speed detection
	if (GetRotationAmount(AngularVelocity) < NegligibleSpeedRatio * AngularMaxVelocity)
	{
		AngularTargetVelocity = FVector::ZeroVector;
		AngularVelocity = NULL_QUAT;
		ClearCurrentCommand();
	}
}

/*----------------------------------------------------
	Customization
----------------------------------------------------*/

void AFlareShip::SetShipDescription(FFlareShipDescription* Description)
{
	ShipDescription = Description;

	// Load data from the ship info
	if (Description)
	{
		LinearMaxVelocity = Description->LinearMaxVelocity;
		AngularMaxVelocity = Description->AngularMaxVelocity;
	}
}

void AFlareShip::SetOrbitalEngineDescription(FFlareShipModuleDescription* Description)
{
	OrbitalEngineDescription = Description;
	ReloadAllParts(UFlareOrbitalEngine::StaticClass(), Description);

	// Find the orbital thrust rating
	if (Description)
	{
		for (int32 i = 0; i < Description->Characteristics.Num(); i++)
		{
			const FFlarePartCharacteristic& Characteristic = Description->Characteristics[i];

			// Calculate the orbital engine linear thrust force in N (data value in kN)
			if (Characteristic.CharacteristicType == EFlarePartAttributeType::EnginePower)
			{
				LinearOrbitalThrust = 100 * 1000 * Characteristic.CharacteristicValue;
			}
		}
	}
}

void AFlareShip::SetRCSDescription(FFlareShipModuleDescription* Description)
{
	RCSDescription = Description;
	ReloadAllParts(UFlareRCS::StaticClass(), Description);

	// Find the RCS turn and power rating, since RCSs themselves don't do anything
	if (Description)
	{
		for (int32 i = 0; i < Description->Characteristics.Num(); i++)
		{
			const FFlarePartCharacteristic& Characteristic = Description->Characteristics[i];

			// Calculate the angular acceleration rate from the ton weight (data value in �/s per 100T)
			if (Airframe && Characteristic.CharacteristicType == EFlarePartAttributeType::RCSAccelerationRating)
			{
				float Mass = Airframe->GetMass() / 100000;
				AngularAccelerationRate = Characteristic.CharacteristicValue / (60 * Mass);
			}

			// Calculate the RCS linear thrust force in N (data value in kN)
			else if (Characteristic.CharacteristicType == EFlarePartAttributeType::EnginePower)
			{
				LinearThrust = 100 * 1000 * Characteristic.CharacteristicValue;
			}
		}
	}
}

void AFlareShip::SetWeaponDescription(int32 Index, FFlareShipModuleDescription* Description)
{
	if (Index < WeaponList.Num())
	{
		WeaponDescriptionList[Index] = Description;
		ReloadPart(WeaponList[Index], Description);
	}
	else
	{
		FLOGV("AFlareShip::SetWeaponDescription : failed (no such index %d)", Index);
	}
}

void AFlareShip::StartPresentation()
{
	Airframe->SetSimulatePhysics(false);
	FakeThrust = true;
}

void AFlareShip::UpdateCustomization()
{
	Super::UpdateCustomization();

	Airframe->UpdateCustomization();
}


/*----------------------------------------------------
	Input
----------------------------------------------------*/

void AFlareShip::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	check(InputComponent);

	InputComponent->BindAxis("Thrust", this, &AFlareShip::ThrustInput);
	InputComponent->BindAxis("MoveVerticalInput", this, &AFlareShip::MoveVerticalInput);
	InputComponent->BindAxis("MoveHorizontalInput", this, &AFlareShip::MoveHorizontalInput);

	InputComponent->BindAxis("RollInput", this, &AFlareShip::RollInput);
	InputComponent->BindAxis("PitchInput", this, &AFlareShip::PitchInput);
	InputComponent->BindAxis("YawInput", this, &AFlareShip::YawInput);
	InputComponent->BindAxis("MouseInputY", this, &AFlareShip::PitchInput);
	InputComponent->BindAxis("MouseInputX", this, &AFlareShip::YawInput);

	InputComponent->BindAction("ZoomIn", EInputEvent::IE_Released, this, &AFlareShip::ZoomIn);
	InputComponent->BindAction("ZoomOut", EInputEvent::IE_Released, this, &AFlareShip::ZoomOut);

	InputComponent->BindAction("FaceForward", EInputEvent::IE_Released, this, &AFlareShip::FaceForward);
	InputComponent->BindAction("FaceBackward", EInputEvent::IE_Released, this, &AFlareShip::FaceBackward);
	InputComponent->BindAction("Boost", EInputEvent::IE_Pressed, this, &AFlareShip::BoostOn);
	InputComponent->BindAction("Boost", EInputEvent::IE_Released, this, &AFlareShip::BoostOff);
	InputComponent->BindAction("Glide", EInputEvent::IE_Released, this, &AFlareShip::ToggleGliding);

	InputComponent->BindAction("MouseLeft", EInputEvent::IE_Pressed, this, &AFlareShip::StartFire);
	InputComponent->BindAction("MouseLeft", EInputEvent::IE_Released, this, &AFlareShip::StopFire);
}

void AFlareShip::MousePositionInput(FVector2D Val)
{
	if (!ExternalCamera)
	{
		// Compensation curve = 1 + (input-1)/(1-AngularInputDeadRatio)
		Val.X = FMath::Clamp(1. + (FMath::Abs(Val.X) - 1. ) / (1. - AngularInputDeadRatio) , 0., 1.) * FMath::Sign(Val.X);
		Val.Y = FMath::Clamp(1. + (FMath::Abs(Val.Y) - 1. ) / (1. - AngularInputDeadRatio) , 0., 1.) * FMath::Sign(Val.Y);
		

		ManualAngularVelocity.Z = Val.X * AngularMaxVelocity;
		ManualAngularVelocity.Y = Val.Y * AngularMaxVelocity;
	}
}

void AFlareShip::ThrustInput(float Val)
{
	if (!ExternalCamera)
	{
		ManualLinearVelocity.X = Val * LinearMaxVelocity;
	}
}

void AFlareShip::MoveVerticalInput(float Val)
{
	if (!ExternalCamera && CanMoveVertical)
	{
		ManualLinearVelocity.Z = LinearMaxVelocity * Val;
	}
}

void AFlareShip::MoveHorizontalInput(float Val)
{
	if (!ExternalCamera)
	{
		ManualLinearVelocity.Y = LinearMaxVelocity * Val;
	}
}

void AFlareShip::RollInput(float Val)
{
	if (!ExternalCamera)
	{
		ManualAngularVelocity.X = - Val * AngularMaxVelocity;
	}
}

void AFlareShip::PitchInput(float Val)
{
	if (ExternalCamera)
	{
		FRotator CurrentRot = WorldToLocal(CameraContainerPitch->GetComponentRotation().Quaternion()).Rotator();
		SetCameraPitch(CurrentRot.Pitch + Val * CameraPanSpeed);
	}
}

void AFlareShip::YawInput(float Val)
{
	if (ExternalCamera)
	{
		FRotator CurrentRot = WorldToLocal(CameraContainerPitch->GetComponentRotation().Quaternion()).Rotator();
		SetCameraYaw(CurrentRot.Yaw + Val * CameraPanSpeed);
	}
}

void AFlareShip::ZoomIn()
{
	if (ExternalCamera)
	{
		StepCameraDistance(true);
	}
}

void AFlareShip::ZoomOut()
{
	if (ExternalCamera)
	{
		StepCameraDistance(false);
	}
}

void AFlareShip::FaceForward()
{
	if (IsManualPilot())
	{
		PushCommandRotation(Airframe->GetPhysicsLinearVelocity().Rotation().Quaternion());
	}
}

void AFlareShip::FaceBackward()
{
	if (IsManualPilot())
	{
		PushCommandRotation((-Airframe->GetPhysicsLinearVelocity()).Rotation().Quaternion());
	}
}

void AFlareShip::BoostOn()
{
	if (IsManualPilot())
	{
		ManualOrbitalBoost = true;
	}
}

void AFlareShip::BoostOff()
{
	ManualOrbitalBoost = false;
}


void AFlareShip::ToggleGliding()
{
	if (IsGliding())
	{
		Status = EFlareShipStatus::SS_Manual;
	}
	else
	{
		Status = EFlareShipStatus::SS_Gliding;
	}
}

void AFlareShip::StartFire()
{
	if (!ExternalCamera)
	{
		for (int32 i = 0; i < WeaponList.Num(); i++)
		{
			WeaponList[i]->StartFire();
		}
	}
}

void AFlareShip::StopFire()
{
	if (!ExternalCamera)
	{
		for (int32 i = 0; i < WeaponList.Num(); i++)
		{
			WeaponList[i]->StopFire();
		}
	}
}

/*----------------------------------------------------
	Physics
----------------------------------------------------*/

void AFlareShip::LowLevelAutoPilotSubTick(float DeltaSeconds)
{

  
	TArray<UActorComponent*> Engines = GetComponentsByClass(UFlareEngine::StaticClass());

	UE_LOG(LogTemp, Warning, TEXT("LowLevelAutoPilotSubTick num engine=%d"), Engines.Num());

	TArray<float*> EngineCommands;

	FVector LinearTarget = Airframe->GetComponentToWorld().GetRotation().RotateVector(LinearTargetVelocity);
	FVector AngularTarget = Airframe->GetComponentToWorld().GetRotation().RotateVector(AngularTargetVelocity);
	
	EngineCommands.Add(ComputeLinearVelocityStabilisation(DeltaSeconds, Engines, LinearTarget, 0.0));
	EngineCommands.Add(ComputeAngularVelocityStabilisation(DeltaSeconds, Engines, AngularTarget));


	for (int32 EngineIndex = 0; EngineIndex < Engines.Num(); EngineIndex++) {
		float ThrustRatio = 0;
		for (int32 CommandIndex = 0; CommandIndex < EngineCommands.Num(); CommandIndex++) {
			float newThustRatio =EngineCommands[CommandIndex][EngineIndex];
			//FLOGV("Merge command engine %d Commande %d newThustRatio=%f",EngineIndex, CommandIndex, newThustRatio);
			/*if (newThustRatio * ThrustRatio < 0) {
				// Opposite order
				ThrustRatio = 0;
			}*/
			ThrustRatio = ThrustRatio + newThustRatio;
		}
		UFlareEngine* Engine = Cast<UFlareEngine>(Engines[EngineIndex]);
		//FLOGV("Merge command engine %d ThrustRatio=%f",EngineIndex, ThrustRatio);
		//Engine->SetTargetThrustRatio(ThrustRatio);
	}
}

void AFlareShip::PhysicSubTick(float DeltaTime)
{
	//UE_LOG(LogTemp, Warning, TEXT("PhysicSubTick"));

	FVector Acceleration = FVector(0);
	Acceleration += TickSumForce / Airframe->GetMass();
	/*UE_LOG(LogTemp, Warning, TEXT("0 - TickSumForce: %s"), *TickSumForce.ToString());
	UE_LOG(LogTemp, Warning, TEXT("1 - Mass: %f"), Airframe->GetMass());
	UE_LOG(LogTemp, Warning, TEXT("2 - deltaTime: %f"), DeltaTime);
	UE_LOG(LogTemp, Warning, TEXT("3 - Acceleration: %s"), *Acceleration.ToString());*/
	
	
	Airframe->SetPhysicsLinearVelocity(Acceleration * DeltaTime * 100, true); // Multiply by 100 because UE4 works in cm

	//UE_LOG(LogTemp, Warning, TEXT("4 - GetPhysicsLinearVelocity: %s"), *Airframe->GetPhysicsLinearVelocity().ToString());

	
	// TODO find a better place to set
	float WorldInertiaTensor = 1000;

	FVector AngularAcceleration = FVector(0);
	AngularAcceleration += TickSumTorque / WorldInertiaTensor;
	// TODO Clamp

	Airframe->SetPhysicsAngularVelocity(AngularAcceleration * DeltaTime, true);

	// Reset force and torque for next tick
	TickSumForce = FVector::ZeroVector;
	TickSumTorque = FVector::ZeroVector;
}

void AFlareShip::AddForceAtLocation(FVector LinearForce, FVector AngularForce, FVector applicationPoint)
{
	TickSumForce += LinearForce;
	FVector ApplicationOffset = (applicationPoint - COM) / 100; // TODO divise by 100 in parameter

	TickSumTorque += FVector::CrossProduct(ApplicationOffset, AngularForce);
}

FVector AFlareShip::GetLinearVelocity() const
{
	return Airframe->GetPhysicsLinearVelocity() / 100;
}

FVector AFlareShip::GetTotalMaxThrustInAxis(TArray<UActorComponent*>& Engines, FVector Axis, float ThurstAngleLimit) const
{
	Axis.Normalize();
	FVector TotalMaxThrust = FVector::ZeroVector;
	for (int32 i = 0; i < Engines.Num(); i++) {
		UFlareEngine* Engine = Cast<UFlareEngine>(Engines[i]);

		FVector WorldThurstAxis = Engine->GetThurstAxis();

		float dot = FVector::DotProduct(WorldThurstAxis, Axis);
		if (dot > ThurstAngleLimit) {
			float ratio = (dot - ThurstAngleLimit) / (1 - ThurstAngleLimit);

			TotalMaxThrust += WorldThurstAxis * Engine->GetMaxThrust() * ratio;
		}
	}

	return TotalMaxThrust;
}

FVector AFlareShip::GetTotalMaxTorqueInAxis(TArray<UActorComponent*>& Engines, FVector TorqueAxis, FVector COM, float ThurstAngleLimit) const
{
	TorqueAxis.Normalize();
	FVector TotalMaxTorque = FVector::ZeroVector;
	for (int32 i = 0; i < Engines.Num(); i++) {
		UFlareEngine* Engine = Cast<UFlareEngine>(Engines[i]);

		float MaxThrust = Engine->GetMaxThrust();
		
		if (MaxThrust == 0) {
			// Not controlable engine
			continue;
		}

		FVector EngineOffset = (Engine->GetComponentLocation() - COM) / 100;
		
		FVector WorldThurstAxis = Engine->GetThurstAxis();
		FVector TorqueDirection = FVector::CrossProduct(EngineOffset, WorldThurstAxis);
		TorqueDirection.Normalize();

		float dot = FVector::DotProduct(TorqueAxis, TorqueDirection);
		if (dot > ThurstAngleLimit) {
			float ratio = (dot - ThurstAngleLimit) / (1 - ThurstAngleLimit);

			TotalMaxTorque += FVector::CrossProduct(EngineOffset, WorldThurstAxis * MaxThrust * ratio);
		}
	}

	return TotalMaxTorque;


}

/*----------------------------------------------------
	Autopilot
----------------------------------------------------*/

float* AFlareShip::ComputeLinearVelocityStabilisation(float DeltaSeconds, TArray<UActorComponent*>& Engines, FVector WorldTargetSpeed, float ThrustAngleLimit) const
{
	//FLOGV("ComputeLinearVelocityStabilisation WorldTargetSpeed=%s",*WorldTargetSpeed.ToString());
  
  	FVector WorldVelocity = GetLinearVelocity();
	
	//FLOGV("ComputeLinearVelocityStabilisation WorldVelocity=%s",*WorldVelocity.ToString());
	
	float* command = new float[Engines.Num()];
	for (int32 i = 0; i < Engines.Num(); i++) {
		UFlareEngine* Engine = Cast<UFlareEngine>(Engines[i]);

		FVector WorldThurstAxis = Engine->GetThurstAxis();
		
		float LocalTargetVelocity = FVector::DotProduct(WorldThurstAxis, WorldTargetSpeed);

		float TotalMaxThrustInAxis = FVector::DotProduct(WorldThurstAxis, GetTotalMaxThrustInAxis(Engines, WorldThurstAxis, ThrustAngleLimit));
				
		// Compute delta to stop
		float WorldVelocityToEnginesStop = FVector::DotProduct(WorldThurstAxis, WorldVelocity);
		//WorldVelocityToEnginesStop += FVector::DotProduct(WorldThurstAxis, getDeltaVToEnginesRatio(Engines, Mass, FinalThurstRatio, WorldThurstAxis, ThrustAngleLimit));

		// Check if air resistant won't make the estimation optimist.
		//WorldVelocityToEnginesStop += FVector::DotProduct(WorldThurstAxis, (getEngineToRatioDuration(Engine, FinalThurstRatio) * (-FinalThurst) / Mass)); // Assusme the air resistance will be almost constant during all the process. It's wrong, but it's better than noting

		float DeltaVelocityToStop = (LocalTargetVelocity - WorldVelocityToEnginesStop);
		
		/*FLOGV("ComputeLinearVelocityStabilisation for engine %d WorldThurstAxis=%s",i, *WorldThurstAxis.ToString());
		FLOGV("ComputeLinearVelocityStabilisation for engine %d LocalTargetVelocity=%f",i, LocalTargetVelocity);
		FLOGV("ComputeLinearVelocityStabilisation for engine %d TotalMaxThrustInAxis=%f",i, TotalMaxThrustInAxis);
		FLOGV("ComputeLinearVelocityStabilisation for engine %d WorldVelocityToEnginesStop=%f",i, WorldVelocityToEnginesStop);
		FLOGV("ComputeLinearVelocityStabilisation for engine %d DeltaVelocityToStop=%f",i, DeltaVelocityToStop);*/
		
		if(FMath::Abs(DeltaVelocityToStop) < 0.0001) {
		    command[i] = 0;
		    continue;
		}

		float ThrustAjustement = DeltaVelocityToStop * Airframe->GetMass() / (1.5*DeltaSeconds);
		float ThrustCommand = FMath::Clamp((ThrustAjustement / TotalMaxThrustInAxis), -1.0f, 1.0f);
	
		

		if (FMath::IsNearlyZero(ThrustCommand)) {
			ThrustCommand = 0;
		}
		
		/*FLOGV("ComputeLinearVelocityStabilisation for engine %d DeltaSeconds=%f",i, DeltaSeconds);
		FLOGV("ComputeLinearVelocityStabilisation for engine %d Mass=%f",i, Airframe->GetMass());
		FLOGV("ComputeLinearVelocityStabilisation for engine %d ThrustAjustement=%f",i, ThrustAjustement);
		
		FLOGV("ComputeLinearVelocityStabilisation for engine %d ThrustCommand=%f",i, ThrustCommand);*/
		Engine->SetTargetLinearThrustRatio(ThrustCommand);
		command[i] = ThrustCommand;
	}
	return command;
}

float* AFlareShip::ComputeAngularVelocityStabilisation(float DeltaSeconds, TArray<UActorComponent*>& Engines, FVector WorldTargetSpeed) const
{
	FVector AngularVelocity = Airframe->GetPhysicsAngularVelocity();

	float* command = new float[Engines.Num()];
	int index = 0;
	for (int32 i = 0; i < Engines.Num(); i++) {
		UFlareEngine* Engine = Cast<UFlareEngine>(Engines[i]);

		FVector EngineOffset = (Engine->GetComponentLocation() - COM) / 100;

		FVector WorldThurstAxis = Engine->GetThurstAxis();
		FVector TorqueDirection = FVector::CrossProduct(EngineOffset, WorldThurstAxis); 
		if (TorqueDirection.Size() < 0.001) {
			command[index++] = 0;
			continue;
		}
		TorqueDirection.Normalize();

		float LocalTargetVelocity = FVector::DotProduct(TorqueDirection, WorldTargetSpeed);

		float TotalMaxTorqueInAxis = FVector::DotProduct(TorqueDirection, GetTotalMaxTorqueInAxis(Engines, TorqueDirection, COM, 0));
		if (FMath::IsNearlyZero(TotalMaxTorqueInAxis)) {
			// Just wait better days
			command[index++] = 0;
			continue;
		}

		// Compute delta to stop
		float WorldVelocityToEnginesStop = FVector::DotProduct(TorqueDirection, AngularVelocity);
		//WorldVelocityToEnginesStop += FVector::DotProduct(TorqueDirection, getDeltaAngularVelocityToEnginesRatio(Engines, COM, InertiaTensor, FinalThurstRatio)); // TODO inertia

		// Check if air resistant won't make the estimation optimist.
		//WorldVelocityToEnginesStop += FVector::DotProduct(TorqueDirection, (getEngineToRatioDuration(Engine, FinalThurstRatio) * (-FinalTorque) / InertiaTensor)); // Assusme the air resistance will be almost constant during all the process. It's wrong, but it's better than noting

		float DeltaVelocityToStop = (LocalTargetVelocity - WorldVelocityToEnginesStop);

		float InertiaTensor = 1000; //TODO externalize
		
		float ThrustAjustement = DeltaVelocityToStop * InertiaTensor / (1.5*DeltaSeconds);
		float ThrustCommand = FMath::Clamp((ThrustAjustement / TotalMaxTorqueInAxis), -1.0f, 1.0f);

		if (FMath::IsNearlyZero(ThrustCommand)) {
			ThrustCommand = 0;
		}
		Engine->SetTargetAngularThrustRatio(ThrustCommand);
		command[index++] = ThrustCommand;
	}

	return command;
}