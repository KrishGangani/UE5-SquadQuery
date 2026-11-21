// P07 SquadQuery (demo) - the EQS queries of the benchmark, built in C++.

#include "SQQueries.h"

#include "Engine/EngineTypes.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Trace.h"
#include "SQArenaSubsystem.h"
#include "SQCoverSlotGenerator.h"
#include "SquadQueryContext_Threat.h"

namespace SQQueries
{
	/** "The threat can't see this slot": a Visibility trace from the threat's eye to the slot at chest height must hit
	 *  something. A pure filter, so in SingleResult mode the engine can run it last as the final condition (01b #3). */
	static UEnvQueryTest_Trace* MakeHiddenFromThreatTest(UEnvQueryOption& Option)
	{
		UEnvQueryTest_Trace* Test = NewObject<UEnvQueryTest_Trace>(&Option);
		Test->Context = USquadQueryContext_Threat::StaticClass();
		Test->TraceData.TraceMode = EEnvQueryTrace::GeometryByChannel;
		Test->TraceData.TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
		Test->TraceData.TraceShape = EEnvTraceShape::Line;
		Test->TraceFromContext.DefaultValue = true;                                        // from the threat to the slot
		Test->ContextHeightOffset.DefaultValue = USQArenaSubsystem::ThreatEyeHeightCm;     // same heights as the arena's
		Test->ItemHeightOffset.DefaultValue = USQArenaSubsystem::SlotProbeHeightCm;        // visibility table (05 B5)
		Test->TestPurpose = EEnvTestPurpose::Filter;
		Test->BoolValue.DefaultValue = true;                                               // pass when the trace hits
		return Test;
	}

	/** "At least 8 m from the threat": a horizontal-distance filter. */
	static UEnvQueryTest_Distance* MakeFarFromThreatTest(UEnvQueryOption& Option)
	{
		UEnvQueryTest_Distance* Test = NewObject<UEnvQueryTest_Distance>(&Option);
		Test->DistanceTo = USquadQueryContext_Threat::StaticClass();
		Test->TestMode = EEnvTestDistance::Distance2D;
		Test->TestPurpose = EEnvTestPurpose::Filter;
		Test->FilterType = EEnvTestFilterType::Minimum;
		Test->FloatValueMin.DefaultValue = USQArenaSubsystem::MinThreatDistanceCm;   // the arena's one constant (05 B4)
		return Test;
	}

	UEnvQuery* MakeAgentCoverQuery(UObject& Outer, bool bOnlyClaimable)
	{
		UEnvQuery* Query = NewObject<UEnvQuery>(&Outer, TEXT("SQAgentCoverQuery"));
		UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query);

		USQCoverSlotGenerator* Generator = NewObject<USQCoverSlotGenerator>(Option);
		Generator->Configure(UEnvQueryContext_Querier::StaticClass(), BoxHalfExtentCm, bOnlyClaimable, /*bIncludeClaimedSlots*/ false);
		Option->Generator = Generator;

		// "Closer to me is better": the agent's personal score. In SquadQuery this term moves into the solver's cost.
		UEnvQueryTest_Distance* NearMe = NewObject<UEnvQueryTest_Distance>(Option);
		NearMe->DistanceTo = UEnvQueryContext_Querier::StaticClass();
		NearMe->TestMode = EEnvTestDistance::Distance2D;
		NearMe->TestPurpose = EEnvTestPurpose::Score;
		NearMe->ScoringEquation = EEnvTestScoreEquation::InverseLinear;

		Option->Tests.Add(MakeHiddenFromThreatTest(*Option));
		Option->Tests.Add(MakeFarFromThreatTest(*Option));
		Option->Tests.Add(NearMe);
		for (int32 Index = 0; Index < Option->Tests.Num(); ++Index)
		{
			Option->Tests[Index]->TestOrder = Index;   // as an asset would store it; the manager only rebuilds it if unset (EnvQueryManager.cpp:874-884)
		}
		Query->GetOptionsMutable().Add(Option);
		return Query;
	}
}
