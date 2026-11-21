// SquadQueryCore - how much work a finished EQS query did.

#include "SquadQueryWork.h"

#include "EnvironmentQuery/EnvQueryTypes.h"

FSquadQueryWork FSquadQueryWork::FromFinishedQuery(const FEnvQueryInstance& Instance)
{
	FSquadQueryWork Work;
#if USE_EQS_DEBUGGER
	// Store() writes each step's processed-item count, including partial steps of a time-sliced test
	// (EnvQueryInstance.cpp:29-37). Steps [0, NumGenerators) are generators, the rest are tests in run order.
	const FEnvQueryDebugData& Debug = Instance.DebugData;
	for (int32 Option = 0; Option < Debug.OptionStats.Num() && Option < Debug.OptionData.Num(); ++Option)
	{
		const TArray<FEnvQueryDebugProfileData::FStep>& Steps = Debug.OptionStats[Option].StepData;
		const int32 NumGenerators = FMath::Min(Debug.OptionData[Option].NumGenerators, Steps.Num());
		for (int32 Step = 0; Step < Steps.Num(); ++Step)
		{
			(Step < NumGenerators ? Work.ItemsGenerated : Work.ItemTests) += Steps[Step].NumProcessedItems;
		}
	}
#endif
	return Work;
}
