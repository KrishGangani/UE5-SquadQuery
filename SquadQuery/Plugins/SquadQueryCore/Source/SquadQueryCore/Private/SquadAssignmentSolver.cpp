// SquadQueryCore - minimum-cost assignment of squad members to candidate slots.

#include "SquadAssignmentSolver.h"

float FSquadAssignmentSolver::Solve(TConstArrayView<float> Cost, int32 NumAgents, int32 NumCandidates, TArray<int32>& OutCandidateForAgent)
{
	OutCandidateForAgent.Init(INDEX_NONE, FMath::Max(NumAgents, 0));
	const int32 N = NumAgents;
	const int32 M = FMath::Max(NumCandidates, 0);
	if (N <= 0)
	{
		return 0.f;
	}
	// A matrix of the wrong size is a caller bug, not "everything forbidden": say so (in int64, so the product can't
	// overflow into a passing check), and leave everyone unassigned. A NaN is a caller bug too (a distance computed
	// from a bad location); it is treated as forbidden, but reported once.
	if (!ensureMsgf(int64(Cost.Num()) == int64(N) * M, TEXT("FSquadAssignmentSolver: %d costs for %d agents x %d candidates."),
		Cost.Num(), N, NumCandidates))
	{
		return 0.f;
	}
	ensureMsgf(!Cost.ContainsByPredicate([](float Value) { return FMath::IsNaN(Value); }), TEXT("FSquadAssignmentSolver: a cost is NaN."));

	// Columns 1..M are the candidates, M+1..M+N the "no slot" columns. Rows and columns are 1-based; index 0 is the
	// algorithm's virtual start. Potentials and reduced costs are double, so sums of centimetre costs stay exact.
	// Inline storage covers a squad of 8 with up to ~150 candidates without touching the heap; larger inputs spill to it.
	const int32 Columns = M + N;
	constexpr int32 InlineColumns = 160;
	TArray<double, TInlineAllocator<InlineColumns>> ColumnPotential;   ColumnPotential.Init(0.0, Columns + 1);
	TArray<double, TInlineAllocator<InlineColumns>> MinReduced;        MinReduced.SetNumUninitialized(Columns + 1);
	TArray<int32, TInlineAllocator<InlineColumns>> RowOfColumn;        RowOfColumn.Init(0, Columns + 1);    // 0 = column free
	TArray<int32, TInlineAllocator<InlineColumns>> PreviousColumn;     PreviousColumn.Init(0, Columns + 1);
	TArray<bool, TInlineAllocator<InlineColumns>> Visited;             Visited.SetNumUninitialized(Columns + 1);
	TArray<double, TInlineAllocator<32>> RowPotential;                 RowPotential.Init(0.0, N + 1);

	// Raw pointers in the loops: in Development builds every TArray index is range-checked, and these loops run
	// O(n^2 (m + n)) times inside the measured AI time. Every index below is in range by construction.
	const float* CostData = Cost.GetData();
	double* const V = ColumnPotential.GetData();
	double* const U = RowPotential.GetData();
	double* const MinR = MinReduced.GetData();
	int32* const RowOf = RowOfColumn.GetData();
	int32* const Prev = PreviousColumn.GetData();
	bool* const Used = Visited.GetData();

	for (int32 Row = 1; Row <= N; ++Row)
	{
		// Grow a shortest augmenting path from this row (Dijkstra over reduced costs), then flip it.
		RowOf[0] = Row;
		int32 Column0 = 0;
		for (int32 Column = 0; Column <= Columns; ++Column)
		{
			MinR[Column] = TNumericLimits<double>::Max();
			Used[Column] = false;
		}
		do
		{
			Used[Column0] = true;
			const int32 Row0 = RowOf[Column0];
			const double RowPotential0 = U[Row0];
			const float* RowCosts = CostData + int64(Row0 - 1) * M;
			double Delta = TNumericLimits<double>::Max();
			int32 Column1 = 0;
			for (int32 Column = 1; Column <= Columns; ++Column)
			{
				if (Used[Column])
				{
					continue;
				}
				// A forbidden pair is a missing edge: never relaxed. The "no slot" columns are always reachable, so the
				// search still finishes.
				const float PairCost = Column <= M ? RowCosts[Column - 1] : UnassignedCost;
				if (!IsForbidden(PairCost))
				{
					const double Reduced = double(PairCost) - RowPotential0 - V[Column];
					if (Reduced < MinR[Column])
					{
						MinR[Column] = Reduced;
						Prev[Column] = Column0;
					}
				}
				if (MinR[Column] < Delta)
				{
					Delta = MinR[Column];
					Column1 = Column;
				}
			}
			for (int32 Column = 0; Column <= Columns; ++Column)
			{
				if (Used[Column])
				{
					U[RowOf[Column]] += Delta;
					V[Column] -= Delta;
				}
				else
				{
					MinR[Column] -= Delta;
				}
			}
			Column0 = Column1;
		}
		while (RowOf[Column0] != 0);

		do
		{
			const int32 Column1 = Prev[Column0];
			RowOf[Column0] = RowOf[Column1];
			Column0 = Column1;
		}
		while (Column0 != 0);
	}

	// Real columns hold the answer; agents left on a "no slot" column stay INDEX_NONE. A forbidden pair can't be here:
	// every assigned pair was reached through a relaxed edge.
	double Total = 0.0;
	for (int32 Column = 1; Column <= M; ++Column)
	{
		if (const int32 Row = RowOf[Column]; Row != 0)
		{
			OutCandidateForAgent[Row - 1] = Column - 1;
			Total += CostData[int64(Row - 1) * M + (Column - 1)];
		}
	}
	return static_cast<float>(Total);
}
