// SquadQueryCore - EQS context: where every member of the querying squad stands.

#pragma once

#include "EnvironmentQuery/EnvQueryContext.h"
#include "SquadQueryContext_Members.generated.h"

#define UE_API SQUADQUERYCORE_API

/**
 * Provides one point per member of the querying USquad. Used as the origin of the shared query's generator: with the
 * engine Smart Object generator's multi-origin support (01b finding 10), the squad's candidates are exactly the union of
 * what each member's own query would have generated, with the same box size. That keeps the comparison with the
 * per-agent baselines fair (02 section 4.1).
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Squad Members"))
class USquadQueryContext_Members : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	UE_API virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

#undef UE_API
