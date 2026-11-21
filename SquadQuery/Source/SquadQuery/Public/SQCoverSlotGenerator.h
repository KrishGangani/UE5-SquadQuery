// P07 SquadQuery (demo) - the engine's Smart Object EQS generator, configured from code.

#pragma once

#include "EnvQueryGenerator_SmartObjects.h"
#include "SQCoverSlotGenerator.generated.h"

/**
 * Exactly the engine's UEnvQueryGenerator_SmartObjects (01b finding 10): it lists Smart Object slots in a box around
 * each origin the context gives. Its settings are protected (01b finding 17), so this subclass sets them from code.
 * Every query of every configuration uses it, so the item source is identical (02 section 7).
 */
UCLASS(HideDropdown)
class USQCoverSlotGenerator : public UEnvQueryGenerator_SmartObjects
{
	GENERATED_BODY()

public:
	/**
	 * @param InOriginContext        where the boxes are centred (the querier, or every member of a squad)
	 * @param InHalfExtent           half-size of each box (cm); the engine tests each smart object's own location against
	 *                               it, so slots can lie up to a block's slot radius outside the box (01b finding 18)
	 * @param bInOnlyClaimable       keep only slots CanBeClaimed() returns true for at generation time (engine default: true)
	 * @param bInIncludeClaimedSlots let the spatial query return claimed slots (engine default: false)
	 */
	void Configure(TSubclassOf<UEnvQueryContext> InOriginContext, const FVector& InHalfExtent, bool bInOnlyClaimable,
		bool bInIncludeClaimedSlots);
};
