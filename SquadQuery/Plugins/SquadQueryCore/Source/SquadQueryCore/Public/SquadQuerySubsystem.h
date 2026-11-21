// SquadQueryCore - the world's squads.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "SquadQuerySubsystem.generated.h"

#define UE_API SQUADQUERYCORE_API

class USquad;
class USquadMemberComponent;

/**
 * Owns every squad of a world. S3: the registry and the threat each squad knows about, which every configuration reads
 * (the per-agent baselines through USquadQueryContext_Threat). S5 adds the shared evaluation.
 */
UCLASS(MinimalAPI)
class USquadQuerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Called by a member in BeginPlay: creates its squad on first use and adds it. */
	UE_API void RegisterMember(USquadMemberComponent& Member);

	/** Called by a member in EndPlay. Empty squads are kept: their id stays valid for the world's lifetime. */
	UE_API void UnregisterMember(USquadMemberComponent& Member);

	/** Tells a squad where its threat is. */
	UE_API void SetThreat(int32 SquadId, const FVector& Location);

	/** The squad with this id, or null if no member ever joined it. */
	UE_API USquad* FindSquad(int32 SquadId) const;

	const TMap<int32, TObjectPtr<USquad>>& GetSquads() const { return Squads; }

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only. */
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	UE_API virtual void Deinitialize() override;
	//~ End UWorldSubsystem

private:
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USquad>> Squads;
};

#undef UE_API
