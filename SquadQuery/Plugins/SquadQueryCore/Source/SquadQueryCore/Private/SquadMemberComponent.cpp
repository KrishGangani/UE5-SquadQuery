// SquadQueryCore - puts an actor into a squad, and carries the slot the squad assigned to it.

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

void USquadMemberComponent::SetAssignment(const FSmartObjectSlotHandle& Slot, const FVector& Location)
{
	if (Slot == AssignedSlot)
	{
		return;
	}
	AssignedSlot = Slot;
	AssignedLocation = Location;
	OnAssignmentChangedNative.Broadcast(*this);
	OnAssignmentChanged.Broadcast(this);
}
