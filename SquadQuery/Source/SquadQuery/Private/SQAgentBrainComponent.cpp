// P07 SquadQuery (demo) - an agent's decision logic.

#include "SQAgentBrainComponent.h"

#include "AIController.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvQueryItemType_SmartObject.h"
#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SmartObjectSubsystem.h"
#include "SQArenaSubsystem.h"
#include "Squad.h"
#include "SquadMemberComponent.h"
#include "SquadQueryContext_Threat.h"
#include "SquadQueryWork.h"
#include "TimerManager.h"

USQAgentBrainComponent::USQAgentBrainComponent()
{
	// Only the polled baselines tick, on their poll interval; ConfigureMode enables it for them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USQAgentBrainComponent::ConfigureMode(ESQBrainMode InMode, const UEnvQuery* InAgentQuery, int32 PollSeed)
{
	Mode = InMode;
	AgentQuery = InAgentQuery;
	Squad = USquadQueryContext_Threat::ResolveSquad(GetOwner());
	PollStream.Initialize(PollSeed);
	if (AgentQuery && !QueryFinishedDelegate.IsBound())
	{
		QueryFinishedDelegate = FQueryFinishedSignature::CreateUObject(this, &USQAgentBrainComponent::OnQueryFinished);
	}

	// A seeded first poll anywhere in one interval, so the agents' polls are spread out, not in step (UE5-UpdateBudget
	// found same-interval ticks stay aligned). A newly enabled tick function always runs on the next frame, whatever its
	// interval, and a cooldown set before its first tick is ignored (TickTaskManager.cpp:2463-2477, 1681-1691); so the
	// first tick only sets the delay, and the polls start after it.
	PendingFirstPollDelay = static_cast<float>(PollStream.FRandRange(0.0, PollIntervalSeconds));
	SetComponentTickEnabled(UsesPolling());

	if (Mode == ESQBrainMode::SquadFollower)
	{
		// The squad decides and claims (02 section 9); the brain only turns the decision into movement.
		USquadMemberComponent* Member = GetOwner()->FindComponentByClass<USquadMemberComponent>();
		if (ensureMsgf(Member, TEXT("SquadFollower needs a USquadMemberComponent on %s"), *GetNameSafe(GetOwner())))
		{
			Member->OnAssignmentChangedNative.AddUObject(this, &USQAgentBrainComponent::OnAssignmentChanged);
		}
	}
}

void USQAgentBrainComponent::BeginPlay()
{
	Super::BeginPlay();
	Arena = GetWorld()->GetSubsystem<USQArenaSubsystem>();
	ResolveController();
}

void USQAgentBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Nothing may call back into a component that is going away, and a claim must not outlive its holder (01b #7).
	if (UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld()); Manager && GetOwner())
	{
		Manager->SilentlyRemoveAllQueriesByQuerier(*GetOwner());
	}
	RunningQuery.Reset();
	ReleaseClaim();
	Super::EndPlay(EndPlayReason);
}

void USQAgentBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (PendingFirstPollDelay.IsSet())
	{
		SetComponentTickInterval(PendingFirstPollDelay.GetValue());   // taken when the tick function is rescheduled
		PendingFirstPollDelay.Reset();
		return;
	}

	// A Behavior Tree service re-arms with Interval +/- RandomDeviation every time it runs (BTService.cpp:105-109).
	SetComponentTickInterval(static_cast<float>(PollIntervalSeconds + PollStream.FRandRange(-PollDeviationSeconds, PollDeviationSeconds)));
	StartQuery();   // skipped while the previous poll's query still runs
}

void USQAgentBrainComponent::OnThreatEvent()
{
	// The abort (a scan of the running queries) is decision work too, not the director's.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	if (Mode != ESQBrainMode::EventClaims)
	{
		return;
	}
	AbortQuery();   // a query still running was asked about the old threat position
	StartQuery();
}

void USQAgentBrainComponent::StartQuery()
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	if (AgentQuery == nullptr || RunningQuery.IsValid())
	{
		return;   // Scripted mode, or a query already running
	}
	const USquad* MySquad = Squad.Get();
	UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld());
	if (MySquad == nullptr || !MySquad->HasThreat() || Manager == nullptr)
	{
		return;   // no threat yet: nothing to hide from
	}

	// PrepareQueryInstance + RunQuery is what FEnvQueryRequest::Execute does (EnvQueryManager.cpp:282-286), but we keep
	// the instance, so its debug data can be read when it finishes (01b findings 1, 6).
	const FEnvQueryRequest Request(AgentQuery, GetOwner());
	RunningQuery = Manager->PrepareQueryInstance(Request, EEnvQueryRunMode::SingleResult);
	if (!RunningQuery.IsValid())
	{
		return;
	}
	Manager->RunQuery(RunningQuery, QueryFinishedDelegate);
	QueryStartTime = GetWorld()->GetTimeSeconds();
	++Counters.QueriesStarted;
}

void USQAgentBrainComponent::AbortQuery()
{
	if (!RunningQuery.IsValid())
	{
		return;
	}
	if (UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld()))
	{
		Manager->AbortQuery(RunningQuery->QueryID);   // calls OnQueryFinished at once, with an aborted result
	}
	RunningQuery.Reset();   // in case the manager no longer had it
}

void USQAgentBrainComponent::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	// Runs inside the EQS tick, but its time is excluded from the EQS budget (01b finding 5) and counted here as AI time.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	if (RunningQuery.IsValid())
	{
		const FSquadQueryWork Work = FSquadQueryWork::FromFinishedQuery(*RunningQuery);
		Counters.ItemsGenerated += Work.ItemsGenerated;
		Counters.ItemTests += Work.ItemTests;
	}
	// Latency only for completed queries; an aborted one (B2, on every event) was not waited for.
	if (Result.IsValid() && Result->IsAborted())
	{
		++Counters.QueriesAborted;
	}
	else
	{
		Counters.QueryLatencySeconds += GetWorld()->GetTimeSeconds() - QueryStartTime;
		++Counters.QueriesFinished;
	}
	RunningQuery.Reset();

	if (!Result.IsValid() || !Result->IsSuccessful() || Result->Items.Num() == 0 || Arena == nullptr)
	{
		return;   // aborted, or no hidden slot nearby: keep the current target
	}
	const FSmartObjectSlotEQSItem& Best = Result->GetItemAsTypeChecked<UEnvQueryItemType_SmartObject>(0);
	const int32 NewSlot = Arena->FindSlotIndex(Best.SlotHandle);
	if (NewSlot == INDEX_NONE || NewSlot == TargetSlot || ShouldKeepCurrent(NewSlot))
	{
		return;
	}

	if (UsesClaims())
	{
		USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld());
		const FSmartObjectClaimHandle NewClaim = SmartObjects
			? SmartObjects->MarkSlotAsClaimed(Best.SlotHandle, ESmartObjectClaimPriority::Normal)
			: FSmartObjectClaimHandle::InvalidHandle;
		if (!NewClaim.IsValid())
		{
			// Someone claimed it between generation and now (01b finding 8): ask again next frame.
			++Counters.ClaimFailures;
			GetWorld()->GetTimerManager().SetTimerForNextTick(this, &USQAgentBrainComponent::StartQuery);
			return;
		}
		ReleaseClaim();
		Claim = NewClaim;
	}
	SetTarget(NewSlot);
}

bool USQAgentBrainComponent::ShouldKeepCurrent(int32 NewSlot)
{
	// The keep-current rule of 02 section 7: a valid slot is left only for one at least KeepCurrentBonusCm closer. With
	// claims the agent's own slot never comes back from its own query (01b finding 8), so whether it is still valid has
	// to be checked: one Visibility trace from the threat, counted as an item test. The cheap comparisons come first, so
	// the trace runs only when keeping the slot is actually possible.
	const USquad* MySquad = Squad.Get();
	if (TargetSlot == INDEX_NONE || MySquad == nullptr || !MySquad->HasThreat())
	{
		return false;
	}
	++Counters.KeepChecks;
	const FVector& Current = Arena->GetSlots()[TargetSlot].Location;
	const FVector Me = GetOwner()->GetActorLocation();
	if (FVector::Dist2D(Me, Current) > FVector::Dist2D(Me, Arena->GetSlots()[NewSlot].Location) + KeepCurrentBonusCm)
	{
		return false;   // the new slot is closer by more than the bonus: switch, whatever the current one is worth
	}
	const FVector& Threat = MySquad->GetThreatLocation();
	if (!USQArenaSubsystem::IsFarFromThreat(Threat, Current))
	{
		return false;
	}
	++Counters.ItemTests;   // one trace, one item test
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(SQKeepCurrent), /*bTraceComplex*/ false);
	return USQArenaSubsystem::TraceHidden(*GetWorld(), Threat, Current, Params);
}

void USQAgentBrainComponent::ReleaseClaim()
{
	if (Claim.IsValid())
	{
		if (USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			SmartObjects->MarkSlotAsFree(Claim);
		}
		Claim.Invalidate();
	}
}

void USQAgentBrainComponent::OnAssignmentChanged(USquadMemberComponent& Member)
{
	// Called from the squad's result handler, inside its SquadQueryDecide scope (SetTarget opens its own as well). An
	// invalid slot handle (no slot for this member) maps to INDEX_NONE, which stops the agent.
	if (Arena)
	{
		SetTarget(Arena->FindSlotIndex(Member.GetAssignedSlot()));
	}
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
