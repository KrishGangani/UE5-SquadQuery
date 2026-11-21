// SquadQueryCore - the world's squads.

#include "SquadQuerySubsystem.h"

#include "Squad.h"
#include "SquadMemberComponent.h"

bool USquadQuerySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
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
}

void USquadQuerySubsystem::UnregisterMember(USquadMemberComponent& Member)
{
	if (USquad* Squad = Member.Squad)
	{
		Squad->Members.Remove(&Member);
	}
	Member.Squad = nullptr;
}

void USquadQuerySubsystem::SetThreat(int32 SquadId, const FVector& Location)
{
	if (USquad* Squad = FindSquad(SquadId))
	{
		Squad->ThreatLocation = Location;
		Squad->bHasThreat = true;
	}
}

USquad* USquadQuerySubsystem::FindSquad(int32 SquadId) const
{
	const TObjectPtr<USquad>* Found = Squads.Find(SquadId);
	return Found ? Found->Get() : nullptr;
}

void USquadQuerySubsystem::Deinitialize()
{
	// Members normally leave in EndPlay, before the world tears its subsystems down. Clear any that didn't, so no
	// component keeps a pointer to a squad that is about to be destroyed.
	for (const TPair<int32, TObjectPtr<USquad>>& Entry : Squads)
	{
		if (USquad* Squad = Entry.Value)
		{
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
	Super::Deinitialize();
}
