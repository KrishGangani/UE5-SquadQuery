// P07 SquadQuery (demo) - the engine's Smart Object EQS generator, configured from code.

#include "SQCoverSlotGenerator.h"

void USQCoverSlotGenerator::Configure(TSubclassOf<UEnvQueryContext> InOriginContext, const FVector& InHalfExtent,
	bool bInOnlyClaimable, bool bInIncludeClaimedSlots)
{
	QueryOriginContext = InOriginContext;
	QueryBoxExtent = InHalfExtent;
	bOnlyClaimable = bInOnlyClaimable;
	SmartObjectRequestFilter.bShouldIncludeClaimedSlots = bInIncludeClaimedSlots;
}
