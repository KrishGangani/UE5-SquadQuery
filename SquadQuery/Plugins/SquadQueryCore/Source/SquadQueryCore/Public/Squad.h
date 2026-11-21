// SquadQueryCore - one squad: its members and what it knows about its threat.

#pragma once

#include "UObject/Object.h"
#include "Squad.generated.h"

#define UE_API SQUADQUERYCORE_API

class USquadMemberComponent;

/**
 * One squad, owned by USquadQuerySubsystem. A UObject because EQS needs a UObject owner for every query, and the
 * contexts must find the squad from that owner (02 section 4.1). Its member list is a UPROPERTY, so the garbage collector
 * sees it; members are added and removed only by the subsystem.
 */
UCLASS(MinimalAPI)
class USquad : public UObject
{
	GENERATED_BODY()

public:
	int32 GetSquadId() const { return SquadId; }
	const TArray<TObjectPtr<USquadMemberComponent>>& GetMembers() const { return Members; }

	/** The threat this squad responds to. Set by game code through USquadQuerySubsystem::SetThreat. */
	bool HasThreat() const { return bHasThreat; }
	const FVector& GetThreatLocation() const { return ThreatLocation; }

private:
	friend class USquadQuerySubsystem;

	int32 SquadId = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USquadMemberComponent>> Members;

	FVector ThreatLocation = FVector::ZeroVector;
	bool bHasThreat = false;
};

#undef UE_API
