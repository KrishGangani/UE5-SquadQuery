# 12 · Future Scalability: SquadQuery

How the system would grow, in the order the evidence suggests. The starter list is in `02` §11.

| Step | What | Why, from the measurements |
|---|---|---|
| 1 | **Finish the shared squad evaluation** (S5): `USquadQuerySubsystem` as a tickable event queue, the all-matching squad query over the members' union, the solver, claims, `OnAssignmentChanged`. Drafted, not yet built into the measured game | Without it, SC1–SC6 can't be measured. The baselines are captured and waiting (`09` S3) |
| 2 | **Cap work per frame by time, not by count** | B2's event frames reach 8.35 ms at N = 128 (`09` S3). A fixed `MaxEvaluationsPerFrame` limits how many evaluations start. A frame-time budget (UE5-UpdateBudget's scheduler) would limit what they cost |
| 3 | **Share visibility across squads** | In the benchmark every squad faces the same threat, so a per-event cache of slot visibility would let squads share traces. This holds only while squads share a threat |
| 4 | **Several threats, squad roles** | The threat context returns several points, and the trace test's multiple-context filter decides "hidden from all". Roles add per-member cost terms. The solver stays the same |
| 5 | **Path-based personal cost** | Straight-line distance makes the same error in every configuration. One navmesh search per agent would replace it |
| 6 | **Async traces** | Move the visibility traces off the game thread, at the cost of one frame of latency (`01b` §3) |
| 7 | **StateTree adapter** | One `SendStateTreeEvent` per assignment change, so a sleeping StateTree wakes only when its orders change (`01b` #12) |
| 8 | **Dedicated server** | Nothing renders, and assignments need no replication because movement replicates |

**Limits that don't go away:**
- The solver is O(n²(m + n)) per squad: fine for squads of 8–16, not for a 200-agent "squad".
- In Development builds, EQS bookkeeping scales with the number of query owners (`01b` #22). Per-agent designs must therefore be compared in a build where that overhead is measured or removed.
