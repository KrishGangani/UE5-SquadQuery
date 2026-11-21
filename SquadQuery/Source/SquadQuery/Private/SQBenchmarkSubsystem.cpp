// P07 SquadQuery (demo) - USQBenchmarkSubsystem.

#include "SQBenchmarkSubsystem.h"

#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SQAgent.h"
#include "SQArenaSubsystem.h"
#include "SQQueries.h"
#include "Squad.h"
#include "SquadMemberComponent.h"
#include "SquadQuerySubsystem.h"
#include "Stats/Stats.h"

DEFINE_LOG_CATEGORY_STATIC(LogSQBench, Log, All);

// The run's events (BenchStart, Threat, BenchEnd), per-frame metric counts, and the metrics' own timing scope.
CSV_DEFINE_CATEGORY(SQBench, true);

namespace SQBench
{
	// The configuration names of 01 section 6.2.
	struct FModeName
	{
		const TCHAR* Name;
		ESQBrainMode Mode;
	};
	constexpr FModeName Modes[] = {
		{ TEXT("Scripted"), ESQBrainMode::Scripted },
		{ TEXT("B0"), ESQBrainMode::PolledEQS },
		{ TEXT("B1"), ESQBrainMode::PolledClaims },
		{ TEXT("B2"), ESQBrainMode::EventClaims },
		{ TEXT("SQ"), ESQBrainMode::SquadFollower },
	};

	constexpr float SquadRingRadiusCm = 250.f;         // members start on a ring around their squad centre
	constexpr float SquadSpacingCm = 1400.f;           // squad centres at least this far apart
	constexpr float SpawnRangeCm = 5000.f;             // squad centres inside +/- 50 m
	constexpr float SpawnProjectionExtentCm = 10.f;    // a ring point must lie on the navmesh within 10 cm (open floor)
	constexpr float CameraHeightCm = 10000.f;          // top-down view of the whole arena
	constexpr int32 ExitDelayFrames = 3;               // frames after BenchEnd before quitting, so the CSV file is written
	constexpr double MinEventIntervalSeconds = 1.0;    // shortest accepted -SQEventInterval
}

bool USQBenchmarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

USQBenchmarkSubsystem::FRunSettings USQBenchmarkSubsystem::ReadRunSettings()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FRunSettings Read;
	FParse::Value(CommandLine, TEXT("SQMode="), Read.Mode);
	FParse::Value(CommandLine, TEXT("SQAgents="), Read.NumAgents);
	FParse::Value(CommandLine, TEXT("SQWarmup="), Read.WarmupSeconds);
	FParse::Value(CommandLine, TEXT("SQCapture="), Read.CaptureSeconds);
	FParse::Value(CommandLine, TEXT("SQEventInterval="), Read.EventIntervalSeconds);
	FParse::Value(CommandLine, TEXT("SQRunTag="), Read.RunTag);
	Read.bExitWhenDone = FParse::Param(CommandLine, TEXT("SQExit"));
	return Read;
}

bool USQBenchmarkSubsystem::ParseMode(const FString& Mode, ESQBrainMode& OutMode)
{
	for (const SQBench::FModeName& Entry : SQBench::Modes)
	{
		if (Mode == Entry.Name)
		{
			OutMode = Entry.Mode;
			return true;
		}
	}
	return false;
}

bool USQBenchmarkSubsystem::ValidateRunSettings()
{
	if (!ParseMode(Settings.Mode, BrainMode))
	{
		FString Known;
		for (const SQBench::FModeName& Entry : SQBench::Modes)
		{
			Known += (Known.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(Entry.Name);
		}
		UE_LOG(LogSQBench, Error, TEXT("Unknown -SQMode=%s (this build supports: %s)."), *Settings.Mode, *Known);
		return false;
	}
	// At most one event fires per frame, and agents need time to move between events: 1 s is a floor, not a target.
	if (Settings.WarmupSeconds < 0.0 || Settings.CaptureSeconds <= 0.0 || Settings.EventIntervalSeconds < SQBench::MinEventIntervalSeconds)
	{
		UE_LOG(LogSQBench, Error, TEXT("Invalid timing: warm-up %.3f s, capture %.3f s, event interval %.3f s (at least %.1f s)."),
			Settings.WarmupSeconds, Settings.CaptureSeconds, Settings.EventIntervalSeconds, SQBench::MinEventIntervalSeconds);
		return false;
	}
	return true;
}

void USQBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Settings = ReadRunSettings();
	if (Settings.NumAgents <= 0)
	{
		return; // No run requested: the world is just the arena.
	}
	if (!ValidateRunSettings())
	{
		return;
	}

	Arena = InWorld.GetSubsystem<USQArenaSubsystem>();
	if (Arena == nullptr)
	{
		UE_LOG(LogSQBench, Error, TEXT("No arena subsystem: nothing to run in."));
		return;
	}

	Phase = ERunPhase::WaitingForArena;
	if (Arena->IsReady())
	{
		StartRun(InWorld);
	}
	else
	{
		Arena->OnArenaReady.AddUObject(this, &USQBenchmarkSubsystem::HandleArenaReady);
	}
}

void USQBenchmarkSubsystem::HandleArenaReady()
{
	if (Phase == ERunPhase::WaitingForArena)
	{
		StartRun(*GetWorld());
	}
}

void USQBenchmarkSubsystem::StartRun(UWorld& World)
{
	if (BrainMode == ESQBrainMode::SquadFollower)
	{
		// SquadQuery: one shared query per squad, evaluated by the subsystem on events (02 sections 2 and 6).
		if (USquadQuerySubsystem* Squads = World.GetSubsystem<USquadQuerySubsystem>())
		{
			SharedSquadQuery = SQQueries::MakeSquadCoverQuery(*this);
			Squads->SetSharedQuery(SharedSquadQuery);
		}
	}
	else if (BrainMode != ESQBrainMode::Scripted)
	{
		AgentQuery = SQQueries::MakeAgentCoverQuery(*this);
	}
	SpawnSquads(World);
	AgentMetrics.SetNum(Agents.Num());
	SlotTargetCounts.SetNumZeroed(Arena->GetSlots().Num());
	SlotInConflict.Init(false, Arena->GetSlots().Num());
	Events.Reserve(FMath::CeilToInt32((Settings.WarmupSeconds + Settings.CaptureSeconds) / Settings.EventIntervalSeconds) + 2);

	// One fixed top-down view. The game mode gives the player no pawn (ASQGameMode), so nothing is ever possessed that
	// could take the view back; with auto-management off, the camera stays the view target for the whole run.
	Camera = World.SpawnActor<ACameraActor>(FVector(0.f, 0.f, SQBench::CameraHeightCm), FRotator(-90.f, 0.f, 0.f));
	if (APlayerController* PlayerController = World.GetFirstPlayerController())
	{
		PlayerController->bAutoManageActiveCameraTarget = false;
		PlayerController->SetViewTarget(Camera);
	}

	// The threat marker is only for the viewer: no collision, never part of the navmesh.
	const FTransform MarkerTransform(FVector(0.f, 0.f, -1000.f));
	ThreatMarker = World.SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), MarkerTransform);
	if (ThreatMarker)
	{
		UStaticMeshComponent* Mesh = ThreatMarker->GetStaticMeshComponent();
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetCanEverAffectNavigation(false);
		ThreatMarker->FinishSpawning(MarkerTransform);
	}

	// Captured events at BenchStart + offset + k * interval; warm-up events step back from the first one.
	StartTime = World.GetTimeSeconds();
	const double FirstCapturedEvent = Settings.WarmupSeconds + EventOffsetSeconds;
	NextEventTime = StartTime + FirstCapturedEvent
		- FMath::FloorToDouble(FirstCapturedEvent / Settings.EventIntervalSeconds) * Settings.EventIntervalSeconds;
	Phase = ERunPhase::WarmUp;

	// Run description inside the capture itself, so every CSV is self-describing.
	CSV_METADATA(TEXT("SQRunTag"), *Settings.RunTag);
	CSV_METADATA(TEXT("SQMode"), *Settings.Mode);
	CSV_METADATA(TEXT("SQAgents"), *FString::FromInt(Agents.Num()));
	CSV_METADATA(TEXT("SQSeed"), *FString::FromInt(Arena->GetSeed()));
	CSV_METADATA(TEXT("SQArenaHash"), *FString::Printf(TEXT("0x%08X"), Arena->GetArenaHash()));
}

void USQBenchmarkSubsystem::SpawnSquads(UWorld& World)
{
	// A stream of its own (seed + 1000): the agent layout depends on the seed, but not the other way round.
	FRandomStream Stream(Arena->GetSeed() + 1000);
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
	const int32 NumSquads = FMath::DivideAndRoundUp(Settings.NumAgents, SquadSize);
	// The capsule's half-height from the class default (Character.cpp:263-269); the simple-collision getters return 0
	// on a default object, whose capsule never registered (Character.cpp:234-239). + 2 cm: rests on the floor.
	const float SpawnHeight = GetDefault<ASQAgent>()->GetDefaultHalfHeight() + 2.f;

	uint32 Crc = 0;
	TArray<FVector> Centers;
	TArray<FVector, TInlineAllocator<SquadSize>> Ring;
	for (int32 Attempt = 0; Attempt < 10000 && Centers.Num() < NumSquads; ++Attempt)
	{
		const float X = Stream.FRandRange(-SQBench::SpawnRangeCm, SQBench::SpawnRangeCm);
		const float Y = Stream.FRandRange(-SQBench::SpawnRangeCm, SQBench::SpawnRangeCm);
		const FVector Center(X, Y, 0.f);
		if (Centers.ContainsByPredicate([&Center](const FVector& Other)
			{ return FVector::DistSquared2D(Center, Other) < FMath::Square(SQBench::SquadSpacingCm); }))
		{
			continue;
		}

		// Every member's start point must lie on open floor: the navmesh within a few centimetres of it.
		Ring.Reset();
		const int32 Members = FMath::Min(SquadSize, Settings.NumAgents - Centers.Num() * SquadSize);
		for (int32 Member = 0; Member < Members; ++Member)
		{
			const float Angle = 2.f * UE_PI * Member / SquadSize;
			const FVector Point = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * SQBench::SquadRingRadiusCm;
			FNavLocation OnNavmesh;
			if (NavSys == nullptr || !NavSys->ProjectPointToNavigation(Point, OnNavmesh,
				FVector(SQBench::SpawnProjectionExtentCm, SQBench::SpawnProjectionExtentCm, 200.f)))
			{
				break;
			}
			Ring.Add(OnNavmesh.Location);
		}
		if (Ring.Num() != Members)
		{
			continue;
		}

		const int32 SquadIndex = Centers.Add(Center);
		for (const FVector& Point : Ring)
		{
			const FTransform SpawnTransform(Point + FVector(0.f, 0.f, SpawnHeight));
			ASQAgent* Agent = World.SpawnActorDeferred<ASQAgent>(ASQAgent::StaticClass(), SpawnTransform, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
			if (Agent == nullptr)
			{
				UE_LOG(LogSQBench, Error, TEXT("Spawning agent %d failed; the run is incomplete."), Agents.Num());
				continue;
			}
			Agent->GetSquadMember()->SetSquadId(SquadIndex);   // before BeginPlay, which registers it with its squad
			Agent->FinishSpawning(SpawnTransform);
			// After FinishSpawning: the seeded poll phase needs a registered tick function.
			const uint32 PollSeed = HashCombineFast(GetTypeHash(Arena->GetSeed()), GetTypeHash(Agents.Num()));   // per agent, from the seed
			Agent->GetBrain()->ConfigureMode(BrainMode, AgentQuery, static_cast<int32>(PollSeed));
			Agents.Add(Agent);
			Crc = FCrc::TypeCrc32(Point, Crc);
		}
	}

	UE_CLOG(Centers.Num() < NumSquads, LogSQBench, Error, TEXT("Only %d of %d squads found room."), Centers.Num(), NumSquads);
	UE_LOG(LogSQBench, Display, TEXT("Spawned %d agents in %d squads (seed %d); start-position hash 0x%08X"),
		Agents.Num(), Centers.Num(), Arena->GetSeed(), Crc);
}

void USQBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Phase == ERunPhase::Idle || Phase == ERunPhase::WaitingForArena)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	if (Phase == ERunPhase::WarmUp && Now - StartTime >= Settings.WarmupSeconds)
	{
		BenchStartTime = Now;
		Phase = ERunPhase::Capture;
		for (int32 Index = 0; Index < Agents.Num(); ++Index)
		{
			if (Agents[Index])
			{
				Agents[Index]->GetBrain()->ResetCounters();   // from here the counters are the capture's
			}
		}
		if (USquadQuerySubsystem* Squads = GetWorld()->GetSubsystem<USquadQuerySubsystem>())
		{
			Squads->ResetTotals();   // the squad evaluation's counters too
		}
		// A conflict already running when the capture starts is a capture conflict too: forget the warm-up's state so
		// its first captured frame counts as an onset.
		SlotInConflict.Init(false, SlotInConflict.Num());
		CSV_EVENT(SQBench, TEXT("BenchStart"));
		UE_LOG(LogSQBench, Display, TEXT("BenchStart at %.3f s (warm-up %.1f s)"), Now, Settings.WarmupSeconds);
	}

	if (Phase == ERunPhase::WarmUp || Phase == ERunPhase::Capture)
	{
		if (Now >= NextEventTime)
		{
			FireThreatEvent(Now);
			NextEventTime += Settings.EventIntervalSeconds;
		}
		UpdateMetrics(Now);
	}

	if (Phase == ERunPhase::Capture && Now - BenchStartTime >= Settings.CaptureSeconds)
	{
		// The stop request is processed at the end of this frame; the report is written next frame, outside the capture.
		CSV_EVENT(SQBench, TEXT("BenchEnd"));
		Phase = ERunPhase::Done;
		FramesSinceEnd = 0;
		const int64 AgentFrames = int64(CapturedFrames) * Agents.Num();
		UE_LOG(LogSQBench, Display, TEXT("BenchEnd after %.3f s of capture: %d captured events, %d conflict onsets, in cover %.1f%% of agent-frames"),
			Now - BenchStartTime, CapturedEvents, ConflictOnsets, AgentFrames > 0 ? 100.0 * InCoverAgentFrames / AgentFrames : 0.0);
	}
	else if (Phase == ERunPhase::Done)
	{
		++FramesSinceEnd;
		if (FramesSinceEnd == 1)
		{
			WriteRunReport();
		}
		if (Settings.bExitWhenDone && FramesSinceEnd == SQBench::ExitDelayFrames)
		{
			FPlatformMisc::RequestExit(false, TEXT("USQBenchmarkSubsystem"));
		}
	}
}

void USQBenchmarkSubsystem::FireThreatEvent(double Now)
{
	const int32 EventIndex = Events.Num();
	FEventRecord& Event = Events.AddDefaulted_GetRef();
	Event.ThreatIndex = EventIndex % Arena->GetThreatPositions().Num();
	Event.Time = Now;
	Event.bCaptured = (Phase == ERunPhase::Capture);
	Event.Arrivals.SetNum(Agents.Num());   // -1 / -1 stays unless the agent reaches cover in this window
	CapturedEvents += Event.bCaptured ? 1 : 0;

	CurrentThreatIndex = Event.ThreatIndex;
	const FVector ThreatLocation = Arena->GetThreatPositions()[CurrentThreatIndex];
	if (ThreatMarker)
	{
		ThreatMarker->SetActorLocation(ThreatLocation + FVector(0.f, 0.f, USQArenaSubsystem::ThreatEyeHeightCm));
	}
	CSV_EVENT(SQBench, TEXT("Threat"));

	// A new event closes every open window: an agent still waiting for the previous one keeps its -1.
	for (int32 Index = 0; Index < AgentMetrics.Num(); ++Index)
	{
		AgentMetrics[Index].AwaitingEvent = EventIndex;
		AgentMetrics[Index].PositionAtEvent = Agents[Index] ? Agents[Index]->GetActorLocation() : FVector::ZeroVector;
	}

	// Every configuration learns of the threat the same way: each squad is told (01 section 6.2). B0 and B1 notice at
	// their next poll; B2 queries now; SQ (S5) queues a squad evaluation.
	if (USquadQuerySubsystem* Squads = GetWorld()->GetSubsystem<USquadQuerySubsystem>())
	{
		for (const TPair<int32, TObjectPtr<USquad>>& Entry : Squads->GetSquads())
		{
			Squads->SetThreat(Entry.Key, ThreatLocation);
		}
	}
	if (BrainMode == ESQBrainMode::EventClaims)
	{
		for (ASQAgent* Agent : Agents)
		{
			if (Agent)
			{
				Agent->GetBrain()->OnThreatEvent();
			}
		}
	}
	else if (BrainMode == ESQBrainMode::Scripted)
	{
		AssignScripted();
	}
}

bool USQBenchmarkSubsystem::IsSlotValidCover(int32 SlotIndex) const
{
	return Arena->IsValidCover(CurrentThreatIndex, SlotIndex);
}

bool USQBenchmarkSubsystem::IsInCover(int32 AgentIndex) const
{
	const ASQAgent* Agent = Agents[AgentIndex];
	if (Agent == nullptr)
	{
		return false;
	}
	const int32 Slot = Agent->GetBrain()->GetTargetSlot();
	return IsSlotValidCover(Slot)
		&& FVector::DistSquared2D(Agent->GetActorLocation(), Arena->GetSlots()[Slot].Location) <= FMath::Square(InCoverRadiusCm);
}

void USQBenchmarkSubsystem::AssignScripted()
{
	// A test oracle, not a configuration: it knows the precomputed visibility table, so its decisions cost no queries.
	// Pass 1 keeps every still-valid target; pass 2 gives each remaining agent (in index order) the nearest free valid slot.
	const TArray<FSQCoverSlot>& Slots = Arena->GetSlots();
	TBitArray<> Taken(false, Slots.Num());
	TArray<int32> Choice;
	Choice.Init(INDEX_NONE, Agents.Num());

	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const int32 Current = Agents[Index] ? Agents[Index]->GetBrain()->GetTargetSlot() : INDEX_NONE;
		if (Current != INDEX_NONE && IsSlotValidCover(Current) && !Taken[Current])
		{
			Choice[Index] = Current;
			Taken[Current] = true;
		}
	}
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		if (Choice[Index] != INDEX_NONE || Agents[Index] == nullptr)
		{
			continue;
		}
		const FVector From = Agents[Index]->GetActorLocation();
		double BestDistanceSquared = TNumericLimits<double>::Max();
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			if (Taken[Slot] || !IsSlotValidCover(Slot))
			{
				continue;
			}
			const double DistanceSquared = FVector::DistSquared2D(From, Slots[Slot].Location);
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				Choice[Index] = Slot;
			}
		}
		if (Choice[Index] != INDEX_NONE)
		{
			Taken[Choice[Index]] = true;
		}
	}
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		if (Agents[Index])
		{
			Agents[Index]->GetBrain()->SetTarget(Choice[Index]);
		}
	}
}

void USQBenchmarkSubsystem::UpdateMetrics(double Now)
{
	CSV_SCOPED_TIMING_STAT(SQBench, Metrics);

	FMemory::Memzero(SlotTargetCounts.GetData(), SlotTargetCounts.Num());
	int32 InCover = 0;
	int32 Awaiting = 0;
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		if (Agents[Index] == nullptr)
		{
			continue;
		}
		const int32 Slot = Agents[Index]->GetBrain()->GetTargetSlot();
		if (SlotTargetCounts.IsValidIndex(Slot) && SlotTargetCounts[Slot] < MAX_uint8)
		{
			++SlotTargetCounts[Slot];
		}

		const bool bInCover = IsInCover(Index);
		InCover += bInCover ? 1 : 0;
		FAgentMetrics& Metrics = AgentMetrics[Index];
		if (Metrics.AwaitingEvent != INDEX_NONE)
		{
			if (bInCover)
			{
				FEventRecord& Event = Events[Metrics.AwaitingEvent];
				FArrival& Arrival = Event.Arrivals[Index];
				Arrival.Seconds = static_cast<float>(Now - Event.Time);
				Arrival.DistanceCm = static_cast<float>(FVector::Dist2D(Metrics.PositionAtEvent, Arena->GetSlots()[Slot].Location));
				Metrics.AwaitingEvent = INDEX_NONE;
			}
			else
			{
				++Awaiting;
			}
		}
	}

	// A conflict = two or more agents target the same slot. Count onsets (not frames), so one long conflict is one.
	int32 ConflictSlots = 0;
	int32 ConflictAgents = 0;
	for (int32 Slot = 0; Slot < SlotTargetCounts.Num(); ++Slot)
	{
		const bool bConflict = SlotTargetCounts[Slot] >= 2;
		if (bConflict)
		{
			++ConflictSlots;
			ConflictAgents += SlotTargetCounts[Slot];
			if (!SlotInConflict[Slot] && Phase == ERunPhase::Capture)
			{
				++ConflictOnsets;
			}
		}
		SlotInConflict[Slot] = bConflict;
	}

	if (Phase == ERunPhase::Capture)
	{
		++CapturedFrames;
		InCoverAgentFrames += InCover;
		ConflictAgentFrames += ConflictAgents;
	}

	CSV_CUSTOM_STAT(SQBench, InCover, InCover, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SQBench, Awaiting, Awaiting, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SQBench, ConflictSlots, ConflictSlots, ECsvCustomStatOp::Set);
}

void USQBenchmarkSubsystem::WriteRunReport() const
{
	const int64 AgentFrames = int64(CapturedFrames) * Agents.Num();
	// Item counts in the report come from EQS debug data: compiled in (USE_EQS_DEBUGGER) and switched on at run time.
#if USE_EQS_DEBUGGER
	const int32 EqsDebugData = FEnvQueryInstance::bDebuggingInfoEnabled ? 1 : 0;
#else
	const int32 EqsDebugData = 0;
#endif
	// The squad evaluation's work (SQ; zeros for the baselines), and the solve time distribution for SC7.
	FSquadQueryTotals SquadTotals;
	if (const USquadQuerySubsystem* Squads = GetWorld()->GetSubsystem<USquadQuerySubsystem>())
	{
		SquadTotals = Squads->GetTotals();
	}
	TArray<float> Solves = SquadTotals.SolveMicroseconds;
	Solves.Sort();
	const auto SolvePercentile = [&Solves](double P) -> double
	{
		return Solves.IsEmpty() ? 0.0 : Solves[FMath::Clamp(FMath::CeilToInt32(P / 100.0 * Solves.Num()) - 1, 0, Solves.Num() - 1)];
	};

	const FString Header = FString::Printf(
		TEXT("# run=%s mode=%s agents=%d seed=%d arenahash=0x%08X warmup=%.3f capture=%.3f interval=%.3f walkspeed=%.1f eqsdebug=%d benchstart=%.6f captured_events=%d frames=%d agent_frames=%lld in_cover_agent_frames=%lld conflict_onsets=%d conflict_agent_frames=%lld sq_queries_started=%d sq_queries_finished=%d sq_queries_aborted=%d sq_stale_results=%d sq_items_generated=%lld sq_item_tests=%lld sq_evaluations=%d sq_assignments=%d sq_claim_failures=%d sq_query_latency_s=%.6f sq_solves=%d sq_solve_us_p50=%.2f sq_solve_us_p99=%.2f sq_solve_us_max=%.2f\n"),
		*Settings.RunTag, *Settings.Mode, Agents.Num(), Arena->GetSeed(), Arena->GetArenaHash(), Settings.WarmupSeconds,
		Settings.CaptureSeconds, Settings.EventIntervalSeconds, ASQAgent::WalkSpeedCmPerSecond, EqsDebugData, BenchStartTime, CapturedEvents,
		CapturedFrames, AgentFrames, InCoverAgentFrames, ConflictOnsets, ConflictAgentFrames, SquadTotals.QueriesStarted,
		SquadTotals.QueriesFinished, SquadTotals.QueriesAborted, SquadTotals.StaleResults, SquadTotals.ItemsGenerated,
		SquadTotals.ItemTests, SquadTotals.Evaluations, SquadTotals.Assignments, SquadTotals.ClaimFailures,
		SquadTotals.QueryLatencySeconds, Solves.Num(), SolvePercentile(50.0), SolvePercentile(99.0), SolvePercentile(100.0));

	FString AgentsReport = Header + TEXT("agent,squad,switches,oscillations,move_requests,move_retries,move_failures,queries_started,queries_finished,queries_aborted,items_generated,item_tests,keep_checks,claim_failures,query_latency_s,target_slot,final_x,final_y,final_slot_distance_cm\n");
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const ASQAgent* Agent = Agents[Index];
		if (Agent == nullptr)
		{
			AgentsReport += FString::Printf(TEXT("%d,,,,,,,,,,,,,,,,,,\n"), Index);
			continue;
		}
		const USQAgentBrainComponent* Brain = Agent->GetBrain();
		const FSQBrainCounters& Captured = Brain->GetCounters();
		const FVector Location = Agent->GetActorLocation();
		const int32 Slot = Brain->GetTargetSlot();
		// How far the agent ended from its slot (-1: no slot). Above InCoverRadiusCm it never got there: a stuck agent.
		const double SlotDistance = Arena->GetSlots().IsValidIndex(Slot) ? FVector::Dist2D(Location, Arena->GetSlots()[Slot].Location) : -1.0;
		AgentsReport += FString::Printf(TEXT("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%d,%d,%.6f,%d,%.1f,%.1f,%.1f\n"), Index,
			Agent->GetSquadMember()->GetSquadId(), Captured.Switches, Captured.Oscillations, Captured.MoveRequests, Captured.MoveRetries,
			Captured.MoveFailures, Captured.QueriesStarted, Captured.QueriesFinished, Captured.QueriesAborted, Captured.ItemsGenerated,
			Captured.ItemTests, Captured.KeepChecks, Captured.ClaimFailures, Captured.QueryLatencySeconds, Slot, Location.X, Location.Y,
			SlotDistance);
	}

	FString EventsReport = Header + TEXT("event,threat,time,agent,seconds_to_cover,distance_cm\n");
	for (int32 EventIndex = 0; EventIndex < Events.Num(); ++EventIndex)
	{
		const FEventRecord& Event = Events[EventIndex];
		if (!Event.bCaptured)
		{
			continue;
		}
		for (int32 AgentIndex = 0; AgentIndex < Event.Arrivals.Num(); ++AgentIndex)
		{
			EventsReport += FString::Printf(TEXT("%d,%d,%.6f,%d,%.4f,%.1f\n"), EventIndex, Event.ThreatIndex, Event.Time - BenchStartTime,
				AgentIndex, Event.Arrivals[AgentIndex].Seconds, Event.Arrivals[AgentIndex].DistanceCm);
		}
	}

	const FString Folder = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SQRuns"));
	const FString AgentsPath = FPaths::Combine(Folder, Settings.RunTag + TEXT("_agents.csv"));
	const FString EventsPath = FPaths::Combine(Folder, Settings.RunTag + TEXT("_events.csv"));
	const bool bSaved = FFileHelper::SaveStringToFile(AgentsReport, *AgentsPath) && FFileHelper::SaveStringToFile(EventsReport, *EventsPath);
	UE_CLOG(bSaved, LogSQBench, Display, TEXT("Run report written: %s (+ _events.csv)"), *FPaths::ConvertRelativePathToFull(AgentsPath));
	UE_CLOG(!bSaved, LogSQBench, Error, TEXT("Could not write the run report to %s"), *Folder);
}

TStatId USQBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USQBenchmarkSubsystem, STATGROUP_Tickables);
}
