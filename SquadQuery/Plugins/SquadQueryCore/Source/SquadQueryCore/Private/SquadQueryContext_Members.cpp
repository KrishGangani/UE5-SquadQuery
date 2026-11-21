// SquadQueryCore - EQS context: where every member of the querying squad stands.

#include "SquadQueryContext_Members.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Actor.h"
#include "Squad.h"
#include "SquadMemberComponent.h"

void USquadQueryContext_Members::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const USquad* Squad = Cast<USquad>(QueryInstance.Owner.Get());
	if (Squad == nullptr)
	{
		return;
	}
	TArray<FVector> Locations;
	Locations.Reserve(Squad->GetMembers().Num());
	for (const USquadMemberComponent* Member : Squad->GetMembers())
	{
		if (Member && Member->GetOwner())
		{
			Locations.Add(Member->GetOwner()->GetActorLocation());
		}
	}
	UEnvQueryItemType_Point::SetContextHelper(ContextData, Locations);
}
