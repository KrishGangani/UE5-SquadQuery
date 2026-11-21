// P07 SquadQuery (demo) - builds the seeded test arena: floor, cover blocks with Smart Object slots, runtime navmesh,
// threat positions and a visibility table, then reports "ready".

#pragma once

#include "Containers/BitArray.h"
#include "SmartObjectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SQArenaSubsystem.generated.h"

class ANavigationData;
class ASQCoverBlock;
class USmartObjectDefinition;

/** One cover slot of the arena, in a flat table that every configuration and the metrics share. Plain C++ (no
 *  reflection needed): it holds no UObject pointers, so GC doesn't have to see it. */
struct FSQCoverSlot
{
	/** World position on the floor (z = 0). */
	FVector Location = FVector::ZeroVector;

	/** Owning block (index into the arena's block list) and slot index inside the block's definition (0..5). */
	int32 BlockIndex = INDEX_NONE;
	int32 SlotIndex = INDEX_NONE;

	/** The Smart Object slot, valid once the block's Smart Object has registered (that is, once the arena is ready). */
	FSmartObjectSlotHandle Handle;
};

/**
 * The test arena of 01_Problem_Analysis.md 6.1, built by code from a seed. A world subsystem for the same reason as
 * UE5-UpdateBudget's director: there is no authored map, and the subsystem already has the world's lifetime.
 *
 * Timeline:
 *   OnWorldBeginPlay   spawn the floor, 100 blocks (deferred, so each Smart Object definition is set before
 *                      BeginPlay registers it) and one nav bounds volume with a box body (01b finding 9)
 *   build finished     the navigation system reports a finished navmesh build (OnNavigationGenerationFinishedDelegate)
 *                      and no other build is pending: ready. Fails after 60 s of real time
 *   when ready         read the slot handles, trace the visibility table (threat position x slot), broadcast
 *                      OnArenaReady. With -SQArenaCheck: run the S1 self-check, log it, and quit with exit code 0 or 1
 *
 * Command line: -SQSeed=S (default 1), -SQArenaCheck.
 */
UCLASS()
class USQArenaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem

	/** True once navigation is built and the slot handles and visibility table are filled. */
	bool IsReady() const { return bReady; }

	/** Fires once, when the arena becomes ready. */
	FSimpleMulticastDelegate OnArenaReady;

	const TArray<FSQCoverSlot>& GetSlots() const { return Slots; }
	const TArray<FVector>& GetThreatPositions() const { return ThreatPositions; }

	/** True if a Visibility trace from the threat's eye to the slot is blocked. False before the arena is ready. */
	bool IsSlotHidden(int32 ThreatIndex, int32 SlotIndex) const
	{
		return bReady && ThreatPositions.IsValidIndex(ThreatIndex) && Slots.IsValidIndex(SlotIndex)
			&& HiddenTable[ThreatIndex * Slots.Num() + SlotIndex];
	}

	/**
	 * Valid cover from a threat position: hidden from it and at least MinThreatDistanceCm away, the same two conditions
	 * as the cover query's filters (02 section 7). The one definition the self-check and every metric use.
	 */
	bool IsValidCover(int32 ThreatIndex, int32 SlotIndex) const
	{
		return IsSlotHidden(ThreatIndex, SlotIndex)
			&& FVector::DistSquared2D(ThreatPositions[ThreatIndex], Slots[SlotIndex].Location) >= FMath::Square(MinThreatDistanceCm);
	}

	int32 GetSeed() const { return Seed; }

	/** CRC of every block transform and threat position: equal seeds must give equal hashes. */
	uint32 GetArenaHash() const { return ArenaHash; }

	/** Layout constants (cm). The arena is centred on the world origin. */
	static constexpr int32 GridCount = 10;                 // 10 x 10 blocks
	static constexpr float GridSpacingCm = 1200.f;
	static constexpr float GridJitterCm = 300.f;
	static constexpr float FloorSizeCm = 13000.f;          // 120 m of blocks plus a margin
	static constexpr float SlotStandoffCm = 60.f;          // slot distance from the block face
	static constexpr float SlotAlongCm = 80.f;             // long-side slots at +/- this from the block centre
	static constexpr int32 NumThreatPositions = 10;
	static constexpr float ThreatEyeHeightCm = 170.f;      // a standing shooter
	static constexpr float SlotProbeHeightCm = 90.f;       // a crouched agent's chest
	static constexpr float MinThreatDistanceCm = 800.f;    // a cover slot is at least this far from the threat

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only: subsystems also exist in editor worlds, where nothing should be spawned. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	void BuildArena(UWorld& World);
	USmartObjectDefinition* CreateCoverDefinition();
	void SpawnNavigationBounds(UWorld& World);
	void PlaceThreatPositions(FRandomStream& Stream);
	/** Bound to UNavigationSystemV1::OnNavigationGenerationFinishedDelegate (a dynamic delegate, so a UFUNCTION). */
	UFUNCTION()
	void HandleNavigationGenerationFinished(ANavigationData* NavData);
	void CheckNavigationTimeout();
	void FinishArena(UWorld& World);
	void FillSlotHandles(UWorld& World);
	void BuildVisibilityTable(UWorld& World);
	bool RunSelfCheck(UWorld& World) const;

	/** One runtime definition shared by every block (no assets, 01b finding 11). Each block's component references it
	 *  too; the arena holds it so its owner is explicit. */
	UPROPERTY(Transient)
	TObjectPtr<USmartObjectDefinition> CoverDefinition;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ASQCoverBlock>> Blocks;

	TArray<FSQCoverSlot> Slots;
	TArray<FVector> ThreatPositions;

	/** Row-major: ThreatIndex * Slots.Num() + SlotIndex. */
	TBitArray<> HiddenTable;

	FTimerHandle TimeoutTimer;
	double BuildStartRealSeconds = 0.0;
	int32 Seed = 1;
	uint32 ArenaHash = 0;
	bool bSelfCheck = false;
	bool bReady = false;
};
