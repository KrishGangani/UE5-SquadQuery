// SquadQueryCore - automation test: the shared squad evaluation decides as the per-agent path does, for a squad of one.
// Run: Tools\run_tests.bat (editor, headless), filter "SquadQuery".

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "SquadAssignmentSolver.h"
#include "SquadQuerySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SquadQuerySameDecision
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/**
	 * What one baseline agent picks (USQAgentBrainComponent::OnQueryFinished + ShouldKeepCurrent): the nearest valid
	 * slot, unless its current slot is still valid and at most KeepCurrentBonusCm farther. Slots claimed by someone else
	 * are not valid (the per-agent generator drops them). Current = INDEX_NONE when it has no valid slot.
	 */
	int32 PerAgentChoice(const TArray<float>& Distance, const TArray<bool>& ClaimedByOther, int32 Current)
	{
		int32 Nearest = INDEX_NONE;
		for (int32 Slot = 0; Slot < Distance.Num(); ++Slot)
		{
			if (!ClaimedByOther[Slot] && (Nearest == INDEX_NONE || Distance[Slot] < Distance[Nearest]))
			{
				Nearest = Slot;
			}
		}
		if (Nearest == INDEX_NONE || Nearest == Current)
		{
			return Nearest == INDEX_NONE ? Current : Nearest;
		}
		const bool bKeep = Current != INDEX_NONE && Distance[Current] <= Distance[Nearest] + USquadQuerySubsystem::KeepCurrentBonusCm;
		return bKeep ? Current : Nearest;
	}

	/** What the squad subsystem picks for a squad of one (USquadQuerySubsystem::Assign): the solver over the same costs. */
	int32 SquadChoice(const TArray<float>& Distance, const TArray<bool>& ClaimedByOther, int32 Current)
	{
		TArray<float> Cost;
		for (int32 Slot = 0; Slot < Distance.Num(); ++Slot)
		{
			Cost.Add(ClaimedByOther[Slot] ? FSquadAssignmentSolver::ForbiddenCost : USquadQuerySubsystem::MemberSlotCost(Distance[Slot], Slot == Current));
		}
		TArray<int32> Choice;
		FSquadAssignmentSolver::Solve(Cost, 1, Distance.Num(), Choice);
		// A result with no usable slot keeps the current assignment, as a failed per-agent query keeps its target.
		return Choice[0] == INDEX_NONE ? Current : Choice[0];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadSameDecisionAsPerAgentTest, "SquadQuery.Squad.SameDecisionAsPerAgent", SquadQuerySameDecision::Flags)
bool FSquadSameDecisionAsPerAgentTest::RunTest(const FString& Parameters)
{
	// 10,000 random decisions: 1-16 hidden slots up to 30 m away, about one in five claimed by another squad, and the
	// member either without a slot or on one of them (distances near the 3 m bonus edge included by the range).
	FRandomStream Stream(11);
	int32 Differences = 0;
	int32 Keeps = 0;
	for (int32 Trial = 0; Trial < 10000; ++Trial)
	{
		const int32 NumSlots = Stream.RandRange(1, 16);
		TArray<float> Distance;
		TArray<bool> ClaimedByOther;
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			Distance.Add(Stream.FRandRange(0.f, 3000.f));
			ClaimedByOther.Add(Stream.RandRange(0, 4) == 0);
		}
		int32 Current = Stream.RandRange(-1, NumSlots - 1);
		if (Current != INDEX_NONE && ClaimedByOther[Current])
		{
			Current = INDEX_NONE;   // a member's own slot is claimed by the member, not by someone else
		}
		const int32 Expected = SquadQuerySameDecision::PerAgentChoice(Distance, ClaimedByOther, Current);
		const int32 Actual = SquadQuerySameDecision::SquadChoice(Distance, ClaimedByOther, Current);
		Differences += Expected == Actual ? 0 : 1;
		Keeps += (Current != INDEX_NONE && Expected == Current) ? 1 : 0;
	}
	TestEqual(TEXT("decisions where the squad path differs from the per-agent path"), Differences, 0);
	TestTrue(TEXT("the keep-current rule was exercised"), Keeps > 100);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
