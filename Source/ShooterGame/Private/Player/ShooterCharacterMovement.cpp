// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterGame.h"
#include "Player/ShooterCharacterMovement.h"

UShooterCharacterMovement::UShooterCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsToTeleport = false;
}

void UShooterCharacterMovement::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (!CharacterOwner)
	{
		return;
	}

	// Teleport is executed both on the owning client and the server
	if (bWantsToTeleport && (CharacterOwner->GetLocalRole() == ROLE_Authority || CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy))
	{
		const AShooterCharacter* ShooterCharacterOwner = Cast<AShooterCharacter>(PawnOwner);

		const FVector CurrentLocation = ShooterCharacterOwner->GetActorLocation();
		const FVector NewLocation = ShooterCharacterOwner->GetActorLocation() + ShooterCharacterOwner->GetActorForwardVector() * ShooterCharacterOwner->GetTeleportDistance();

		// Teleport forward to the nearest non blocked location inside the TeleportDistance
		PawnOwner->SetActorLocation(NewLocation, true);
		bWantsToTeleport = false;
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

void UShooterCharacterMovement::DoTeleport()
{
	bWantsToTeleport = true;
}

void UShooterCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)//Client only
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToTeleport = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
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

void FSavedMove_ExtendedMovement::Clear()
{
	Super::Clear();
	bSavedWantsToTeleport = 0;
}

uint8 FSavedMove_ExtendedMovement::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if(bSavedWantsToTeleport) Result |= FLAG_Custom_0;
	
	return Result;
}

bool FSavedMove_ExtendedMovement::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (bSavedWantsToTeleport != ((FSavedMove_ExtendedMovement*)& NewMove)->bSavedWantsToTeleport)
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
		// Set the move properties before saving and sending to the server
		bSavedWantsToTeleport = CharMov->bWantsToTeleport;
	}
}

void FSavedMove_ExtendedMovement::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UShooterCharacterMovement* CharMov = Cast<UShooterCharacterMovement>(Character->GetCharacterMovement());
	if (CharMov)
	{
		// Set up character properties, used in movement prediction
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