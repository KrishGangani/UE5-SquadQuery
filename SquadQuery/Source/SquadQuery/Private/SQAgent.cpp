// P07 SquadQuery (demo) - one squad member.

#include "SQAgent.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SQAgentBrainComponent.h"
#include "SquadMemberComponent.h"
#include "UObject/ConstructorHelpers.h"

ASQAgent::ASQAgent()
{
	// The character's own tick is not needed: movement ticks in its component, decisions in the brain.
	PrimaryActorTick.bCanEverTick = false;

	// A plain engine AI controller possesses every spawned agent: path following comes from it, decisions don't.
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Spawned;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = WalkSpeedCmPerSecond;
	Movement->bOrientRotationToMovement = true;
	// RVO avoidance: agents steer around each other instead of pushing into a squadmate who already stands on their path.
	// Without it, the first S2 run left about 6% of agent-events short of cover, each stuck with its capsule touching
	// another agent's (09 S2-a). The same for every configuration, so it changes movement cost equally.
	Movement->bUseRVOAvoidance = true;
	bUseControllerRotationYaw = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetStaticMesh(CylinderMesh.Object);
	// The engine cylinder is 100 cm wide and 100 cm tall, centred on its origin; match the default capsule (radius 34,
	// half-height 88). The capsule collides and blocks; the body is only seen.
	const float CapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	Body->SetRelativeScale3D(FVector(2.f * CapsuleRadius, 2.f * CapsuleRadius, 2.f * CapsuleHalfHeight) / 100.f);
	Body->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Body->SetCanEverAffectNavigation(false);

	Brain = CreateDefaultSubobject<USQAgentBrainComponent>(TEXT("Brain"));
	SquadMember = CreateDefaultSubobject<USquadMemberComponent>(TEXT("SquadMember"));
}
