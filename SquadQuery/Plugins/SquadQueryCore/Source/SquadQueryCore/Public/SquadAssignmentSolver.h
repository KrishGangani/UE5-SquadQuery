// SquadQueryCore - minimum-cost assignment of squad members to candidate slots. Plain C++: no UObject, no world.

#pragma once

#include "CoreMinimal.h"

/**
 * The assignment problem: give each agent at most one candidate and each candidate at most one agent, so that the
 * total cost is as small as possible. Solved exactly with the Hungarian method in its shortest-augmenting-path form
 * (Kuhn 1955, Munkres 1957; 02 section 2), O(n^2 (m + n)) for n agents and m candidates.
 *
 * How the awkward cases are handled:
 *  - A forbidden pair (IsForbidden: a cost >= ForbiddenCost, or one that is not a finite number) is a missing edge: the
 *    search never relaxes it, so it can't be chosen (for example a slot another squad holds).
 *  - The matrix gets n extra "no slot" columns (one per agent, shared: any agent may take any of them), each costing
 *    UnassignedCost for everyone. It keeps the search finite (every agent can always reach one), and it is worse than
 *    any allowed pair, so the solver first gives as many agents as possible a real slot, then minimises the total. The
 *    same padding makes n > m work.
 * The "as many as possible" guarantee holds while allowed costs stay well below UnassignedCost: a pair costing
 * UnassignedCost or more is never preferred to leaving the agent unassigned. The squad system's costs are centimetres,
 * at most a few hundred metres (1e4-1e5), against 1e7.
 */
struct FSquadAssignmentSolver
{
	static constexpr float ForbiddenCost = 1.0e9f;
	static constexpr float UnassignedCost = 1.0e7f;

	/** The one rule for "this pair may not be chosen". +inf is a valid way to say it; NaN is a caller bug (Solve ensures). */
	static bool IsForbidden(float PairCost) { return !FMath::IsFinite(PairCost) || PairCost >= ForbiddenCost; }

	/**
	 * @param Cost                  row-major NumAgents x NumCandidates (a mismatch ensures and leaves everyone unassigned)
	 * @param OutCandidateForAgent  per agent, the chosen candidate index, or INDEX_NONE
	 * @return the total cost of the chosen pairs
	 */
	static SQUADQUERYCORE_API float Solve(TConstArrayView<float> Cost, int32 NumAgents, int32 NumCandidates, TArray<int32>& OutCandidateForAgent);
};
