// P07 SquadQuery (demo) - one cover block of the test arena: a low wall with six Smart Object cover slots.

#pragma once

#include "GameFramework/Actor.h"
#include "SmartObjectDefinition.h"
#include "SQCoverBlock.generated.h"

class USmartObjectComponent;
class UStaticMeshComponent;

/**
 * The behavior type of a cover slot. It carries no data: Smart Object definitions are only valid when every slot has
 * at least one behavior definition (SmartObjectDefinition.cpp:205-222), and this one also names what the slots are for.
 * Same shape as Epic's own test behavior (SmartObjectTestTypes.h).
 */
UCLASS(HideDropdown, NotBlueprintType)
class USQCoverBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
};

/**
 * A 3.0 m x 1.0 m x 1.3 m block that blocks the Visibility channel (so it hides a crouched agent from the threat) and
 * navigation (so agents walk around it), plus the Smart Object component that owns its six cover slots.
 *
 * Spawn it deferred and call SetCoverDefinition() before FinishSpawning(): in game worlds the Smart Object component
 * registers itself in BeginPlay, and its definition can't change once it is registered (SmartObjectComponent.cpp:359-372).
 */
UCLASS()
class ASQCoverBlock : public AActor
{
	GENERATED_BODY()

public:
	ASQCoverBlock();

	/** Gives the Smart Object component its (shared, runtime-built) definition. Only valid before FinishSpawning(). */
	void SetCoverDefinition(USmartObjectDefinition* Definition);

	USmartObjectComponent* GetSmartObjectComponent() const { return SmartObject; }

	/** Block size in cm (X = length, Y = thickness, Z = height). The slot offsets in USQArenaSubsystem are derived from it. */
	static constexpr float LengthCm = 300.f;
	static constexpr float ThicknessCm = 100.f;
	static constexpr float HeightCm = 130.f;

	/** The engine cube both the blocks and the arena floor are made of: 100 cm on each side, centred on its origin.
	 *  Cooked only because DefaultGame.ini always cooks /Engine/BasicShapes (05 A8). */
	static constexpr const TCHAR* CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	static constexpr float CubeSizeCm = 100.f;

private:
	/** Unscaled root at floor level, so slot offsets (relative to the actor) are in plain centimetres. */
	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<USceneComponent> Root;

	/** The engine cube (100 cm) scaled to the block size. */
	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Cover")
	TObjectPtr<USmartObjectComponent> SmartObject;
};
