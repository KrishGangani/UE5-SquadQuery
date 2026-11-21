// P07 SquadQuery (demo) - the EQS queries of the benchmark, built in C++.

#pragma once

#include "CoreMinimal.h"

class UEnvQuery;

/**
 * The cover queries of 02 section 7, built from code with public EQS API (01b finding 17): every test and threshold is
 * visible in review and identical across configurations. A shipping game would author the same query as an asset.
 */
namespace SQQueries
{
	/** Half-size of the box around each origin, cm: about three cover blocks each way (02 section 7). */
	inline const FVector BoxHalfExtentCm(1500.f, 1500.f, 200.f);

	/**
	 * The per-agent query (B0, B1, B2): slots around the querier that the threat can't see and that are at least 8 m
	 * from it, scored by distance to the querier (closer is better). Run it in SingleResult mode, so the engine moves
	 * the trace filter last and stops at the first hidden slot (01b finding 3).
	 * @param bOnlyClaimable  the generator keeps only claimable slots (the engine default; true for every baseline)
	 */
	UEnvQuery* MakeAgentCoverQuery(UObject& Outer, bool bOnlyClaimable = true);
}
