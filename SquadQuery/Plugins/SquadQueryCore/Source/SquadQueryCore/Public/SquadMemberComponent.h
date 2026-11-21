// SquadQueryCore - puts an actor into a squad.

#pragma once

#include "Components/ActorComponent.h"
#include "SquadMemberComponent.generated.h"

#define UE_API SQUADQUERYCORE_API

class USquad;

/**
 * Makes its owner a member of squad SquadId: it registers with USquadQuerySubsystem in BeginPlay and leaves in EndPlay.
 * A component, so any pawn can join a squad, Blueprint pawns included (02 section 4.1). It never ticks.
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

protected:
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class USquadQuerySubsystem;

	UPROPERTY(EditAnywhere, Category = "Squad")
	int32 SquadId = 0;

	/** Set by the subsystem on registration, cleared on unregistration. */
	UPROPERTY(Transient)
	TObjectPtr<USquad> Squad;
};

#undef UE_API
