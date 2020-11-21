// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterCharacterMovement.h"
#include "Classes/Kismet/KismetSystemLibrary.h"

#include <string>

#include "ShooterPlayerState.h"

UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AirControl = 0.95f;
	
	// Jetpack
	bIsJetpackEnabled = true;
	bIsJetpackActive = false;
	JetpackForce = 1500.f;
	JetpackAccelerationModifier = 1.5f;
	MaxJetpackFuel = 100.0f;
	JetpackFuel = MaxJetpackFuel;
	JetpackFuelConsumptionRate = 1.0f;
	JetpackFuelRefillRate = 10.0f;

	// Wall jump
	WallJumpStrength = 1000.0f;
	
	// Movement flags
	bWantsToTeleport = false;
	bWantsToJetpack = false;

}

void UShooterCharacterMovement::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (!CharacterOwner)
	{
		return;
	}

	if (PawnOwner->IsLocallyControlled())
	{
		MoveDirection = PawnOwner->GetLastMovementInputVector();
		if (GetNetMode() == ENetMode::NM_Client)
		{
			ServerSetMoveDirection(MoveDirection);
		}
	}
	
	// Teleport is executed both on the owning client for responsiveness and the server
	if (bWantsToTeleport && (CharacterOwner->GetLocalRole() == ROLE_Authority || CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy))
	{
		AShooterCharacter* ShooterCharacterOwner = Cast<AShooterCharacter>(PawnOwner);

		const FVector CurrentLocation = ShooterCharacterOwner->GetActorLocation();
		const FVector NewLocation = ShooterCharacterOwner->GetActorLocation() + ShooterCharacterOwner->GetActorForwardVector() * ShooterCharacterOwner->GetTeleportDistance();

		// Teleport forward to the nearest non blocked location inside the TeleportDistance
		PawnOwner->SetActorLocation(NewLocation, true);
		bWantsToTeleport = false;
	}

	// Jetpack movement executed both on clients and server 
	if (bWantsToJetpack == true && CanJetpack() == true)
	{
		bIsJetpackActive = true;	
		JetpackFuel = FMath::Clamp(JetpackFuel - JetpackFuelConsumptionRate * DeltaSeconds, 0.0f, MaxJetpackFuel);

		// The owner client update the fuel left in the player state 
		AShooterPlayerController* PlayerController = PawnOwner ? Cast<AShooterPlayerController>(PawnOwner->GetController()) : NULL;
		if(PlayerController)
		{
			AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerController->PlayerState);
			PlayerState->SetJetpackFuelLeft(static_cast<int32>(JetpackFuel));
		}
		
		SetMovementMode(MOVE_Falling);
		
		Velocity.Z += JetpackForce * DeltaSeconds;
		Velocity.Y += (MoveDirection.Y * JetpackForce * DeltaSeconds) * 0.5f;
		Velocity.X += (MoveDirection.X * JetpackForce * DeltaSeconds) * 0.5f;
	}
	else bIsJetpackActive = false;
	
	RefillJetpack(DeltaSeconds);
}

float UShooterCharacterMovement::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();

	const AShooterCharacter* ShooterCharacterOwner = Cast<AShooterCharacter>(PawnOwner);
	if (ShooterCharacterOwner)
	{
		if (ShooterCharacterOwner->IsTargeting())
		{
			MaxSpeed *= ShooterCharacterOwner->GetTargetingSpeedModifier();
		}
		if (ShooterCharacterOwner->IsRunning())
		{
			MaxSpeed *= ShooterCharacterOwner->GetRunningSpeedModifier();
		}
	}

	return MaxSpeed;
}

float UShooterCharacterMovement::GetMaxAcceleration() const
{
	float MaxAcceleration = Super::GetMaxAcceleration();
	
	if (bIsJetpackActive == true && bWantsToJetpack == true)
	{
		MaxAcceleration *= JetpackAccelerationModifier;
	}
	
	return MaxAcceleration;
}

FVector UShooterCharacterMovement::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	if (bIsJetpackActive == true)
	{
		// Better scale down the gravity when using the jetpack
		return Super::NewFallVelocity(InitialVelocity, (Gravity * GravityScaleWhileJetpack), DeltaTime);
	}
	
	return Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
}

bool UShooterCharacterMovement::ServerSetMoveDirection_Validate(const FVector& MoveDir)
{
	return true;
}

void UShooterCharacterMovement::ServerSetMoveDirection_Implementation(const FVector& MoveDir)
{
	MoveDirection = MoveDir;
}

void UShooterCharacterMovement::DoActivateJetpack(bool bActivate)
{
	bWantsToJetpack = bActivate;
}

bool UShooterCharacterMovement::IsUsingJetpack() const
{
	return bIsJetpackActive;
}

bool UShooterCharacterMovement::CanJetpack() const
{
	return JetpackFuel > 0.0f;
}

void UShooterCharacterMovement::RefillJetpack(float DeltaSeconds)
{
	if (IsMovingOnGround())
	{
		if(JetpackFuel < MaxJetpackFuel) JetpackFuel = FMath::Clamp(JetpackFuel + JetpackFuelRefillRate * DeltaSeconds, 0.0f, MaxJetpackFuel);
		AShooterPlayerController* PlayerController = PawnOwner ? Cast<AShooterPlayerController>(PawnOwner->GetController()) : NULL;
		if(PlayerController)
		{
			AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(PlayerController->PlayerState);
			PlayerState->SetJetpackFuelLeft(static_cast<int32>(JetpackFuel));
		}
	}
}

bool UShooterCharacterMovement::IsInAirNearWall(FVector& WallNormal) const
{
	if(IsMovingOnGround()) return false;
	
	FVector Location = CharacterOwner->GetActorLocation() - FVector(0.0f, 0.0f, CharacterOwner->GetDefaultHalfHeight());
	float Radius = CharacterOwner->GetSimpleCollisionRadius() + 30.0f;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic) };
	TArray<AActor*> IgnoredActors;
	TArray<AActor*> OverlappedActors;

	if(UKismetSystemLibrary::SphereOverlapActors(GetWorld(), Location, Radius, ObjectTypes, ABlockingVolume::StaticClass(), IgnoredActors, OverlappedActors))
	{	
		// Can overlap more than one bounding volume (es. ceil and side wall)
		for(const AActor* Wall : OverlappedActors)
		{
			// Get the bounding volume normal
			FHitResult OutHit;
			FCollisionQueryParams CollisionParams;
			Wall->ActorLineTraceSingle(OutHit, Location, FVector(Wall->GetActorLocation().X, Wall->GetActorLocation().Y, Location.Z), ECC_WorldStatic, CollisionParams);

			// Skip bounding volume that are not vertical
			WallNormal = FVector(OutHit.Normal.X, OutHit.Normal.Y, OutHit.Normal.Z);
			if(FMath::IsNearlyZero(WallNormal.X) && FMath::IsNearlyZero(WallNormal.Y)) continue;	// Skip ceil and floor
			if(WallNormal.CosineAngle2D(WallNormal) < 0.25*PI) continue;	// Skip "not so vertical" walls 

			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Normal ") + WallNormal.ToString());

			// The player is near a wall
			return true;
		}
		
		return false;
	}
	
	return false;
}

void UShooterCharacterMovement::DoWallJump() const
{
	FVector WallNormal;
	if(bIsJetpackActive && IsInAirNearWall(WallNormal))
	{
		FVector JumpDirection = WallNormal + FVector(0.0f, 0.0f, 10.f); // Jump a little higher
		ServerLaunchCharacter(WallNormal * WallJumpStrength);
		if(PawnOwner->GetLocalRole() < ROLE_Authority) CharacterOwner->LaunchCharacter(WallNormal * WallJumpStrength, false, false);
	}
}

void UShooterCharacterMovement::DoTeleport()
{
	bWantsToTeleport = true;
}

void UShooterCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToTeleport = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToJetpack = (Flags&FSavedMove_Character::FLAG_Custom_1) != 0;
}

class FNetworkPredictionData_Client* UShooterCharacterMovement::GetPredictionData_Client() const
{
	check(PawnOwner != NULL);
	//check(PawnOwner->GetLocalRole() < ROLE_Authority);

	if (!ClientPredictionData)
	{
		UShooterCharacterMovement* MutableThis = const_cast<UShooterCharacterMovement*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ExtendedMovement(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

bool UShooterCharacterMovement::ServerLaunchCharacter_Validate(const FVector& Direction)
{
	return true;
}

void UShooterCharacterMovement::ServerLaunchCharacter_Implementation(const FVector& Direction) const
{
	if(PawnOwner->GetLocalRole() == ROLE_Authority)
		CharacterOwner->LaunchCharacter(Direction, false, false);
}

void FSavedMove_ExtendedMovement::Clear()
{
	Super::Clear();
	bSavedWantsToTeleport = 0;
	bSavedWantsToJetpack = 0;
}

uint8 FSavedMove_ExtendedMovement::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if(bSavedWantsToTeleport) Result |= FLAG_Custom_0;
	if(bSavedWantsToJetpack) Result |= FLAG_Custom_1;
	
	return Result;
}

bool FSavedMove_ExtendedMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (bSavedWantsToTeleport != ((FSavedMove_ExtendedMovement*)& NewMove)->bSavedWantsToTeleport)
	{
		return false;
	}
	if (bSavedWantsToJetpack != ((FSavedMove_ExtendedMovement*)&NewMove)->bSavedWantsToJetpack)
	{
		return false;
	}
	if (SavedJetpackFuel != ((FSavedMove_ExtendedMovement*)&NewMove)->SavedJetpackFuel)
	{
		return false;
	}
	
	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_ExtendedMovement::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UShooterCharacterMovement* CharMov = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (CharMov)
	{
		// Set the move properties before saving and sending it to the server
		SavedMoveDirection = CharMov->MoveDirection;
		bSavedWantsToTeleport = CharMov->bWantsToTeleport;
		SavedJetpackFuel = CharMov->JetpackFuel;
		bSavedWantsToJetpack = CharMov->bWantsToJetpack;
	}
}

void FSavedMove_ExtendedMovement::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UShooterCharacterMovement* CharMov = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (CharMov)
	{
		// Set up character properties, used in movement prediction
		CharMov->MoveDirection = SavedMoveDirection;
		CharMov->JetpackFuel = SavedJetpackFuel;
		CharMov->bWantsToTeleport = bSavedWantsToTeleport;
		CharMov->bWantsToJetpack = bSavedWantsToJetpack;
	}
}

FNetworkPredictionData_Client_ExtendedMovement::FNetworkPredictionData_Client_ExtendedMovement(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_ExtendedMovement::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ExtendedMovement());
}