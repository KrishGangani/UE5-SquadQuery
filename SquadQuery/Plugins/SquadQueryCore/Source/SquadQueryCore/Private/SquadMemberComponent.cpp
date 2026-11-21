// SquadQueryCore - puts an actor into a squad.

#include "SquadMemberComponent.h"

#include "Engine/World.h"
#include "SquadQuerySubsystem.h"

USquadMemberComponent::USquadMemberComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USquadMemberComponent::BeginPlay()
{
	Super::BeginPlay();
	if (USquadQuerySubsystem* Subsystem = GetWorld()->GetSubsystem<USquadQuerySubsystem>())
	{
		Subsystem->RegisterMember(*this);
	}
}

void USquadMemberComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USquadQuerySubsystem* Subsystem = GetWorld()->GetSubsystem<USquadQuerySubsystem>())
	{
		Subsystem->UnregisterMember(*this);
	}
	Super::EndPlay(EndPlayReason);
}
