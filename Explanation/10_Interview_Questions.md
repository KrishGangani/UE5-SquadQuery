# 10 · Interview Questions: SquadQuery

Questions an interviewer could ask about this project, with answers grounded in what was built and measured (`09`). Numbers are medians of 3 packaged runs at N = 128 unless stated.

**1. Why not just give every soldier its own EQS cover query?**
That is the baseline, and it works. Its cost grows with N and repeats for squadmates: at 128 agents the polled per-agent query (B0) runs 256 queries and about 21,400 item tests per second for 0.43 ms of AI time per frame, and squadmates pick the same slot (160 conflict onsets per minute). SquadQuery asks whether a squad can answer the agent-independent part once and assign slots jointly.

**2. EQS caches templates. Does it cache results?**
No (`01b` #4). Two agents asking the same question get two evaluations. A `SingleResult` query also moves its most expensive filter (the trace) last and stops at the first item that passes (`01b` #3), so a per-agent query is cheaper than "items × tests" suggests. A shared query has to trace every candidate, so sharing is not automatically cheaper. That is hypothesis H2.

**3. Smart Object claims already stop two agents holding one slot. What is left?**
Who gets which slot. Claims are first come, first served. With claims, conflicts drop to 0, but the races turn into claim failures. The event-driven baseline B2 loses 151 claims per minute at N = 128, because every agent queries on the same frame from the same picture. Each loss is a repeated query (H5).

**4. Polling vs events: what did the baselines show?**
Polling repeats work when nothing has changed. With the threat moving every 6 s, B0 costs 16× B2's mean AI time (0.426 vs 0.026 ms) and 14× its item tests, and still gets worse cover. But event-driven work bunches up. B2's event frames reach 8.35 ms at N = 128 while its p99 is 0.05 ms, because event frames are under 1% of a capture. The lesson: judge event-driven systems on p99.9 and event-window maxima, not on p99.

**5. Why the Hungarian method and not greedy assignment?**
Greedy claiming depends on the order and gives no optimality guarantee. A 2 × 2 case in the tests gives 10 against an optimum of 4. The shortest-augmenting-path Hungarian method is exact and costs O(n²(m + n)): a few thousand inner steps for 8 × 128. It is also testable, because its total must equal brute force on 1,000 random instances.

**6. How do you handle more agents than slots, and slots an agent may not take?**
With padding: n extra "no slot" columns at a high fixed cost, so the search always finishes and an agent with no allowed slot ends up unassigned. Forbidden pairs (a slot another squad holds) are missing edges that the search never relaxes. Non-finite costs count as forbidden. The review found that a single `-inf` would otherwise make the search loop forever.

**7. How do you measure "AI time" fairly across four designs?**
The sum, per frame, of three exclusive CSV stats:
- one stat of our own around every piece of decision code;
- EQS's own stat;
- the engine's `Pathfinding` stat.

The last one matters. `MoveToLocation`'s path query opens its own exclusive scope inside ours, so without it, designs that switch slots often would look cheaper. The metrics run in their own scope, outside AI time, and never trace: a visibility table is built once.

**8. What surprised you in the engine?**
- A navmesh bounds volume spawned at runtime has its bounds cached as a single point until you call `UpdateBounds()`, and the build-finished event also fires for that near-empty build (`01b` #20).
- The editor and the packaged game round floats differently (`/fp:precise` vs `/fp:fast`), so the arena hash repeats only within one binary (#21).
- The EQS debugger's per-query bookkeeping walks every query owner, which favours designs with few owners in Development builds (#22).

**9. Isn't this a known idea?**
Yes. A 2014 UE4 forum design shared squad cover scores, and F.E.A.R. kept squadmates off the same cover node. This project adds three things: an engine-native implementation on UE 5.8's own EQS and Smart Objects, an exact joint assignment, and published measurements. The gap is weak for the idea and medium for the study.

**10. What is not finished?**
The squad-level shared evaluation (S5) is designed and drafted, but it is not in the measured game, so the comparison with B0–B2 (SC1–SC6) was not measured. The solver is built and unit-tested. `09` marks every unmeasured criterion as such.
