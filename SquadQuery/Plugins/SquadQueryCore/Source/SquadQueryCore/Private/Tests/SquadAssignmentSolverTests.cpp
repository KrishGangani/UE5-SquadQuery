// SquadQueryCore - automation tests for FSquadAssignmentSolver, against brute force (no world needed).
// Run: Tools\run_tests.bat (editor, headless), filter "SquadQuery.Solver".

#include "Misc/AutomationTest.h"
#include "Algo/Count.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "SquadAssignmentSolver.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace SquadQueryTests
{
	/** Editor and game contexts; one filter (ProductFilter), as the macro requires (AutomationTest.h:4127-4133). */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** The solver's objective: real pairs at their cost, unassigned agents at UnassignedCost. */
	double Objective(const TArray<float>& Cost, int32 NumCandidates, const TArray<int32>& Assignment)
	{
		double Total = 0.0;
		for (int32 Agent = 0; Agent < Assignment.Num(); ++Agent)
		{
			Total += Assignment[Agent] == INDEX_NONE ? FSquadAssignmentSolver::UnassignedCost : Cost[Agent * NumCandidates + Assignment[Agent]];
		}
		return Total;
	}

	/** Every injective partial assignment, recursively; forbidden pairs skipped. Returns the best objective. */
	double BruteForce(const TArray<float>& Cost, int32 NumAgents, int32 NumCandidates, int32 Agent = 0, uint32 UsedMask = 0, double SoFar = 0.0)
	{
		if (Agent == NumAgents)
		{
			return SoFar;
		}
		double Best = BruteForce(Cost, NumAgents, NumCandidates, Agent + 1, UsedMask, SoFar + FSquadAssignmentSolver::UnassignedCost);
		for (int32 Candidate = 0; Candidate < NumCandidates; ++Candidate)
		{
			const float PairCost = Cost[Agent * NumCandidates + Candidate];
			if ((UsedMask & (1u << Candidate)) == 0 && !FSquadAssignmentSolver::IsForbidden(PairCost))
			{
				Best = FMath::Min(Best, BruteForce(Cost, NumAgents, NumCandidates, Agent + 1, UsedMask | (1u << Candidate), SoFar + PairCost));
			}
		}
		return Best;
	}

	/** No candidate twice, and no forbidden pair (at most 8 candidates here, so a bit mask, as in BruteForce). */
	bool IsFeasible(const TArray<float>& Cost, int32 NumCandidates, const TArray<int32>& Assignment)
	{
		uint32 UsedMask = 0;
		for (int32 Agent = 0; Agent < Assignment.Num(); ++Agent)
		{
			const int32 Candidate = Assignment[Agent];
			if (Candidate == INDEX_NONE)
			{
				continue;
			}
			if ((UsedMask & (1u << Candidate)) != 0 || FSquadAssignmentSolver::IsForbidden(Cost[Agent * NumCandidates + Candidate]))
			{
				return false;
			}
			UsedMask |= 1u << Candidate;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadSolverMatchesBruteForceTest, "SquadQuery.Solver.MatchesBruteForce", SquadQueryTests::Flags)
bool FSquadSolverMatchesBruteForceTest::RunTest(const FString& Parameters)
{
	// SC7: 1,000 random instances, up to 6 agents and 8 candidates, about one pair in ten forbidden.
	FRandomStream Stream(7);
	int32 Mismatches = 0;
	int32 WrongTotals = 0;
	int32 Infeasible = 0;
	for (int32 Trial = 0; Trial < 1000; ++Trial)
	{
		const int32 NumAgents = Stream.RandRange(1, 6);
		const int32 NumCandidates = Stream.RandRange(0, 8);
		TArray<float> Cost;
		Cost.SetNumUninitialized(NumAgents * NumCandidates);
		for (float& Value : Cost)
		{
			const bool bForbidden = Stream.RandRange(0, 9) == 0;
			const float Distance = FMath::RoundToFloat(Stream.FRandRange(0.f, 3000.f));
			Value = bForbidden ? FSquadAssignmentSolver::ForbiddenCost : Distance;
		}
		TArray<int32> Assignment;
		const float Returned = FSquadAssignmentSolver::Solve(Cost, NumAgents, NumCandidates, Assignment);
		Infeasible += SquadQueryTests::IsFeasible(Cost, NumCandidates, Assignment) ? 0 : 1;
		const double Found = SquadQueryTests::Objective(Cost, NumCandidates, Assignment);
		const double Best = SquadQueryTests::BruteForce(Cost, NumAgents, NumCandidates);
		Mismatches += FMath::IsNearlyEqual(Found, Best, 1e-2) ? 0 : 1;
		// The returned total is the real pairs only: the objective minus the unassigned agents' padding.
		const int32 Unassigned = static_cast<int32>(Algo::Count(Assignment, INDEX_NONE));
		WrongTotals += FMath::IsNearlyEqual(double(Returned), Found - Unassigned * double(FSquadAssignmentSolver::UnassignedCost), 1e-2) ? 0 : 1;
	}
	TestEqual(TEXT("instances where the solver's objective differs from brute force"), Mismatches, 0);
	TestEqual(TEXT("instances where the returned total isn't the sum of the chosen pairs"), WrongTotals, 0);
	TestEqual(TEXT("instances with a slot used twice or a forbidden pair chosen"), Infeasible, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadSolverKnownOptimumTest, "SquadQuery.Solver.KnownOptimum", SquadQueryTests::Flags)
bool FSquadSolverKnownOptimumTest::RunTest(const FString& Parameters)
{
	// Greedy (each agent in order takes its cheapest free slot) picks 0->0 (1), then 1->1 (9): total 10.
	// The optimum is 0->1, 1->0 (2 + 2 = 4). The squad case the solver exists for (02 section 2).
	const TArray<float> Cost = { 1.f, 2.f,
	                             2.f, 9.f };
	TArray<int32> Assignment;
	const float Total = FSquadAssignmentSolver::Solve(Cost, 2, 2, Assignment);
	TestEqual(TEXT("assignment"), Assignment, TArray<int32>({ 1, 0 }));
	TestEqual(TEXT("total"), Total, 4.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadSolverEdgeCasesTest, "SquadQuery.Solver.EdgeCases", SquadQueryTests::Flags)
bool FSquadSolverEdgeCasesTest::RunTest(const FString& Parameters)
{
	TArray<int32> Assignment;

	// No agents.
	TestEqual(TEXT("no agents: total"), FSquadAssignmentSolver::Solve({}, 0, 5, Assignment), 0.f);
	TestEqual(TEXT("no agents: empty result"), Assignment.Num(), 0);

	// No candidates: everyone unassigned.
	FSquadAssignmentSolver::Solve({}, 3, 0, Assignment);
	TestEqual(TEXT("no candidates"), Assignment, TArray<int32>({ INDEX_NONE, INDEX_NONE, INDEX_NONE }));

	// More agents than candidates: the cheapest agent gets the only slot.
	FSquadAssignmentSolver::Solve({ 5.f, 1.f, 3.f }, 3, 1, Assignment);
	TestEqual(TEXT("three agents, one slot"), Assignment, TArray<int32>({ INDEX_NONE, 0, INDEX_NONE }));

	// Everything forbidden: nobody is put on a forbidden slot.
	const float F = FSquadAssignmentSolver::ForbiddenCost;
	const float Total = FSquadAssignmentSolver::Solve({ F, F, F, F }, 2, 2, Assignment);
	TestEqual(TEXT("all forbidden"), Assignment, TArray<int32>({ INDEX_NONE, INDEX_NONE }));
	TestEqual(TEXT("all forbidden: total"), Total, 0.f);

	// A forbidden pair is avoided even when it is the agent's only cheap option.
	FSquadAssignmentSolver::Solve({ F, 50.f,
	                                1.f, 2.f }, 2, 2, Assignment);
	TestEqual(TEXT("forbidden avoided"), Assignment, TArray<int32>({ 1, 0 }));

	// Ties are broken the same way every time: the lowest column wins, row by row (the strict < in the column search).
	// Pinned to the exact answer, so a change of tie-breaking (and of the benchmark's run-to-run behaviour) fails here.
	const TArray<float> Ties = { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	FSquadAssignmentSolver::Solve(Ties, 3, 3, Assignment);
	TestEqual(TEXT("ties"), Assignment, TArray<int32>({ 0, 1, 2 }));

	// +inf is a way to say "forbidden": never chosen, and the search still finishes.
	FSquadAssignmentSolver::Solve({ -std::numeric_limits<float>::infinity(), 1.f, 2.f, 3.f }, 2, 2, Assignment);
	TestNotEqual(TEXT("a non-finite cost is never chosen"), Assignment[0], 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSquadSolverTimingTest, "SquadQuery.Solver.Timing8x128", SquadQueryTests::Flags)
bool FSquadSolverTimingTest::RunTest(const FString& Parameters)
{
	// The size of one squad evaluation in the benchmark: 8 members, about 128 candidate slots (03, S4). Timed in the
	// Development editor, so it is an order of magnitude, not a benchmark result; the packaged runs time every solve.
	constexpr int32 NumAgents = 8;
	constexpr int32 NumCandidates = 128;
	constexpr int32 Repeats = 2000;
	FRandomStream Stream(11);
	TArray<float> Cost;
	Cost.SetNumUninitialized(NumAgents * NumCandidates);
	for (float& Value : Cost)
	{
		Value = FMath::RoundToFloat(Stream.FRandRange(0.f, 3000.f));
	}
	TArray<int32> Assignment;
	double Checksum = 0.0;
	const double Start = FPlatformTime::Seconds();
	for (int32 Repeat = 0; Repeat < Repeats; ++Repeat)
	{
		Checksum += FSquadAssignmentSolver::Solve(Cost, NumAgents, NumCandidates, Assignment);
	}
	const double MicrosecondsPerSolve = (FPlatformTime::Seconds() - Start) * 1e6 / Repeats;
	AddInfo(FString::Printf(TEXT("Solve 8 x 128: %.2f us per call (mean of %d; checksum %.0f)"), MicrosecondsPerSolve, Repeats, Checksum));
	TestFalse(TEXT("an agent left without a slot"), Assignment.Contains(INDEX_NONE));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
