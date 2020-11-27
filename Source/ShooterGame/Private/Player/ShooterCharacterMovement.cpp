// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterCharacterMovement.h"
#include "Classes/Kismet/KismetSystemLibrary.h"

#include <string>

#include "ShooterPlayerState.h"

UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AirControl = 0.95f;

	// Teleport
	bWantsToTeleport = false;
	
	// Jetpack
	bIsJetpackEnabled = true;
	bIsJetpackActive = false;
	JetpackForce = 1500.f;
	JetpackAccelerationModifier = 1.5f;
	MaxJetpackFuel = 100.0f;
	JetpackFuel = MaxJetpackFuel;
	JetpackFuelConsumptionRate = 1.0f;
	JetpackFuelRefillRate = 10.0f;
	bWantsToJetpack = false;
	
	// Wall jump
	WallJumpStrength = 1000.0f;

	// Wall run
	bWallRunEnable = true;
	MaxWallRunTime = 3.5f;
	MinSpeedToWallRun = 50.0f;
	WallNormal = FVector(0.0f);
	WallRunSide = 0;
	bIsWallRunning = false;
	bWantsToWallRun = false;
}

void UShooterCharacterMovement::GetLifetimeReplicatedProps(::TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
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

	
	//// Wall run ////
	
	if(IsMovingOnGround()) // Reset wall jump variables when walking on the ground
	{
		WallNormal = FVector(0.0f);
		WallRunSide = 0;
		CharacterSideLean(0.0f);
	}
	
	if(bWallRunEnable && bWantsToWallRun && !bIsWallRunning && CanWallRun()) // Not wall running -> wall running
	{ 
		SetWallRun(true, WallNormal);
	}
	else if(bWallRunEnable && (!bWantsToWallRun || !CanWallRun()) && bIsWallRunning) // Wall running -> not wall running
	{
		SetWallRun(false, WallNormal);
	}

	
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
	if (bIsWallRunning)
	{
		return Super::NewFallVelocity(InitialVelocity, FVector(0.0f), DeltaTime);
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

bool UShooterCharacterMovement::IsInAirNearWall(FVector& WallNormal)
{
	if(IsMovingOnGround()) return false;
	
	FVector Location = CharacterOwner->GetActorLocation() - FVector(0.0f, 0.0f, CharacterOwner->GetDefaultHalfHeight());
	float Radius = CharacterOwner->GetSimpleCollisionRadius() + 30.0f;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = { UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic) };
	TArray<AActor*> IgnoredActors;
	TArray<AActor*> OverlappedActors;
	if(UKismetSystemLibrary::SphereOverlapActors(GetWorld(), Location, Radius, ObjectTypes, ABlockingVolume::StaticClass(), IgnoredActors, OverlappedActors))
	{	
		// Can overlap more than one bounding volume (es. ceil and side wall), look for a valid overlap
		for(const AActor* Wall : OverlappedActors)
		{
			if (!Wall->ActorHasTag("Wall")) continue;	// Only objects tagged as "Wall" can be runned
			

			// Look for the best collision point in the four direction respect to the actor: forward, left, back, right
			FVector TestDirs[4] = { CharacterOwner->GetActorForwardVector() * Radius, CharacterOwner->GetActorRightVector() * Radius, -CharacterOwner->GetActorForwardVector() * Radius, -CharacterOwner->GetActorRightVector() * Radius };
			FHitResult OutHit;
			FCollisionQueryParams CollisionParams;
			float ImpactDistance = 0;
			FVector HitPoint;
			bool bFound = false;
			for(FVector TestDir : TestDirs)
			{
				Wall->ActorLineTraceSingle(OutHit, Location, Location + TestDir, ECC_WorldStatic, CollisionParams);
				if(OutHit.Normal.IsZero()) continue; // No hit in this direction
				
				if(WallNormal.IsZero()) WallNormal = OutHit.Normal; // First hit 

				float NewImpactDistance = (OutHit.ImpactPoint - Location).Size(); 
				if(ImpactDistance == 0 || NewImpactDistance < ImpactDistance ) // Check if this is is better than the previous one
				{
					WallNormal = OutHit.Normal;
					HitPoint = OutHit.ImpactPoint;
					ImpactDistance = NewImpactDistance;
					bFound = true;
				} 
			}

			if(!bFound) continue; // No valid hit found -> Test next colliding volume

			// Skip bounding volume that are not vertical
			if(FMath::IsNearlyZero(WallNormal.X) && FMath::IsNearlyZero(WallNormal.Y)) continue; // Skip ceil and floor
			if(WallNormal.CosineAngle2D(WallNormal) < 0.25*PI) continue; // Skip "not so vertical" walls 
			
			// DrawDebugDirectionalArrow(GetWorld(), HitPoint, HitPoint + WallNormal * 100.0f, 120.f, FColor::Magenta, true, -1.f, 0, 5.f);

			// Update wall side due the character can turn 180 while wall running
			float NewWallRunSide = FMath::Sign(FVector::CrossProduct(WallNormal, PawnOwner->GetActorForwardVector()).Z);
			AShooterPlayerController* PlayerController = PawnOwner ? Cast<AShooterPlayerController>(PawnOwner->GetController()) : NULL;
			if(PlayerController && NewWallRunSide != WallRunSide)
			{
				WallRunSide = NewWallRunSide;
				CharacterSideLean(0.0f);
				CharacterSideLean(-10.0f * WallRunSide); // Apply the proper lean
			}
			
			return true; // The player is near a wall -> return true 
		}
		
		return false;
	}
	
	return false;
}

void UShooterCharacterMovement::DoNormalWallJump()
{
	FVector WallNormal;
	
	if(IsInAirNearWall(WallNormal))
	{
		FVector JumpDirection = WallNormal + FVector(0.0f, 0.0f, 10.f); // Jump a little higher
		ServerLaunchCharacter(WallNormal * WallJumpStrength);
		if(PawnOwner->GetLocalRole() < ROLE_Authority) CharacterOwner->LaunchCharacter(WallNormal * WallJumpStrength, false, false);
	}
}

void UShooterCharacterMovement::DoWallJump(FVector Direction)
{
	FVector WallNormal;
	if(IsInAirNearWall(WallNormal))
	{
		ServerLaunchCharacter(Direction * WallJumpStrength);
		if(PawnOwner->GetLocalRole() < ROLE_Authority) CharacterOwner->LaunchCharacter(Direction * WallJumpStrength, false, false);
	}
}

void UShooterCharacterMovement::CharacterSideLean(const float SideLeanAmount) const
{
	AShooterPlayerController* PlayerController = PawnOwner ? Cast<AShooterPlayerController>(PawnOwner->GetController()) : NULL;			
	if(PlayerController)
	{
		const FRotator CurrentRot = PlayerController->GetControlRotation();
		PlayerController->SetControlRotation( FRotator(CurrentRot.Pitch, CurrentRot.Yaw, SideLeanAmount));
	}
}

void UShooterCharacterMovement::SetWallRun(const bool bNewIsWallRunning, const FVector NewWallNormal)
{
	ClearWallRunTimer();	// Clear pending timer that handle max wall run allowed time
	
	WallNormal = NewWallNormal;
	bIsWallRunning = bNewIsWallRunning;
	
	if(bIsWallRunning)	// Enter wall run
	{
		SetMovementMode(MOVE_Flying);
		
		if(PawnOwner->IsLocallyControlled())
		{
			// Lean in the opposite wall direction
			AShooterPlayerController* PlayerController = PawnOwner ? Cast<AShooterPlayerController>(PawnOwner->GetController()) : NULL;
			WallRunSide = FMath::Sign(FVector::CrossProduct(WallNormal, PawnOwner->GetActorForwardVector()).Z);
			CharacterSideLean(-10.0f * WallRunSide);

			// Set max wall run timer
			GetWorld()->GetTimerManager().SetTimer(WallRunTimerHandle, this, &UShooterCharacterMovement::StopWallRun, MaxWallRunTime, false);
		}
		// Velocity.Z = 0.0;
	}
	else	// Exit wall run
	{
		SetMovementMode(MOVE_Falling);
		if(PawnOwner->IsLocallyControlled())
		{
			CharacterSideLean(0.0f);
		}
	}	
}

void UShooterCharacterMovement::ClearWallRunTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(WallRunTimerHandle);
	WallRunTimerHandle.Invalidate();
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);	// WORKAROUND: Invalidate does not work, have to clear all timers
}

void UShooterCharacterMovement::StopWallRun()
{
	DoNormalWallJump();
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
	bWantsToWallRun = (Flags&FSavedMove_Character::FLAG_Custom_2) != 0;
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

bool UShooterCharacterMovement::CanWallRun()
{
	const float Speed = FVector2D(CharacterOwner->GetVelocity().X, CharacterOwner->GetVelocity().Y).Size();

	if(!bIsWallRunning && Velocity.Z > 0) return false;	// Only attach to wall when falling
	
	return (bWallRunEnable && IsInAirNearWall(WallNormal)) && (Speed > MinSpeedToWallRun);	// Cannot wall run too slow
}

void UShooterCharacterMovement::DoWallRun(bool bNewWantsToWallRun)
{
	bWantsToWallRun = bNewWantsToWallRun;
}

void FSavedMove_ExtendedMovement::Clear()
{
	Super::Clear();
	bSavedWantsToTeleport = 0;
	bSavedWantsToJetpack = 0;
	bSavedWantsToWallRun = 0;
	bSavedIsWallRunning = 0;
}

uint8 FSavedMove_ExtendedMovement::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if(bSavedWantsToTeleport) Result |= FLAG_Custom_0;
	if(bSavedWantsToJetpack) Result |= FLAG_Custom_1;
	if(bSavedWantsToWallRun) Result |= FLAG_Custom_2;
	
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

	// Wall run
	if (bSavedWantsToWallRun != ((FSavedMove_ExtendedMovement*)&NewMove)->bSavedWantsToWallRun)
	{
		return false;
	}
	if (bSavedIsWallRunning != ((FSavedMove_ExtendedMovement*)&NewMove)->bSavedIsWallRunning)
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
		bSavedWantsToWallRun = CharMov->bWantsToWallRun;
		bSavedIsWallRunning = CharMov->bIsWallRunning;
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
		CharMov->bWantsToWallRun = bSavedWantsToWallRun;
		CharMov->bIsWallRunning = bSavedIsWallRunning;
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