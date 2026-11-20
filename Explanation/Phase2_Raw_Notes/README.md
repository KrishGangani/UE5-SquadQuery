# Phase 2 raw notes

These are the working notes behind [01b_Research_Findings.md](../01b_Research_Findings.md). They are kept so every statement in the findings can be traced back to the exact engine line or web page it came from.

| File | Scope |
|---|---|
| [A_eqs_internals.md](A_eqs_internals.md) | How EQS runs a query: the global time slice, breadth-first stepping, the template cache, test sorting and the single-result shortcut, the Development-build debugger, building queries in C++ |
| [B_claims_statetree_perception.md](B_claims_statetree_perception.md) | Smart Object claims and the EQS Smart Object generator, runtime Smart Objects, StateTree events and scheduled ticking, sight time-slicing, async traces, debugging tools |
| [C_navigation_and_measurement.md](C_navigation_and_measurement.md) | A navmesh for geometry built at runtime (runtime generation, bounds without a placed volume), the measurement facts reused from UE5-UpdateBudget, what counts as AI time |
| [D_existing_work.md](D_existing_work.md) | Fab and GitHub cover plugins, a 2014 forum design, F.E.A.R. and Killzone, assignment algorithms, the remaining gap |

**The findings file is authoritative.** These notes are the audit trail.
