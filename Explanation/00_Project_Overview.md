# 00 · Project Overview: SquadQuery

> *"This was the problem. This was my investigation. This was my solution. These were my technical decisions. This was how I measured the results."*

**Status (2026-11-21):** Phases 1–3 are done (problem in `01`, verified research in `01b`, final architecture v1.0 in `02`). Phase 4 created the UE project and plugin skeleton, built and packaged with 0 errors and 0 warnings (`09`). No gameplay code exists yet, and no results exist yet.

---

## A. Identity

| | |
|---|---|
| **Name** | SquadQuery: shared tactical decisions for squads |
| **Technical category** | AI decision-making at scale: environment queries, reservation and assignment, event-driven re-evaluation |
| **Main objective** | For squads of 8 agents under fire, evaluate the environment once per squad with EQS, add a cheap per-agent score, assign cover positions jointly and without conflicts, and re-evaluate only on events. Then **measure** cost and decision quality against the engine's per-agent options at 8 / 32 / 128 agents |
| **Difficulty** | ★★★½☆ Advanced |
| **Core UE technologies** | EQS (`UEnvQueryManager`, custom contexts, the Smart Object generator), Smart Objects (slot claims), navigation (runtime navmesh, `AAIController::MoveToLocation`), `UTickableWorldSubsystem`, `UActorComponent`, CSV profiler, Unreal Insights, Visual Logger; StateTree events as an adapter |
| **Relation to existing work** | The idea is **not new**: a 2014 UE4 forum design shared squad cover scores with per-agent weights, and F.E.A.R. (2005) kept squad members off the same cover node. SquadQuery rebuilds the idea **on UE 5.8's own EQS and Smart Object claims**, uses an optimal joint assignment, and publishes a controlled comparison of cost and decision quality. See `01` §3 and `01b` §5 |

## B–D. Problem, solution, architecture

- Problem, root-cause hypotheses, success criteria and the measurement plan: **[01_Problem_Analysis.md](01_Problem_Analysis.md)**
- What Phase 2 found in the engine source and on the web: **[01b_Research_Findings.md](01b_Research_Findings.md)**
- Solution and architecture (final v1.0): **[02_System_Architecture.md](02_System_Architecture.md)**
- Build steps: **[03_Implementation_Roadmap.md](03_Implementation_Roadmap.md)**

## E. Technical demonstration

**What the viewer sees**
- A 120 m arena with 100 low cover blocks, seen from above. Cover slots are small markers: grey when free, coloured when targeted.
- Squads of 8 agents, one colour per squad. A line from each agent to its target slot.
- A red threat marker that jumps to a new firing position every 6 s. Slots the threat can see turn dark.
- An overlay with the configuration (B0, B1, B2 or SQ), AI ms per frame, queries per second, conflicts and switches so far.

**Before** (B0, then B1 and B2)
- B0: when the threat moves, neighbours run to the **same** slot and shove each other; some agents flip between two slots as they move. AI time shows a steady load from polling.
- B1/B2: claims keep agents apart, but the losers of each claim race re-query (counted on screen), and some agents cross the squad to reach a slot a closer squadmate could have taken.

**After** (SQ)
- One shared evaluation per squad per event. Every agent gets a different slot in the same frame, and nobody gives up a valid slot unless the threat makes it unsafe.
- The overlay shows fewer item tests, zero conflicts, at most one switch per agent per event, and the time-to-cover distribution.
- If SQ does **not** beat the event-driven per-agent baseline on CPU (hypothesis H2), the demo shows that too, next to the decision-quality numbers.

**Recorded material:** a side-by-side video of B0 vs SQ on the same seed; charts of AI ms (mean, p99) and item tests per second against N; conflicts, switches and time-to-cover per configuration; one Insights capture of an event frame.

## G. Portfolio presentation (produced in Phase 10)

1. Problem statement, one paragraph (from `01` §1).
2. The investigation: hypotheses H1–H7, including the one that argues against the project (H2), and what the captures showed for each.
3. The architecture diagram (`02` §5) and the per-event flow (`02` §6).
4. Key decisions and rejected alternatives: build on EQS and Smart Objects rather than beside them; optimal assignment vs greedy claiming; event-driven with a per-frame evaluation cap; perception out of the measured loop.
5. Before/after results against **all three** baselines, with the full environment and method.
6. What didn't improve, stated plainly.
7. Video, source repository and the complete `Explanation/` folder.

## H. Industry relevance

**Skills demonstrated:**
- engine-level understanding of EQS: one global time slice, breadth-first steps, the template cache, the single-result shortcut, and the Development-build debugger;
- the Smart Object reservation model, and where it stops;
- a classic optimisation algorithm (the assignment problem) applied to gameplay, with unit tests against brute force;
- event-driven AI design, and its cost (bunched work, solved with a per-frame cap);
- measuring decision quality, not just milliseconds;
- honest positioning against prior art.

**Studios and roles that ask for this:**
- Rocksteady: "coordinating combat-related and navigation related NPC behaviours";
- WB Games Montréal: "State Trees, EQS, navigation" and "CPU/memory budgets, time-slicing or behavioral LOD";
- Ubisoft (UE AI): "AI budgets, behavioral LODs, agent counts";
- Epic's NPC team: "scalability of NPC systems";
- AI roles at CD PROJEKT RED and People Can Fly (navigation, decision-making AI, performance measurement).

Sources: [research/01](Research/01_job_market.md) (WB1, WB2, U12, E2, C3, P1).

**Interview questions it prepares you for:**
- "How does EQS actually execute a query? What happens when 200 agents run one in the same frame?"
- "Two AIs keep picking the same cover. How do you fix it, and what does each fix cost?"
- "Why aren't Smart Object claims enough on their own?"
- "Greedy vs optimal assignment: when does it matter, and how would you prove it?"
- "Polling vs events in AI: what do you gain, and what new problem do events create?"
- "How do you measure AI decision quality, not just its CPU cost?"
- "Your optimisation shares work across agents, but the per-agent query has an early-out. How do you find out which is cheaper?"
- "This idea existed in F.E.A.R. What is your contribution?"
