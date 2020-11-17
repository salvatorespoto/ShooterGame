// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once
#include "ShooterCharacterMovement.generated.h"

UCLASS()
class UShooterCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

	friend class FSavedMove_ExtendedMovement;

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	virtual float GetMaxSpeed() const override;

	/** Character teleport */
	bool bWantsToTeleport : 1;
	
	/** Teleport character forward of "distance" units */
	void DoTeleport();
};

class FSavedMove_ExtendedMovement : public FSavedMove_Character
{
public:

    typedef FSavedMove_Character Super;

    virtual void Clear() override;

    virtual uint8 GetCompressedFlags() const override;

    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;

    virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
    
    virtual void PrepMoveFor(class ACharacter* Character) override;

	/** Character teleport */
	uint8 bSavedWantsToTeleport : 1;
};

class FNetworkPredictionData_Client_ExtendedMovement : public FNetworkPredictionData_Client_Character
{
public:

	FNetworkPredictionData_Client_ExtendedMovement(const UCharacterMovementComponent& ClientMovement);

    typedef FNetworkPredictionData_Client_Character Super;

    virtual FSavedMovePtr AllocateNewMove() override;
};