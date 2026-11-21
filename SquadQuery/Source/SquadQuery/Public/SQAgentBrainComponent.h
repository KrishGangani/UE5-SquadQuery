// P07 SquadQuery (demo) - an agent's decision logic. S2: the one path from a decision to movement, plus its counters.

#pragma once

#include "AITypes.h"
#include "Components/ActorComponent.h"
#include "SQAgentBrainComponent.generated.h"

class AAIController;
class USQArenaSubsystem;
struct FPathFollowingResult;

/** What one brain counted. Plain numbers, so a copy taken at BenchStart can be subtracted from the final values. */
struct FSQBrainCounters
{
	int32 Switches = 0;        // leaving a slot, for another one or for none
	int32 Oscillations = 0;    // taking again, within OscillationWindowSeconds, the slot held before the current one
	int32 MoveRequests = 0;    // MoveToLocation calls, retries included
	int32 MoveRetries = 0;     // moves issued again because path following ended short of the slot
	int32 MoveFailures = 0;    // requests path following refused outright

	FSQBrainCounters operator-(const FSQBrainCounters& Earlier) const
	{
		FSQBrainCounters Delta;
		Delta.Switches = Switches - Earlier.Switches;
		Delta.Oscillations = Oscillations - Earlier.Oscillations;
		Delta.MoveRequests = MoveRequests - Earlier.MoveRequests;
		Delta.MoveRetries = MoveRetries - Earlier.MoveRetries;
		Delta.MoveFailures = MoveFailures - Earlier.MoveFailures;
		return Delta;
	}
};

/**
 * Turns "go to slot N" into movement, and counts what the metrics need. Every configuration ends in SetTarget()
 * (02 section 9): only how the slot is chosen differs. S2 has one chooser, the benchmark director's scripted
 * assignment (used to test the metrics); S3 adds the per-agent EQS baselines and S5 the SquadQuery follower.
 */
UCLASS()
class USQAgentBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USQAgentBrainComponent();

	/** The agent's squad, set once by the director before the agent finishes spawning. */
	void Setup(int32 InSquadIndex) { SquadIndex = InSquadIndex; }

	/**
	 * The one place a decision becomes movement. A new slot starts one MoveToLocation (whose path query runs
	 * synchronously, 01b finding 15); INDEX_NONE stops the agent; the current slot again does nothing. If path following
	 * later ends the move short of the slot (blocked, off path, invalid path), the move is issued again on the next
	 * frame, whatever chose the slot, so no configuration leaves an agent stranded.
	 */
	void SetTarget(int32 SlotIndex);

	int32 GetTargetSlot() const { return TargetSlot; }
	int32 GetSquadIndex() const { return SquadIndex; }

	/** Everything this brain counted since it spawned; the director subtracts a copy taken at BenchStart. */
	const FSQBrainCounters& GetCounters() const { return Counters; }

	/** A return to the previously held slot within this many seconds counts as an oscillation (01 section 6.3). */
	static constexpr double OscillationWindowSeconds = 3.0;

	/** How close MoveToLocation must get (cm). The "in cover" test is a little looser, because capsules jostle. */
	static constexpr float AcceptanceRadiusCm = 50.f;

protected:
	virtual void BeginPlay() override;

private:
	/** The agent's AI controller, cached; bound to its path following's finished event on first use. */
	AAIController* ResolveController();

	/** One MoveToLocation to the target slot; remembers its request id and counts it. */
	void RequestMove(AAIController& AIController);

	/** Path following finished a request: if it was ours and ended short of the slot, retry next frame. */
	void OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void RetryMove();

	UPROPERTY(Transient)
	TObjectPtr<USQArenaSubsystem> Arena;

	UPROPERTY(Transient)
	TObjectPtr<AAIController> Controller;

	FAIRequestID CurrentMoveId;

	int32 SquadIndex = INDEX_NONE;

	int32 TargetSlot = INDEX_NONE;
	int32 PreviousSlot = INDEX_NONE;     // the slot held before TargetSlot (stops in between ignored)
	double PreviousSlotLeftTime = -1.0;  // world time it was left

	FSQBrainCounters Counters;
};
