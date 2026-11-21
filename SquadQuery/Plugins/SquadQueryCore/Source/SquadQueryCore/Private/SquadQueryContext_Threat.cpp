// SquadQueryCore - EQS context: where the querier's squad believes its threat is.

#include "SquadQueryContext_Threat.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Actor.h"
#include "Squad.h"
#include "SquadMemberComponent.h"

USquad* USquadQueryContext_Threat::ResolveSquad(UObject* QueryOwner)
{
	if (USquad* Squad = Cast<USquad>(QueryOwner))
	{
		return Squad;
	}
	const AActor* Actor = Cast<AActor>(QueryOwner);
	const USquadMemberComponent* Member = Actor ? Actor->FindComponentByClass<USquadMemberComponent>() : nullptr;
	return Member ? Member->GetSquad() : nullptr;
}

void USquadQueryContext_Threat::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const USquad* Squad = ResolveSquad(QueryInstance.Owner.Get());
	if (Squad && Squad->HasThreat())
	{
		// The threat's position on the floor. The tests that use it add their own height (02 section 7).
		UEnvQueryItemType_Point::SetContextHelper(ContextData, Squad->GetThreatLocation());
	}
}
