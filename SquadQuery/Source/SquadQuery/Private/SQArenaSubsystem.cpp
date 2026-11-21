// P07 SquadQuery (demo) - builds the seeded test arena and reports when its navmesh is ready.

#include "SQArenaSubsystem.h"

#include "Components/BrushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "PhysicsEngine/BodySetup.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "SQCoverBlock.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSQArena, Log, All);

namespace SQArena
{
	// The six slots of one block, in the block's local frame (X = length, Y = thickness): two on each long side,
	// one on each short side, each SlotStandoffCm from the face. Adjacent slots are at least 1.6 m apart.
	constexpr float LongSideY = 0.5f * ASQCoverBlock::ThicknessCm + USQArenaSubsystem::SlotStandoffCm;   // 110
	constexpr float ShortSideX = 0.5f * ASQCoverBlock::LengthCm + USQArenaSubsystem::SlotStandoffCm;     // 210
	const FVector SlotOffsets[] =
	{
		FVector( USQArenaSubsystem::SlotAlongCm,  LongSideY, 0.f),
		FVector(-USQArenaSubsystem::SlotAlongCm,  LongSideY, 0.f),
		FVector( USQArenaSubsystem::SlotAlongCm, -LongSideY, 0.f),
		FVector(-USQArenaSubsystem::SlotAlongCm, -LongSideY, 0.f),
		FVector( ShortSideX, 0.f, 0.f),
		FVector(-ShortSideX, 0.f, 0.f),
	};
	constexpr int32 SlotsPerBlock = UE_ARRAY_COUNT(SlotOffsets);
	constexpr int32 NumBlocks = USQArenaSubsystem::GridCount * USQArenaSubsystem::GridCount;
	constexpr int32 NumSlots = NumBlocks * SlotsPerBlock;

	// Unjittered block centres run from -GridHalfExtentCm to +GridHalfExtentCm (54 m) on both axes.
	constexpr float GridHalfExtentCm = 0.5f * USQArenaSubsystem::GridSpacingCm * (USQArenaSubsystem::GridCount - 1);

	constexpr double NavTimeoutRealSeconds = 60.0;       // real time: fixed-step runs can simulate faster than real time
	// Path-check corners: beyond every block and slot (centre + jitter + the farthest slot), with a margin, and inside
	// the floor and the nav bounds.
	constexpr float CornerCm = GridHalfExtentCm + USQArenaSubsystem::GridJitterCm + ShortSideX + 290.f;   // 6200
	static_assert(CornerCm < 0.5f * USQArenaSubsystem::FloorSizeCm, "path-check corners must lie on the floor");
	constexpr float NavBoundsHeightCm = 1000.f;          // the bounds box spans z = -5 m .. +5 m
	constexpr float ThreatClearanceCm = 260.f;           // a threat never stands inside or against a block
	constexpr int32 MinHiddenSlotsPerThreat = 256;       // 2 x 128 agents: the S1 pass threshold (03)

	// One synchronous path test between two floor points. It builds no path object, and a partial path counts as
	// a failure (PImplRecastNavMesh.cpp:310-314, RecastNavMesh.cpp:3920-3921). The query takes the navmesh's
	// default filter when none is given (NavigationData.cpp:53-56).
	bool IsPathComplete(const UNavigationSystemV1& NavSys, const FVector& From, const FVector& To)
	{
		const ANavigationData* NavData = NavSys.GetDefaultNavDataInstance();
		return NavData != nullptr && NavSys.TestPathSync(FPathFindingQuery(nullptr, *NavData, From, To));
	}

	// The readiness test: a complete path along the arena's diagonal, corner to corner.
	bool SpansArena(const UNavigationSystemV1& NavSys)
	{
		return IsPathComplete(NavSys, FVector(-CornerCm, -CornerCm, 0.f), FVector(CornerCm, CornerCm, 0.f));
	}

	// The self-check's verdict as the process exit code. A failure is a forced exit, so the code reaches the caller
	// (WindowsPlatformMisc.cpp:1493-1515); a pass lets the engine shut down normally.
	void RequestSelfCheckExit(bool bPass)
	{
		if (bPass)
		{
			FPlatformMisc::RequestExit(/*Force*/ false, TEXT("SQArenaCheck"));
		}
		else
		{
			FPlatformMisc::RequestExitWithStatus(/*Force*/ true, 1, TEXT("SQArenaCheck"));
		}
	}
}

bool USQArenaSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USQArenaSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const TCHAR* CommandLine = FCommandLine::Get();
	FParse::Value(CommandLine, TEXT("SQSeed="), Seed);
	bSelfCheck = FParse::Param(CommandLine, TEXT("SQArenaCheck"));

	// Listen before the arena exists, so no build can finish unheard. The navigation system broadcasts this whenever
	// its generator's task count drops to zero (RecastNavMeshGenerator.cpp:7614-7631 -> RecastNavMesh.cpp:3237-3240
	// -> NavigationSystem.cpp:4913-4915), in game builds too.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&InWorld))
	{
		NavSys->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(this, &USQArenaSubsystem::HandleNavigationGenerationFinished);
	}
	else
	{
		UE_LOG(LogSQArena, Error, TEXT("No navigation system in this world: the arena can't become ready."));
	}

	BuildStartRealSeconds = FPlatformTime::Seconds();
	BuildArena(InWorld);

	InWorld.GetTimerManager().SetTimer(TimeoutTimer, this, &USQArenaSubsystem::CheckNavigationTimeout,
		static_cast<float>(SQArena::NavTimeoutRealSeconds), /*bLoop*/ false);
}

void USQArenaSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeoutTimer);
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			NavSys->OnNavigationGenerationFinishedDelegate.RemoveAll(this);
		}
	}
	Super::Deinitialize();
}

void USQArenaSubsystem::BuildArena(UWorld& World)
{
	FRandomStream Stream(Seed);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, ASQCoverBlock::CubeMeshPath);
	UE_CLOG(Cube == nullptr, LogSQArena, Error, TEXT("%s is missing: the floor has no mesh and navigation can't build."), ASQCoverBlock::CubeMeshPath);

	// Floor: the engine cube flattened to 130 m x 130 m x 20 cm, top face at z = 0. The mesh is set after spawning, on
	// a registered Static component: allowed only before the world has begun play, which is the case in
	// OnWorldBeginPlay (StaticMeshComponent.cpp:2374-2383; World.cpp:6165-6174).
	constexpr float FloorThicknessCm = 20.f;
	const FTransform FloorTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -0.5f * FloorThicknessCm),
		FVector(FloorSizeCm, FloorSizeCm, FloorThicknessCm) / ASQCoverBlock::CubeSizeCm);
	if (AStaticMeshActor* Floor = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FloorTransform))
	{
		Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Floor->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		UE_LOG(LogSQArena, Error, TEXT("Spawning the floor failed."));
	}

	CoverDefinition = CreateCoverDefinition();

	// Blocks on a jittered grid. One random draw per statement: the order in which function arguments are evaluated
	// is unspecified in C++, so draws inside one expression could change order between compilers.
	const float GridOrigin = -SQArena::GridHalfExtentCm;
	Blocks.Reserve(SQArena::NumBlocks);
	Slots.Reserve(SQArena::NumSlots);
	uint32 Crc = 0;
	for (int32 Row = 0; Row < GridCount; ++Row)
	{
		for (int32 Column = 0; Column < GridCount; ++Column)
		{
			const float JitterX = Stream.FRandRange(-GridJitterCm, GridJitterCm);
			const float JitterY = Stream.FRandRange(-GridJitterCm, GridJitterCm);
			const float Yaw = Stream.RandRange(0, 1) * 90.f;
			const FVector Location(GridOrigin + Column * GridSpacingCm + JitterX, GridOrigin + Row * GridSpacingCm + JitterY, 0.f);
			const FTransform BlockTransform(FRotator(0.f, Yaw, 0.f), Location);

			ASQCoverBlock* Block = World.SpawnActorDeferred<ASQCoverBlock>(ASQCoverBlock::StaticClass(), BlockTransform);
			if (Block == nullptr)
			{
				UE_LOG(LogSQArena, Error, TEXT("Spawning cover block %d failed; the arena is incomplete."), Row * GridCount + Column);
				continue;
			}
			Block->SetCoverDefinition(CoverDefinition);
			Block->FinishSpawning(BlockTransform);

			const int32 BlockIndex = Blocks.Add(Block);
			for (int32 SlotIndex = 0; SlotIndex < SQArena::SlotsPerBlock; ++SlotIndex)
			{
				FSQCoverSlot& Slot = Slots.AddDefaulted_GetRef();
				Slot.Location = BlockTransform.TransformPosition(SQArena::SlotOffsets[SlotIndex]);
				Slot.BlockIndex = BlockIndex;
				Slot.SlotIndex = SlotIndex;
			}
			Crc = FCrc::TypeCrc32(Location, Crc);
			Crc = FCrc::TypeCrc32(Yaw, Crc);
		}
	}

	PlaceThreatPositions(Stream);
	for (const FVector& Threat : ThreatPositions)
	{
		Crc = FCrc::TypeCrc32(Threat, Crc);
	}
	ArenaHash = Crc;

	SpawnNavigationBounds(World);

	UE_LOG(LogSQArena, Display, TEXT("Arena built: seed %d, %d blocks, %d slots, %d threat positions, hash 0x%08X, %.1f ms"),
		Seed, Blocks.Num(), Slots.Num(), ThreatPositions.Num(), ArenaHash, (FPlatformTime::Seconds() - BuildStartRealSeconds) * 1000.0);
}

USmartObjectDefinition* USQArenaSubsystem::CreateCoverDefinition()
{
	// Built the way Epic's own Smart Object tests build one (SmartObjectsTest.cpp:65-72): NewObject plus DebugAddSlot().
	USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(this, TEXT("SQCoverDefinition"), RF_Transient);
	for (const FVector& Offset : SQArena::SlotOffsets)
	{
		FSmartObjectSlotDefinition& SlotDefinition = Definition->DebugAddSlot();
		SlotDefinition.Offset = FVector3f(Offset);
		// Without a behavior definition the definition fails validation and its components never register.
		SlotDefinition.BehaviorDefinitions.Add(NewObject<USQCoverBehaviorDefinition>(Definition));
	}
	return Definition;
}

void USQArenaSubsystem::SpawnNavigationBounds(UWorld& World)
{
	// A bounds volume spawned at runtime has no editor brush. Outside the editor its bounds come from the brush
	// component's body setup (BrushComponent.cpp:476-481), so give it one box. FKBoxElem takes full lengths.
	const FTransform VolumeTransform(FVector::ZeroVector);
	ANavMeshBoundsVolume* Volume = World.SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(), VolumeTransform);
	if (Volume == nullptr)
	{
		UE_LOG(LogSQArena, Error, TEXT("Spawning the navigation bounds volume failed; agents will have no navmesh."));
		return;
	}
	UBrushComponent* Brush = Volume->GetBrushComponent();
	UBodySetup* Body = NewObject<UBodySetup>(Brush, NAME_None, RF_Transient);
	Body->AggGeom.BoxElems.Emplace(FloorSizeCm, FloorSizeCm, SQArena::NavBoundsHeightCm);
	Brush->BrushBodySetup = Body;
	// The component's bounds are cached: computed when it registered, during the spawn (Actor.cpp:4330-4336), while it
	// had no body. The navigation system reads the cached value (Actor.cpp:2272-2274 via NavigationSystem.cpp:4145),
	// so recompute it now. Without this the arena's navmesh is never built (09 S1-c).
	Brush->UpdateBounds();

	// Registration already told the navigation system about the volume, with the empty box it had then
	// (NavMeshBoundsVolume.cpp:117-127). This second announcement, with the real box, replaces that request: it is the
	// one that gives the navmesh its bounds, so don't remove it.
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World))
	{
		NavSys->OnNavigationBoundsUpdated(Volume);
	}
}

void USQArenaSubsystem::PlaceThreatPositions(FRandomStream& Stream)
{
	const float Range = SQArena::GridHalfExtentCm;
	for (int32 Attempt = 0; Attempt < 10000 && ThreatPositions.Num() < NumThreatPositions; ++Attempt)
	{
		const float X = Stream.FRandRange(-Range, Range);
		const float Y = Stream.FRandRange(-Range, Range);
		const FVector Candidate(X, Y, 0.f);

		const bool bClear = !Blocks.ContainsByPredicate([&Candidate](const ASQCoverBlock* Block)
		{
			return FVector::DistSquared2D(Block->GetActorLocation(), Candidate) < FMath::Square(SQArena::ThreatClearanceCm);
		});
		if (bClear)
		{
			ThreatPositions.Add(Candidate);
		}
	}
	UE_CLOG(ThreatPositions.Num() < NumThreatPositions, LogSQArena, Error, TEXT("Only %d of %d threat positions found."),
		ThreatPositions.Num(), NumThreatPositions);
}

void USQArenaSubsystem::HandleNavigationGenerationFinished(ANavigationData* NavData)
{
	// Ready = a build has finished, nothing else is queued, and the navmesh spans the arena. "No build in progress"
	// alone is not proof: it is also true before the first build is queued (IsBuildInProgressCheckDirty looks only at
	// queued work). The event alone isn't either: it also fires for a build that covers almost nothing (09 S1-c). So one
	// corner-to-corner path test per event; if it fails, the next build's event (or the timeout) decides.
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (bReady || World == nullptr || NavSys == nullptr || NavSys->IsNavigationBuildInProgress()
		|| !SQArena::SpansArena(*NavSys))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(TimeoutTimer);
	FinishArena(*World);
}

void USQArenaSubsystem::CheckNavigationTimeout()
{
	UWorld* World = GetWorld();
	if (bReady || World == nullptr)
	{
		return;
	}
	// Timers count world time; the navmesh builds on worker threads in real time. A fixed-step run with fast frames
	// runs world time ahead of real time, so re-arm until the real-time limit has really passed.
	const double RealSecondsLeft = SQArena::NavTimeoutRealSeconds - (FPlatformTime::Seconds() - BuildStartRealSeconds);
	if (RealSecondsLeft > 0.0)
	{
		World->GetTimerManager().SetTimer(TimeoutTimer, this, &USQArenaSubsystem::CheckNavigationTimeout,
			static_cast<float>(RealSecondsLeft), /*bLoop*/ false);
		return;
	}
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	UE_LOG(LogSQArena, Error, TEXT("Navigation not ready after %.0f s: navmesh %s, corner-to-corner path %s."), SQArena::NavTimeoutRealSeconds,
		NavSys && NavSys->GetDefaultNavDataInstance() ? TEXT("exists") : TEXT("missing"),
		NavSys && SQArena::SpansArena(*NavSys) ? TEXT("complete") : TEXT("incomplete"));
	if (bSelfCheck)
	{
		SQArena::RequestSelfCheckExit(/*bPass*/ false);
	}
}

void USQArenaSubsystem::FinishArena(UWorld& World)
{
	UE_LOG(LogSQArena, Display, TEXT("Navigation built %.3f s (real) after begin play"), FPlatformTime::Seconds() - BuildStartRealSeconds);
	FillSlotHandles(World);
	BuildVisibilityTable(World);
	bReady = true;
	OnArenaReady.Broadcast();

	if (bSelfCheck)
	{
		SQArena::RequestSelfCheckExit(RunSelfCheck(World));
	}
}

void USQArenaSubsystem::FillSlotHandles(UWorld& World)
{
	const USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(&World);
	if (SmartObjects == nullptr)
	{
		UE_LOG(LogSQArena, Error, TEXT("No Smart Object subsystem: cover slots have no handles."));
		return;
	}

	// GetAllSlots returns a block's slots in definition order, and the table was filled in the same order.
	TArray<FSmartObjectSlotHandle> BlockSlots;
	SlotIndexByHandle.Reset();
	SlotIndexByHandle.Reserve(Slots.Num());
	for (int32 TableIndex = 0; TableIndex < Slots.Num(); ++TableIndex)
	{
		FSQCoverSlot& Slot = Slots[TableIndex];
		if (Slot.SlotIndex == 0)
		{
			BlockSlots.Reset();
			const ASQCoverBlock* Block = Blocks[Slot.BlockIndex];
			if (Block && Block->GetSmartObjectComponent())
			{
				SmartObjects->GetAllSlots(Block->GetSmartObjectComponent()->GetRegisteredHandle(), BlockSlots);
			}
		}
		if (BlockSlots.IsValidIndex(Slot.SlotIndex))
		{
			Slot.Handle = BlockSlots[Slot.SlotIndex];
			SlotIndexByHandle.Add(Slot.Handle, TableIndex);
		}
	}
	const int32 WithHandle = SlotIndexByHandle.Num();
	// A block whose definition failed validation never registers, silently (01b finding 19): say so in every run.
	UE_CLOG(WithHandle != Slots.Num(), LogSQArena, Error, TEXT("Only %d of %d cover slots are registered Smart Object slots."), WithHandle, Slots.Num());
}

void USQArenaSubsystem::BuildVisibilityTable(UWorld& World)
{
	// One synchronous Visibility trace per (threat position, slot), from the threat's eye to a crouched agent's chest:
	// the same heights the cover query's trace test uses (02 section 7). Done once, before any agent exists, so the
	// metrics never trace during a measured run. Pawns ignore Visibility (BaseEngine.ini:3110), so agents never block it.
	const double StartSeconds = FPlatformTime::Seconds();
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(SQArenaVisibility), /*bTraceComplex*/ false);
	HiddenTable.Init(false, ThreatPositions.Num() * Slots.Num());
	for (int32 ThreatIndex = 0; ThreatIndex < ThreatPositions.Num(); ++ThreatIndex)
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			HiddenTable[ThreatIndex * Slots.Num() + SlotIndex] = TraceHidden(World, ThreatPositions[ThreatIndex], Slots[SlotIndex].Location, Params);
		}
	}
	UE_LOG(LogSQArena, Display, TEXT("Visibility table: %d traces in %.1f ms"), HiddenTable.Num(), (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
}

bool USQArenaSubsystem::TraceHidden(const UWorld& World, const FVector& ThreatFloor, const FVector& SlotFloor, const FCollisionQueryParams& Params)
{
	return World.LineTraceTestByChannel(ThreatFloor + FVector(0.f, 0.f, ThreatEyeHeightCm), SlotFloor + FVector(0.f, 0.f, SlotProbeHeightCm),
		ECC_Visibility, Params);
}

bool USQArenaSubsystem::RunSelfCheck(UWorld& World) const
{
	const USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(&World);
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
	if (SmartObjects == nullptr || NavSys == nullptr)
	{
		UE_LOG(LogSQArena, Error, TEXT("SQArenaCheck FAIL: missing the Smart Object or navigation subsystem."));
		return false;
	}

	// 1. The Smart Object runtime knows every slot: an unfiltered spatial query over the whole arena.
	TArray<FSmartObjectRequestResult> Found;
	const float HalfHeight = 0.5f * SQArena::NavBoundsHeightCm;
	const FBox ArenaBox(FVector(-FloorSizeCm, -FloorSizeCm, -HalfHeight), FVector(FloorSizeCm, FloorSizeCm, HalfHeight));
	SmartObjects->FindSmartObjects(FSmartObjectRequest(ArenaBox, FSmartObjectRequestFilter()), Found, FConstStructView());

	// 2. Every slot has a handle, the runtime puts it where our table says, and it lies on the navmesh.
	int32 WithHandle = 0;
	int32 Navigable = 0;
	double MaxLocationErrorCm = 0.0;
	for (const FSQCoverSlot& Slot : Slots)
	{
		if (Slot.Handle.IsValid())
		{
			++WithHandle;
			const FVector RuntimeLocation = SmartObjects->GetSlotTransformChecked(Slot.Handle).GetLocation();
			MaxLocationErrorCm = FMath::Max(MaxLocationErrorCm, FVector::Dist(RuntimeLocation, Slot.Location));
		}
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Slot.Location, Projected, FVector(50.f, 50.f, 150.f)))
		{
			++Navigable;
		}
	}

	// 3. Paths cross the arena along both diagonals and two edges.
	const float C = SQArena::CornerCm;
	const FVector Corners[4] = { FVector(-C, -C, 0.f), FVector(C, C, 0.f), FVector(-C, C, 0.f), FVector(C, -C, 0.f) };
	constexpr int32 PathEnds[4][2] = { { 0, 1 }, { 2, 3 }, { 0, 3 }, { 2, 1 } };
	int32 CompletePaths = 0;
	for (const int32 (&Ends)[2] : PathEnds)
	{
		CompletePaths += SQArena::IsPathComplete(*NavSys, Corners[Ends[0]], Corners[Ends[1]]) ? 1 : 0;
	}

	// 4. Enough cover for 128 agents from every threat position: hidden, and hidden at least 8 m from the threat.
	// With no threat positions the minimums stay 0, so the log line can't suggest plenty of cover.
	int32 MinHidden = ThreatPositions.IsEmpty() ? 0 : MAX_int32, MaxHidden = 0, MinHiddenFar = MinHidden;
	for (int32 ThreatIndex = 0; ThreatIndex < ThreatPositions.Num(); ++ThreatIndex)
	{
		int32 Hidden = 0, HiddenFar = 0;
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			Hidden += IsSlotHidden(ThreatIndex, SlotIndex) ? 1 : 0;
			HiddenFar += IsValidCover(ThreatIndex, SlotIndex) ? 1 : 0;
		}
		MinHidden = FMath::Min(MinHidden, Hidden);
		MaxHidden = FMath::Max(MaxHidden, Hidden);
		MinHiddenFar = FMath::Min(MinHiddenFar, HiddenFar);
	}

	const int32 ExpectedSlots = SQArena::NumSlots;
	const bool bPass = Slots.Num() == ExpectedSlots
		&& Found.Num() == ExpectedSlots
		&& WithHandle == ExpectedSlots
		&& MaxLocationErrorCm < 1.0
		&& Navigable == ExpectedSlots
		&& CompletePaths == 4
		&& ThreatPositions.Num() == NumThreatPositions
		&& MinHiddenFar >= SQArena::MinHiddenSlotsPerThreat;

	UE_LOG(LogSQArena, Display, TEXT("SQArenaCheck %s: seed %d hash 0x%08X; slots %d (expected %d), found by the Smart Object runtime %d, with handle %d, max location error %.3f cm, on navmesh %d; complete corner paths %d/4; threat positions %d; hidden slots per threat %d..%d, of them >= 8 m from the threat at least %d (needs %d)"),
		bPass ? TEXT("PASS") : TEXT("FAIL"), Seed, ArenaHash, Slots.Num(), ExpectedSlots, Found.Num(), WithHandle, MaxLocationErrorCm,
		Navigable, CompletePaths, ThreatPositions.Num(), MinHidden, MaxHidden, MinHiddenFar, SQArena::MinHiddenSlotsPerThreat);
	UE_CLOG(!bPass, LogSQArena, Error, TEXT("SQArenaCheck FAIL (see the line above)."));
	return bPass;
}
