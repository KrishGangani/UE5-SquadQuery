// SquadQueryCore - the world's squads, and their shared, event-driven cover evaluation.

#include "SquadQuerySubsystem.h"

#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvQueryItemType_SmartObject.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SmartObjectSubsystem.h"
#include "Squad.h"
#include "SquadAssignmentSolver.h"
#include "SquadMemberComponent.h"
#include "SquadQueryWork.h"

bool USquadQuerySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USquadQuerySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USquadQuerySubsystem, STATGROUP_Tickables);
}

void USquadQuerySubsystem::RegisterMember(USquadMemberComponent& Member)
{
	TObjectPtr<USquad>& Squad = Squads.FindOrAdd(Member.GetSquadId());
	if (Squad == nullptr)
	{
		Squad = NewObject<USquad>(this);
		Squad->SquadId = Member.GetSquadId();
	}
	Squad->Members.AddUnique(&Member);
	Member.Squad = Squad;
	MarkDirty(*Squad);
}

void USquadQuerySubsystem::UnregisterMember(USquadMemberComponent& Member)
{
	// A claim must not outlive its holder (01b finding 7). No broadcast: the member is going away.
	ReleaseClaim(Member);
	Member.AssignedSlot = FSmartObjectSlotHandle();
	if (USquad* Squad = Member.Squad)
	{
		Squad->Members.Remove(&Member);
		MarkDirty(*Squad);
	}
	Member.Squad = nullptr;
}

void USquadQuerySubsystem::SetThreat(int32 SquadId, const FVector& Location)
{
	if (USquad* Squad = FindSquad(SquadId))
	{
		Squad->ThreatLocation = Location;
		Squad->bHasThreat = true;
		MarkDirty(*Squad);
	}
}

USquad* USquadQuerySubsystem::FindSquad(int32 SquadId) const
{
	const TObjectPtr<USquad>* Found = Squads.Find(SquadId);
	return Found ? Found->Get() : nullptr;
}

void USquadQuerySubsystem::SetSharedQuery(const UEnvQuery* Query)
{
	SharedQuery = Query;
	if (SharedQuery && !QueryFinishedDelegate.IsBound())
	{
		QueryFinishedDelegate = FQueryFinishedSignature::CreateUObject(this, &USquadQuerySubsystem::OnQueryFinished);
	}
	DirtyQueue.Reset();
	for (const TPair<int32, TObjectPtr<USquad>>& Entry : Squads)
	{
		if (USquad* Squad = Entry.Value)
		{
			AbortEvaluation(*Squad);
			Squad->bQueued = false;
			if (SharedQuery)
			{
				MarkDirty(*Squad);
			}
		}
	}
}

bool USquadQuerySubsystem::IsIdle() const
{
	if (!DirtyQueue.IsEmpty())
	{
		return false;
	}
	for (const TPair<int32, TObjectPtr<USquad>>& Entry : Squads)
	{
		if (Entry.Value && Entry.Value->IsEvaluating())
		{
			return false;
		}
	}
	return true;
}

void USquadQuerySubsystem::MarkDirty(USquad& Squad)
{
	// Every event invalidates a result in flight, even with evaluation off (the counter is cheap and always right).
	++Squad.Generation;
	if (SharedQuery && !Squad.bQueued)
	{
		Squad.bQueued = true;
		DirtyQueue.Add(&Squad);
	}
}

void USquadQuerySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (DirtyQueue.IsEmpty())
	{
		return;
	}
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	// First come, first served, at most MaxEvaluationsPerFrame started per frame (H6). A squad with nothing to evaluate
	// (no threat yet, no members) leaves the queue without using the cap; its next event queues it again.
	int32 Processed = 0;
	int32 Started = 0;
	while (Processed < DirtyQueue.Num() && Started < MaxEvaluationsPerFrame)
	{
		USquad* Squad = DirtyQueue[Processed++];
		if (Squad)
		{
			Squad->bQueued = false;
			Started += StartEvaluation(*Squad) ? 1 : 0;
		}
	}
	DirtyQueue.RemoveAt(0, Processed, EAllowShrinking::No);
}

bool USquadQuerySubsystem::StartEvaluation(USquad& Squad)
{
	// A query still running was asked about an older state of the squad.
	AbortEvaluation(Squad);

	UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld());
	if (SharedQuery == nullptr || Manager == nullptr || !Squad.HasThreat() || Squad.Members.IsEmpty())
	{
		return false;
	}

	// The squad owns the query, so the contexts can find it (02 section 4.1). PrepareQueryInstance + RunQuery keep the
	// instance for its debug data, exactly as the baselines do (01b findings 1, 6).
	const FEnvQueryRequest Request(SharedQuery, &Squad);
	Squad.RunningQuery = Manager->PrepareQueryInstance(Request, EEnvQueryRunMode::AllMatching);
	if (!Squad.RunningQuery.IsValid())
	{
		return false;
	}
	Squad.QueryGeneration = Squad.Generation;
	Squad.QueryStartTime = GetWorld()->GetTimeSeconds();
	Manager->RunQuery(Squad.RunningQuery, QueryFinishedDelegate);
	++Totals.QueriesStarted;
	return true;
}

void USquadQuerySubsystem::AbortEvaluation(USquad& Squad)
{
	if (!Squad.RunningQuery.IsValid())
	{
		return;
	}
	if (UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld()))
	{
		Manager->AbortQuery(Squad.RunningQuery->QueryID);   // calls OnQueryFinished at once, with an aborted result
	}
	Squad.RunningQuery.Reset();   // in case the manager no longer had it
}

void USquadQuerySubsystem::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	// Runs inside the EQS tick, but outside its budget (01b finding 5): counted here as AI time.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SquadQueryDecide);

	USquad* Squad = Result.IsValid() ? Cast<USquad>(Result->Owner.Get()) : nullptr;
	if (Squad == nullptr || !Squad->RunningQuery.IsValid() || Result->QueryID != Squad->RunningQuery->QueryID)
	{
		return;
	}

	// The work is counted even for a result that is thrown away: it was done.
	const FSquadQueryWork Work = FSquadQueryWork::FromFinishedQuery(*Squad->RunningQuery);
	Totals.ItemsGenerated += Work.ItemsGenerated;
	Totals.ItemTests += Work.ItemTests;
	const bool bCurrent = Squad->QueryGeneration == Squad->Generation;
	if (Result->IsAborted())
	{
		++Totals.QueriesAborted;
	}
	else
	{
		Totals.QueryLatencySeconds += GetWorld()->GetTimeSeconds() - Squad->QueryStartTime;
		++Totals.QueriesFinished;
	}
	Squad->RunningQuery.Reset();

	if (!bCurrent || Result->IsAborted())
	{
		// An event arrived while the query ran; that event queued the squad again (02 section 8).
		++Totals.StaleResults;
		return;
	}
	if (Result->IsSuccessful())
	{
		Assign(*Squad, *Result);
	}
	// A failed query (no hidden slot near any member) keeps the current assignment, as a baseline agent keeps its target.
}

void USquadQuerySubsystem::Assign(USquad& Squad, const FEnvQueryResult& Result)
{
	USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (SmartObjects == nullptr)
	{
		return;
	}

	// Members from the squad's current list (02 section 8): one that left during the query isn't in it.
	TArray<USquadMemberComponent*, TInlineAllocator<16>> Members;
	TArray<FVector, TInlineAllocator<16>> MemberLocations;
	for (USquadMemberComponent* Member : Squad.Members)
	{
		if (Member && Member->GetOwner())
		{
			Members.Add(Member);
			MemberLocations.Add(Member->GetOwner()->GetActorLocation());
		}
	}
	const int32 NumAgents = Members.Num();
	const int32 NumCandidates = Result.Items.Num();
	auto SlotOf = [&Result](int32 Candidate) -> const FSmartObjectSlotEQSItem&
	{
		return Result.GetItemAsTypeChecked<UEnvQueryItemType_SmartObject>(Candidate);
	};

	// The personal score: straight-line distance, plus KeepCurrentBonusCm on every slot but the member's own (the same
	// rule the baselines apply, 02 section 7; adding it to the others keeps every cost >= 0). A slot claimed by anyone
	// outside the squad is forbidden; the squad's own claims are its to move.
	TArray<float> Cost;
	Cost.SetNumUninitialized(NumAgents * NumCandidates);
	for (int32 Candidate = 0; Candidate < NumCandidates; ++Candidate)
	{
		const FSmartObjectSlotEQSItem& Item = SlotOf(Candidate);
		const bool bHeldBySquad = Members.ContainsByPredicate([&Item](const USquadMemberComponent* Member)
		{
			return Member->AssignedSlot == Item.SlotHandle;
		});
		const bool bForbidden = !bHeldBySquad && !SmartObjects->CanBeClaimed(Item.SlotHandle);
		for (int32 Agent = 0; Agent < NumAgents; ++Agent)
		{
			const bool bCurrent = Members[Agent]->AssignedSlot == Item.SlotHandle;
			Cost[Agent * NumCandidates + Candidate] = bForbidden
				? FSquadAssignmentSolver::ForbiddenCost
				: MemberSlotCost(static_cast<float>(FVector::Dist2D(MemberLocations[Agent], Item.Location)), bCurrent);
		}
	}

	TArray<int32> Choice;
	const double SolveStart = FPlatformTime::Seconds();
	FSquadAssignmentSolver::Solve(Cost, NumAgents, NumCandidates, Choice);
	Totals.SolveMicroseconds.Add(static_cast<float>((FPlatformTime::Seconds() - SolveStart) * 1e6));
	++Totals.Evaluations;

	const auto ChosenSlot = [&Choice, &SlotOf](int32 Agent)
	{
		return Choice[Agent] == INDEX_NONE ? FSmartObjectSlotHandle() : SlotOf(Choice[Agent]).SlotHandle;
	};

	// Free every claim that changes hands first, so two squadmates can swap slots.
	for (int32 Agent = 0; Agent < NumAgents; ++Agent)
	{
		if (ChosenSlot(Agent) != Members[Agent]->AssignedSlot)
		{
			ReleaseClaim(*Members[Agent]);
		}
	}

	// Then claim and notify. Assignment and claiming run in one game-thread handler after the CanBeClaimed check above,
	// so a refusal is not expected; one is counted, and the squad evaluates again next frame.
	bool bRetry = false;
	for (int32 Agent = 0; Agent < NumAgents; ++Agent)
	{
		USquadMemberComponent& Member = *Members[Agent];
		FSmartObjectSlotHandle Slot = ChosenSlot(Agent);
		if (Slot == Member.AssignedSlot)
		{
			continue;
		}
		FVector Location = FVector::ZeroVector;
		if (Slot.IsValid())
		{
			Member.Claim = SmartObjects->MarkSlotAsClaimed(Slot, ESmartObjectClaimPriority::Normal);
			if (Member.Claim.IsValid())
			{
				Location = SlotOf(Choice[Agent]).Location;
			}
			else
			{
				++Totals.ClaimFailures;
				Slot = FSmartObjectSlotHandle();
				bRetry = true;
			}
		}
		++Totals.Assignments;
		Member.SetAssignment(Slot, Location);
	}
	if (bRetry)
	{
		MarkDirty(Squad);
	}
}

void USquadQuerySubsystem::ReleaseClaim(USquadMemberComponent& Member)
{
	if (Member.Claim.IsValid())
	{
		if (USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld()))
		{
			SmartObjects->MarkSlotAsFree(Member.Claim);
		}
		Member.Claim.Invalidate();
	}
}

void USquadQuerySubsystem::Deinitialize()
{
	// Members normally leave in EndPlay, before the world tears its subsystems down. Stop any query still running and
	// clear members that didn't leave, so nothing keeps a pointer to a squad that is about to be destroyed.
	UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld());
	for (const TPair<int32, TObjectPtr<USquad>>& Entry : Squads)
	{
		if (USquad* Squad = Entry.Value)
		{
			if (Manager && Squad->RunningQuery.IsValid())
			{
				Manager->SilentlyRemoveAllQueriesByQuerier(*Squad);
			}
			Squad->RunningQuery.Reset();
			for (USquadMemberComponent* Member : Squad->Members)
			{
				if (Member)
				{
					Member->Squad = nullptr;
				}
			}
			Squad->Members.Reset();
		}
	}
	Squads.Reset();
	DirtyQueue.Reset();
	SharedQuery = nullptr;
	Super::Deinitialize();
}
