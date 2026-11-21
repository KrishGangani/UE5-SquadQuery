// SquadQueryCore - one squad: its members, what it knows about its threat, and its evaluation in flight.

#pragma once

#include "UObject/Object.h"
#include "Squad.generated.h"

#define UE_API SQUADQUERYCORE_API

class USquadMemberComponent;
struct FEnvQueryInstance;

/**
 * One squad, owned by USquadQuerySubsystem. A UObject because EQS needs a UObject owner for every query, and the
 * contexts must find the squad from that owner (02 section 4.1). Its member list is a UPROPERTY, so the garbage
 * collector sees it; members, the threat and the evaluation state are changed only by the subsystem.
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

	/** For tests and debug views: a shared query is running, the squad waits in the queue, how many events it has seen. */
	bool IsEvaluating() const { return RunningQuery.IsValid(); }
	bool IsQueued() const { return bQueued; }
	int32 GetGeneration() const { return Generation; }

private:
	friend class USquadQuerySubsystem;

	int32 SquadId = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USquadMemberComponent>> Members;

	FVector ThreatLocation = FVector::ZeroVector;
	bool bHasThreat = false;

	/** Bumped by every event (threat moved, member joined or left). A result started under an older value is stale. */
	int32 Generation = 0;
	/** In the subsystem's event queue (each squad at most once). */
	bool bQueued = false;

	/** The shared query in flight (its QueryID aborts it), kept so its debug data can be read when it finishes. */
	TSharedPtr<FEnvQueryInstance> RunningQuery;
	int32 QueryGeneration = 0;
	double QueryStartTime = 0.0;
};

#undef UE_API
