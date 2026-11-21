// P07 SquadQuery (demo) - an agent's decision logic.

#include "SQAgentBrainComponent.h"

#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SQArenaSubsystem.h"
#include "TimerManager.h"

USQAgentBrainComponent::USQAgentBrainComponent()
{
	// S2: decisions arrive from outside (the director's scripted assignment). The polled baselines (S3) tick.
	PrimaryComponentTick.bCanEverTick = false;
}

void USQAgentBrainComponent::BeginPlay()
{
	Super::BeginPlay();
	Arena = GetWorld()->GetSubsystem<USQArenaSubsystem>();
	ResolveController();
}

AAIController* USQAgentBrainComponent::ResolveController()
{
	if (Controller == nullptr)
	{
		// AutoPossessAI = Spawned: the controller possesses the pawn while it is spawned, before BeginPlay. Resolved
		// lazily as well, in case a decision ever arrives first.
		const APawn* Pawn = Cast<APawn>(GetOwner());
		Controller = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		if (Controller && Controller->GetPathFollowingComponent())
		{
			Controller->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &USQAgentBrainComponent::OnMoveFinished);
		}
	}
	return Controller;
}

void USQAgentBrainComponent::SetTarget(int32 SlotIndex)
{
	// AI time (01 section 6.3) = EQS's own exclusive stat + this one + the engine's path queries started from here.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	if (SlotIndex == TargetSlot)
	{
		return;
	}

	// Oscillation: taking again the slot held before the current one, within the window. Stops are transparent, so
	// A -> none -> A and A -> none -> B -> A both count.
	const double Now = GetWorld()->GetTimeSeconds();
	if (SlotIndex != INDEX_NONE && SlotIndex == PreviousSlot && Now - PreviousSlotLeftTime <= OscillationWindowSeconds)
	{
		++Counters.Oscillations;
	}
	if (TargetSlot != INDEX_NONE)
	{
		++Counters.Switches;
		PreviousSlot = TargetSlot;
		PreviousSlotLeftTime = Now;
	}
	TargetSlot = SlotIndex;

	AAIController* AIController = ResolveController();
	if (AIController == nullptr || Arena == nullptr)
	{
		return;
	}
	if (TargetSlot == INDEX_NONE || !Arena->GetSlots().IsValidIndex(TargetSlot))
	{
		CurrentMoveId = FAIRequestID::InvalidRequest;
		AIController->StopMovement();
		return;
	}
	RequestMove(*AIController);
}

void USQAgentBrainComponent::RequestMove(AAIController& AIController)
{
	++Counters.MoveRequests;
	const EPathFollowingRequestResult::Type Result = AIController.MoveToLocation(Arena->GetSlots()[TargetSlot].Location,
		AcceptanceRadiusCm, /*bStopOnOverlap*/ false, /*bUsePathfinding*/ true, /*bProjectDestinationToNavigation*/ true,
		/*bCanStrafe*/ false);
	Counters.MoveFailures += Result == EPathFollowingRequestResult::Failed ? 1 : 0;
	CurrentMoveId = Result == EPathFollowingRequestResult::RequestSuccessful ? AIController.GetCurrentMoveRequestID() : FAIRequestID::InvalidRequest;
}

void USQAgentBrainComponent::OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// Only our current request, and only when it ended short of the slot. Success (arrived, or already there) and
	// interruptions (a new request replaced it, or StopMovement) need nothing. Retried next frame, outside path
	// following's own callback.
	if (RequestID != CurrentMoveId || Result.IsSuccess() || Result.IsInterrupted())
	{
		return;
	}
	CurrentMoveId = FAIRequestID::InvalidRequest;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &USQAgentBrainComponent::RetryMove);
}

void USQAgentBrainComponent::RetryMove()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	AAIController* AIController = ResolveController();
	if (AIController && Arena && Arena->GetSlots().IsValidIndex(TargetSlot) && !CurrentMoveId.IsValid())
	{
		++Counters.MoveRetries;
		RequestMove(*AIController);
	}
}
