// SquadQueryCore - EQS context: where the querier's squad believes its threat is.

#pragma once

#include "EnvironmentQuery/EnvQueryContext.h"
#include "SquadQueryContext_Threat.generated.h"

#define UE_API SQUADQUERYCORE_API

class USquad;

/**
 * Provides one point: the threat location of the querier's squad. The querier is either a USquad (a squad's shared
 * query, S5) or an actor with a USquadMemberComponent (a per-agent query). Every configuration reads the threat through
 * this one context, so the input is identical (02 section 4.1). No point is provided if the squad has no threat yet.
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Squad Threat"))
class USquadQueryContext_Threat : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	UE_API virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

	/** The squad a query owner speaks for: the owner itself if it is a USquad, otherwise its member component's squad. */
	static UE_API USquad* ResolveSquad(UObject* QueryOwner);
};

#undef UE_API
