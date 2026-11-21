// P07 SquadQuery (demo) - an agent's decision logic: the per-agent EQS baselines, and the one path to movement.

#pragma once

#include "AITypes.h"
#include "Components/ActorComponent.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Math/RandomStream.h"
#include "Misc/Optional.h"
#include "SmartObjectRuntime.h"
#include "SQAgentBrainComponent.generated.h"

class AAIController;
class UEnvQuery;
class USQArenaSubsystem;
class USquad;
class USquadMemberComponent;
struct FPathFollowingResult;

/** How an agent chooses its cover slot (01 section 6.2). */
enum class ESQBrainMode : uint8
{
	Scripted,       // the director assigns slots (a metrics test oracle, S2)
	PolledEQS,      // B0: own SingleResult query every 0.5 s +/- 0.1 s, no reservation
	PolledClaims,   // B1: B0 + a Smart Object claim of the pick
	EventClaims,    // B2: own query after a threat event (or a failed claim), + claims
	SquadFollower,  // SQ: moves to the slot its squad assigns (USquadMemberComponent::OnAssignmentChangedNative)
};

/** What one brain counted since the director last reset it (at BenchStart). */
struct FSQBrainCounters
{
	int32 Switches = 0;          // leaving a slot, for another one or for none
	int32 Oscillations = 0;      // taking again, within OscillationWindowSeconds, the slot held before the current one
	int32 MoveRequests = 0;      // MoveToLocation calls, retries included
	int32 MoveRetries = 0;       // moves issued again because path following ended short of the slot
	int32 MoveFailures = 0;      // requests path following refused outright
	int32 QueriesStarted = 0;    // per-agent EQS modes only
	int32 QueriesFinished = 0;   // completed (success or no result); aborted ones are counted apart
	int32 QueriesAborted = 0;    // B2 aborts a running query on every threat event
	int64 ItemsGenerated = 0;    // from EQS debug data: 0 without it (01b finding 6)
	int64 ItemTests = 0;         // the same, plus one per keep-current trace
	int32 KeepChecks = 0;
	int32 ClaimFailures = 0;
	double QueryLatencySeconds = 0.0;   // summed over completed queries, world time
};

/**
 * Turns a choice of slot into movement, and counts what the metrics need. Every configuration ends in SetTarget()
 * (02 section 9); only how the slot is chosen differs. The baselines run the engine's EQS exactly as a game would:
 * one query per agent, owned by the agent (01b finding 1), with the keep-current rule of 02 section 7.
 */
UCLASS()
class USQAgentBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USQAgentBrainComponent();

	/**
	 * Chooses the mode; for the per-agent EQS modes also the query template (shared by all agents) and the poll seed;
	 * SquadFollower binds to the owner's USquadMemberComponent. Call after the agent has finished spawning: its squad
	 * membership and tick function must be registered.
	 */
	void ConfigureMode(ESQBrainMode InMode, const UEnvQuery* InAgentQuery, int32 PollSeed);

	/** The threat moved. B2 queries now; the polled modes notice at their next poll; Scripted ignores it. */
	void OnThreatEvent();

	/**
	 * The one place a decision becomes movement. A new slot starts one MoveToLocation (whose path query runs
	 * synchronously, 01b finding 15); INDEX_NONE stops the agent; the current slot again does nothing. If path following
	 * later ends the move short of the slot (blocked, off path, invalid path), the move is issued again on the next
	 * frame, whatever chose the slot, so no configuration leaves an agent stranded.
	 */
	void SetTarget(int32 SlotIndex);

	int32 GetTargetSlot() const { return TargetSlot; }

	/** Everything this brain counted since the last reset. The director resets every brain at BenchStart. */
	const FSQBrainCounters& GetCounters() const { return Counters; }
	void ResetCounters() { Counters = FSQBrainCounters(); }

	static constexpr double OscillationWindowSeconds = 3.0;   // 01 section 6.3
	static constexpr float AcceptanceRadiusCm = 50.f;         // MoveToLocation; "in cover" is a little looser (02 section 7)
	static constexpr double PollIntervalSeconds = 0.5;        // the engine's BT service default (01b finding 16)
	static constexpr double PollDeviationSeconds = 0.1;
	static constexpr float KeepCurrentBonusCm = 300.f;        // 02 section 7: a valid slot is left only for one 3 m closer

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool UsesPolling() const { return Mode == ESQBrainMode::PolledEQS || Mode == ESQBrainMode::PolledClaims; }
	bool UsesClaims() const { return Mode == ESQBrainMode::PolledClaims || Mode == ESQBrainMode::EventClaims; }

	/** The agent's AI controller, cached; bound to its path following's finished event on first use. */
	AAIController* ResolveController();
	void RequestMove(AAIController& AIController);
	void OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void RetryMove();

	void StartQuery();
	void AbortQuery();
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);
	bool ShouldKeepCurrent(int32 NewSlot);
	void ReleaseClaim();
	void OnAssignmentChanged(USquadMemberComponent& Member);

	UPROPERTY(Transient)
	TObjectPtr<USQArenaSubsystem> Arena;

	UPROPERTY(Transient)
	TObjectPtr<AAIController> Controller;

	/** The per-agent query template; null in Scripted mode. */
	UPROPERTY(Transient)
	TObjectPtr<const UEnvQuery> AgentQuery;

	/** The agent's squad (for its threat), cached once in ConfigureMode; squads live as long as the world. */
	TWeakObjectPtr<const USquad> Squad;

	/** Bound once: the engine's own EQS nodes keep their finish delegate as a member too (BTTask_RunEQSQuery.cpp:29). */
	FQueryFinishedSignature QueryFinishedDelegate;

	ESQBrainMode Mode = ESQBrainMode::Scripted;
	FRandomStream PollStream;

	/** Seeded delay before the first poll, applied by the first tick (the only point where an interval sticks). */
	TOptional<float> PendingFirstPollDelay;

	/** The running query, kept so its debug data can be read when it finishes (FSquadQueryWork). Its QueryID aborts it. */
	TSharedPtr<FEnvQueryInstance> RunningQuery;
	double QueryStartTime = 0.0;

	FSmartObjectClaimHandle Claim;
	FAIRequestID CurrentMoveId;

	int32 TargetSlot = INDEX_NONE;
	int32 PreviousSlot = INDEX_NONE;     // the slot held before TargetSlot (stops in between ignored)
	double PreviousSlotLeftTime = -1.0;  // world time it was left

	FSQBrainCounters Counters;
};
