// SquadQueryCore - the world's squads, and their shared, event-driven cover evaluation.

#pragma once

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SquadQuerySubsystem.generated.h"

#define UE_API SQUADQUERYCORE_API

class UEnvQuery;
class USquad;
class USquadMemberComponent;

/** What the shared evaluation did since the last reset (the benchmark's run report reads it). */
struct FSquadQueryTotals
{
	int32 QueriesStarted = 0;
	int32 QueriesFinished = 0;   // completed (success or no result)
	int32 QueriesAborted = 0;    // replaced by a newer event before they finished
	/** Results thrown away because an event arrived while the query ran (02 section 8). */
	int32 StaleResults = 0;
	int64 ItemsGenerated = 0;
	int64 ItemTests = 0;
	/** Solver runs, and members whose slot changed. */
	int32 Evaluations = 0;
	int32 Assignments = 0;
	/** Claims the subsystem tried and the Smart Object subsystem refused. Expected 0 (02 section 8). */
	int32 ClaimFailures = 0;
	double QueryLatencySeconds = 0.0;   // summed over completed queries, world time
	/** One sample per solve, in microseconds, for the p99 of SC7. */
	TArray<float> SolveMicroseconds;
};

/**
 * Owns every squad of a world. The registry and each squad's threat are read by every configuration (the per-agent
 * baselines through USquadQueryContext_Threat). With a shared query set (SQ mode), it also evaluates cover once per
 * squad on events: an event marks the squad dirty; Tick starts at most MaxEvaluationsPerFrame shared queries; each
 * result becomes a cost matrix, the solver assigns slots jointly, and the subsystem claims them and tells the members
 * (02 sections 2 and 6).
 */
UCLASS(MinimalAPI)
class USquadQuerySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Called by a member in BeginPlay: creates its squad on first use and adds it. An event for that squad. */
	UE_API void RegisterMember(USquadMemberComponent& Member);

	/** Called by a member in EndPlay: frees its claim. Empty squads are kept, so their id stays valid. An event. */
	UE_API void UnregisterMember(USquadMemberComponent& Member);

	/** Tells a squad where its threat is. An event. */
	UE_API void SetThreat(int32 SquadId, const FVector& Location);

	/** The squad with this id, or null if no member ever joined it. */
	UE_API USquad* FindSquad(int32 SquadId) const;

	const TMap<int32, TObjectPtr<USquad>>& GetSquads() const { return Squads; }

	/**
	 * Turns the shared evaluation on (SQ mode) with this query, or off with null (the baselines). The query must
	 * generate Smart Object slots (UEnvQueryItemType_SmartObject); it is run in AllMatching mode, owned by the squad.
	 */
	UE_API void SetSharedQuery(const UEnvQuery* Query);

	const FSquadQueryTotals& GetTotals() const { return Totals; }
	void ResetTotals() { Totals = FSquadQueryTotals(); }

	/** True when no squad waits in the queue and no shared query runs (tests wait for it). */
	UE_API bool IsIdle() const;

	/** How many squad evaluations may start in one frame (02 section 7; a setting from S6). */
	static constexpr int32 MaxEvaluationsPerFrame = 4;
	/** A member keeps its current slot unless another is at least this much closer (02 section 7; a setting from S6). */
	static constexpr float KeepCurrentBonusCm = 300.f;

	/**
	 * The solver's cost of giving a member a slot: the straight-line distance, plus KeepCurrentBonusCm unless it is the
	 * member's current slot. For a squad of one this picks what the per-agent keep-current rule picks (the test
	 * SquadQuery.Squad.SameDecisionAsPerAgent checks it).
	 */
	static float MemberSlotCost(float DistanceCm, bool bCurrentSlot) { return DistanceCm + (bCurrentSlot ? 0.f : KeepCurrentBonusCm); }

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only. */
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	UE_API virtual void Deinitialize() override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	UE_API virtual void Tick(float DeltaTime) override;
	UE_API virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

private:
	/** An event: bumps the squad's generation and queues it once. */
	void MarkDirty(USquad& Squad);
	bool StartEvaluation(USquad& Squad);
	void AbortEvaluation(USquad& Squad);
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);
	void Assign(USquad& Squad, const FEnvQueryResult& Result);
	void ReleaseClaim(USquadMemberComponent& Member);

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USquad>> Squads;

	UPROPERTY(Transient)
	TObjectPtr<const UEnvQuery> SharedQuery;

	/** Dirty squads in the order their first event arrived. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USquad>> DirtyQueue;

	/** Bound once (as the brains do): one finish delegate for every shared query. */
	FQueryFinishedSignature QueryFinishedDelegate;

	FSquadQueryTotals Totals;
};

#undef UE_API
