// SquadQueryCore - puts an actor into a squad, and carries the slot the squad assigned to it.

#pragma once

#include "Components/ActorComponent.h"
#include "SmartObjectRuntime.h"
#include "SquadMemberComponent.generated.h"

#define UE_API SQUADQUERYCORE_API

class USquad;
class USquadMemberComponent;

/** Fires when the squad gives this member a different slot (or none). Native: no reflection call per binding. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSquadAssignmentChangedNative, USquadMemberComponent& /*Member*/);
/** The same, for Blueprint pawns. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSquadAssignmentChanged, USquadMemberComponent*, Member);

/**
 * Makes its owner a member of squad SquadId: it registers with USquadQuerySubsystem in BeginPlay and leaves in EndPlay.
 * When the squad's shared evaluation runs (S5), the subsystem writes the member's assignment here, holds its Smart
 * Object claim, and broadcasts OnAssignmentChanged. A component, so any pawn can join a squad (02 section 4.1). It never ticks.
 */
UCLASS(MinimalAPI, ClassGroup = (AI))
class USquadMemberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API USquadMemberComponent();

	/** Which squad to join. Takes effect at BeginPlay: call it before the owner finishes spawning. */
	void SetSquadId(int32 InSquadId) { SquadId = InSquadId; }
	int32 GetSquadId() const { return SquadId; }

	/** The squad this member belongs to, or null before BeginPlay and after EndPlay. */
	USquad* GetSquad() const { return Squad; }

	/** The slot the squad assigned (invalid handle: none) and where it is. */
	const FSmartObjectSlotHandle& GetAssignedSlot() const { return AssignedSlot; }
	const FVector& GetAssignedLocation() const { return AssignedLocation; }

	FOnSquadAssignmentChangedNative OnAssignmentChangedNative;

	UPROPERTY(BlueprintAssignable, Category = "Squad")
	FOnSquadAssignmentChanged OnAssignmentChanged;

protected:
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class USquadQuerySubsystem;

	/** Called by the subsystem only. Broadcasts OnAssignmentChanged if the slot changed. */
	void SetAssignment(const FSmartObjectSlotHandle& Slot, const FVector& Location);

	UPROPERTY(EditAnywhere, Category = "Squad")
	int32 SquadId = 0;

	/** Set by the subsystem on registration, cleared on unregistration. */
	UPROPERTY(Transient)
	TObjectPtr<USquad> Squad;

	FSmartObjectSlotHandle AssignedSlot;
	FVector AssignedLocation = FVector::ZeroVector;

	/** The Smart Object claim the subsystem holds on AssignedSlot for this member. */
	FSmartObjectClaimHandle Claim;
};

#undef UE_API
