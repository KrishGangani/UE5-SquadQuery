// P07 SquadQuery (demo) - one cover block of the test arena.

#include "SQCoverBlock.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "SmartObjectComponent.h"
#include "UObject/ConstructorHelpers.h"

ASQCoverBlock::ASQCoverBlock()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	// Looked up once per class (static), not once per spawned block.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(CubeMeshPath);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetMobility(EComponentMobility::Static);
	Mesh->SetStaticMesh(CubeMesh.Object);
	// The cube is centred on its origin: scale it to the block, lift it onto the floor.
	Mesh->SetRelativeScale3D(FVector(LengthCm, ThicknessCm, HeightCm) / CubeSizeCm);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, HeightCm * 0.5f));
	// Blocks Visibility (the cover test's trace channel) and Pawn (agents walk around it); exported to the navmesh.
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	SmartObject = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObject"));
	SmartObject->SetupAttachment(Root);
}

void ASQCoverBlock::SetCoverDefinition(USmartObjectDefinition* Definition)
{
	SmartObject->SetDefinition(Definition);
}
