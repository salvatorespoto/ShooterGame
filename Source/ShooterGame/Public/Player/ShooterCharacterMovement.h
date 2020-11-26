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
	
	virtual void GetLifetimeReplicatedProps(::TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	
	virtual float GetMaxSpeed() const override;

	virtual float GetMaxAcceleration() const override;

	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector MoveDirection;
	
	/** [Server] Set the current moving direction */
	UFUNCTION(Unreliable, Server, WithValidation)
    void ServerSetMoveDirection(const FVector& MoveDir);

	/** [Local] The character lean on the side (camera roll) of SideLeanAmount degrees */
	void CharacterSideLean(float SideLeanAmount) const;

	
	//// TELEPORT ////
	
	/** Character teleport */
	bool bWantsToTeleport : 1;

	/** Teleport character forward of "distance" units */
	void DoTeleport();

	
	//// JETPACK ////
	
	/** Character wants to activate the jetpack */
	bool bWantsToJetpack : 1;

	/** The jetpack is currently active */
	bool bIsJetpackActive : 1;

	/** The jetpack is ready and can be activated */
	UPROPERTY(BlueprintReadOnly, Category = "Jetpack")
	bool bIsJetpackEnabled;

	/** The jetpack max fuel capacity */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Jetpack")
	float MaxJetpackFuel;

	/** The jetpack fuel consuption rate when used */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Jetpack")
	float JetpackFuelConsumptionRate;
	
	/** The jetpack fuel refill rate when on the ground */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Jetpack")
	float JetpackFuelRefillRate;

	/** The jetpack fuel level */
	float JetpackFuel;
	
	/** Jetpack force */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Jetpack", AdvancedDisplay)
	float JetpackForce;

	/** Jetpack accelleration modifier */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Jetpack", AdvancedDisplay)
	float JetpackAccelerationModifier;

	/** Reduce the gravity when jetpacking */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Jetpack", meta = (ClampMin = 0))
	float GravityScaleWhileJetpack = 0;
	
	/** The player activate the jetpack */
	void DoActivateJetpack(bool bActivate);
	
	/** Check if the player is currently using the jetpack */
	bool IsUsingJetpack() const;
	
	/** Ceck if the player can activate the jetpack */
	bool CanJetpack() const;

	/** Refill the jetpcka fuel when moving on the ground*/
	void RefillJetpack(float DeltaSeconds);

	
	//// WALL JUMP while jetpacking ////

	/** The wall jump strength */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Jetpack")
	float WallJumpStrength;
	
	/** Check if the character is in the air and near a wall */
	bool IsInAirNearWall(FVector& WallNormal);
	
	/** [Server] + [Local] The character execute a wall jump in the wall normal direction */ 
	void DoNormalWallJump();

	/** [Server] + [Local] The character execute a wall jump in the direction Direction */
	void DoWallJump(FVector Direction);

	/** [Server] Launch the character */
	UFUNCTION(Reliable, Server, WithValidation)
    void ServerLaunchCharacter(const FVector& Direction) const;


	//// Wall run ////

	/** The character can to run on the walls */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Movement - WallRun")
	bool bWallRunEnable;
	
	/** Maximum time a character can run on the wall */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Movement - WallRun")
	float MaxWallRunTime;

	/** Timer that handles the max wall run time */
	FTimerHandle WallRunTimerHandle;
	
	/** Minimum speed required to run on the walls */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Movement - WallRun")
	float MinSpeedToWallRun;

	/** The character wants wall run */
	bool bWantsToWallRun;
	
	/** The character is currently wall running*/
	bool bIsWallRunning;
	
	/** The wall normal while wall running */
	FVector WallNormal;

	/** The current wall is on the left or the right ? */
	float WallRunSide;

	/** Check if yhe character can wall run */
	bool CanWallRun();
	
	/** The character activates wall run */
	void DoWallRun(bool bWantsToWallRun);

	/** Start/Stop wall running movement */
	void SetWallRun(bool bNewIsWallRunning, FVector NewWallNormal);

	/** Stop wall running */
	void StopWallRun();

	/** Clear the pending timer that handle the maximum allowed wall run time */
	void ClearWallRunTimer();
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
	
	/** Character is using jetpack */
	uint8 bSavedWantsToJetpack : 1;

	/** Character is using jetpack */
	float SavedJetpackFuel;

	/** Character moving direction */
	FVector SavedMoveDirection;

	
	//// Wall run

	/** Character wants to wall run */
	uint8 bSavedWantsToWallRun : 1;

	/** Character is currently wall running */
	uint8 bSavedIsWallRunning : 1;
};

class FNetworkPredictionData_Client_ExtendedMovement : public FNetworkPredictionData_Client_Character
{
public:

	FNetworkPredictionData_Client_ExtendedMovement(const UCharacterMovementComponent& ClientMovement);

    typedef FNetworkPredictionData_Client_Character Super;

    virtual FSavedMovePtr AllocateNewMove() override;
};