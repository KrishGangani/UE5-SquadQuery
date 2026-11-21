// SquadQueryCore - how much work a finished EQS query did (for the "item tests per second" metric).

#pragma once

#include "CoreMinimal.h"

struct FEnvQueryInstance;

/** Items an EQS query generated, and item tests it ran (one item through one test = one item test). */
struct FSquadQueryWork
{
	int32 ItemsGenerated = 0;
	int32 ItemTests = 0;

	/**
	 * Read from the instance's EQS debug data (01b finding 6): per option, the generator steps count generated items
	 * and every later step counts the items its test processed. That data exists only when USE_EQS_DEBUGGER is on
	 * (every build except Test and Shipping) and FEnvQueryInstance::bDebuggingInfoEnabled is true (the default);
	 * otherwise the result is zero. Call it on a finished instance you kept from PrepareQueryInstance.
	 */
	static SQUADQUERYCORE_API FSquadQueryWork FromFinishedQuery(const FEnvQueryInstance& Instance);
};
