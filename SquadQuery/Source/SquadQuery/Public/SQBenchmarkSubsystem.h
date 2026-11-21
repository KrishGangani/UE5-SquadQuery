// P07 SquadQuery (demo) - benchmark director: spawns the squads, runs the threat timeline, measures decision quality.

#pragma once

#include "Containers/BitArray.h"
#include "SQAgentBrainComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "SQBenchmarkSubsystem.generated.h"

class ACameraActor;
class ASQAgent;
class AStaticMeshActor;
class UEnvQuery;
class USQArenaSubsystem;

/**
 * Runs one timed, repeatable benchmark in a game world. A world subsystem for the same reason as the arena: there is
 * no authored map to place a director in. Tickable, because the threat timeline and the metrics run every frame.
 *
 * Timeline (world time from the moment the squads spawn, which is when the arena reports ready):
 *   events      the threat relocates every EventInterval seconds, cycling through the arena's 10 threat positions.
 *               Captured events fall at BenchStart + EventOffsetSeconds + k * EventInterval, k = 0 .. 9, so the
 *               60 s capture holds 10 events; earlier events (warm-up) get the agents into cover once first
 *   Warmup      CSV_EVENT "BenchStart" -> capture starts (-csvStartOnEvent=BenchStart)
 *   + Capture   CSV_EVENT "BenchEnd"   -> capture stops (-csvStopOnEvent=BenchEnd); the next frame writes the run
 *               report; with -SQExit the game quits a few frames later
 *
 * Metrics (01 section 6.3), computed from per-agent fields and the arena's precomputed visibility table, so they
 * never trace. They run in their own CSV scope (SQBench/Metrics), outside the AI time:
 *   in cover        the agent's target slot is valid cover for the current threat (USQArenaSubsystem::IsValidCover)
 *                   and the agent stands within InCoverRadiusCm of it
 *   time to cover   per agent per captured event: event -> first frame in cover; -1 if not in cover before the next
 *                   event (or the end of the capture); with the straight-line distance walked
 *   conflicts       onsets of "two or more agents target the same slot"
 *   switches, oscillations, move requests   from the brains, counted over the capture
 *
 * Command line: -SQMode=Scripted|B0|B1|B2|SQ, -SQAgents=N (default 0: no run),
 * -SQWarmup=SEC (10), -SQCapture=SEC (60), -SQEventInterval=SEC (6, at least 1), -SQRunTag=NAME (run), -SQExit.
 * The seed is the arena's (-SQSeed=S).
 */
UCLASS()
class USQBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	/** The agent stands this close (cm, horizontally) to a valid slot to count as in cover. */
	static constexpr float InCoverRadiusCm = 75.f;

	/** Squad size of 01 section 6.1. */
	static constexpr int32 SquadSize = 8;

	/** Captured events start this long after BenchStart, so the BenchStart frame and the first event's frame differ. */
	static constexpr double EventOffsetSeconds = 0.25;

protected:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	struct FRunSettings
	{
		FString Mode = TEXT("Scripted");
		int32 NumAgents = 0;
		double WarmupSeconds = 10.0;
		double CaptureSeconds = 60.0;
		double EventIntervalSeconds = 6.0;
		FString RunTag = TEXT("run");
		bool bExitWhenDone = false;
	};

	enum class ERunPhase : uint8 { Idle, WaitingForArena, WarmUp, Capture, Done };

	/** Per-agent measurement state, parallel to Agents. */
	struct FAgentMetrics
	{
		int32 AwaitingEvent = INDEX_NONE;                // the event the agent is still getting into cover for
		FVector PositionAtEvent = FVector::ZeroVector;   // where it stood when that event fired
	};

	/** How one agent answered one event: seconds to cover, and the straight-line distance from where it stood at the
	 *  event to the slot it reached. Both -1 if it wasn't in cover before the window closed. Distance / walking speed
	 *  is the fastest possible time, so the two together show decision delay plus path detours. */
	struct FArrival
	{
		float Seconds = -1.f;
		float DistanceCm = -1.f;
	};

	/** One threat event: which position, when, whether it was captured, and every agent's arrival. */
	struct FEventRecord
	{
		int32 ThreatIndex = INDEX_NONE;
		double Time = 0.0;
		bool bCaptured = false;
		TArray<FArrival> Arrivals;
	};

	static FRunSettings ReadRunSettings();
	/** -SQMode value -> brain mode; false for an unknown value. */
	static bool ParseMode(const FString& Mode, ESQBrainMode& OutMode);
	bool ValidateRunSettings();
	void HandleArenaReady();
	void StartRun(UWorld& World);
	void SpawnSquads(UWorld& World);
	void FireThreatEvent(double Now);
	void AssignScripted();
	void UpdateMetrics(double Now);
	bool IsSlotValidCover(int32 SlotIndex) const;
	bool IsInCover(int32 AgentIndex) const;
	void WriteRunReport() const;

	FRunSettings Settings;
	ESQBrainMode BrainMode = ESQBrainMode::Scripted;
	ERunPhase Phase = ERunPhase::Idle;

	/** The per-agent cover query (B0-B2), one template shared by every agent (EQS caches it, 01b finding 4). */
	UPROPERTY(Transient)
	TObjectPtr<UEnvQuery> AgentQuery;

	/** The squad cover query (SQ), handed to the squad subsystem. */
	UPROPERTY(Transient)
	TObjectPtr<UEnvQuery> SharedSquadQuery;

	UPROPERTY(Transient)
	TObjectPtr<USQArenaSubsystem> Arena;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASQAgent>> Agents;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> ThreatMarker;

	TArray<FAgentMetrics> AgentMetrics;
	TArray<FEventRecord> Events;

	/** Per slot, how many agents target it this frame (reset every frame), and whether it was in conflict last frame. */
	TArray<uint8> SlotTargetCounts;
	TBitArray<> SlotInConflict;

	double StartTime = 0.0;
	double BenchStartTime = 0.0;
	double NextEventTime = 0.0;
	int32 CurrentThreatIndex = INDEX_NONE;
	int32 FramesSinceEnd = 0;

	// Capture totals.
	int32 CapturedEvents = 0;
	int32 CapturedFrames = 0;
	int32 ConflictOnsets = 0;
	int64 ConflictAgentFrames = 0;
	int64 InCoverAgentFrames = 0;
};
