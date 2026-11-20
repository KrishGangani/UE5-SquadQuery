# P07 · SquadQuery: shared tactical decisions for squads

**Author:** Krish Gangani · [github.com/KrishGangani](https://github.com/KrishGangani)

**Domain:** AI decision-making at scale · **Difficulty:** ★★★½☆ · **Engine:** UE 5.8.4

> **Status: in progress. There are no results yet.** Phases 1–4 are done: problem analysis, verified research, the final architecture, and the UE project skeleton (built and packaged with 0 errors and 0 warnings). Sections marked *(pending)* are filled in only from real measurements, in the phase named. The success criteria are **targets fixed before any code exists, not results**.

| Phase | 1 Problem | 2 Research | 3 Architecture | 4 Foundation | 5 Implementation | 6 Explanation | 7 Testing | 8 Profiling | 9 Review | 10 Portfolio |
|---|---|---|---|---|---|---|---|---|---|---|
| State | ✓ | ✓ | ✓ final | ✓ | next (S1 of 8) | ongoing | — | — | — | — |

---

## Problem statement

When a squad of eight comes under fire, each soldier usually runs **its own** EQS cover query. The question "can the threat see this cover slot?" has the same answer for every squadmate, yet each one traces it again, and polled queries repeat it when nothing has changed. All queries share one engine time slice (10 ms per frame by default), so at high agent counts answers arrive late. Squadmates also choose independently, so neighbours pick the same slot. Smart Object claims stop two soldiers *holding* one slot, but the first caller wins, and the loser queries again. Details: [Explanation/01_Problem_Analysis.md](Explanation/01_Problem_Analysis.md).

**Corrected after reading the 5.8.4 source** ([01b](Explanation/01b_Research_Findings.md)):
- The plan said "cost is agents × items × tests". That overstates it: a single-result query moves its expensive filter (the trace) last and **stops at the first item that passes**. A per-agent cover query costs about items × cheap tests + the traces until the first hidden slot. A shared query must trace every candidate, so sharing is **not automatically cheaper**. That is now hypothesis H2, measured, not assumed.
- "EQS caches query templates, not results": **confirmed**.
- "StateTree ticks every frame" (an older forum report) is not a fixed rule in 5.8: a StateTree component can sleep until an event.
- Sight perception time-slicing: **confirmed** (6 traces per tick, 5 ms by default).

## Engineering approach

On a squad event (the threat moved, a member joined or left, a slot was lost), SquadQuery runs **one** all-matching EQS query per squad over the union of the members' nearby cover slots, filtered to "hidden from the threat, at least 8 m away". Each agent's personal cost is its distance to each slot. An exact minimum-cost assignment (the Hungarian method) gives every member a different slot, with a bonus for keeping a still-valid one. Smart Object claims keep squads from taking each other's slots. At most 4 squad evaluations start per frame, so an event doesn't become a spike. It is compared with three per-agent setups on the same arena, queries and thresholds: **B0** polled EQS, **B1** polled EQS + Smart Object claims, **B2** event-driven EQS + claims. Details: [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md).

## What already exists (and what this project adds)

The idea is **not new**. A 2014 UE4 forum design ("Frozenfire") shared squad cover scores with per-agent weights, and F.E.A.R. (2005) kept squad members off the same cover node. Today the engine has per-agent EQS and exclusive Smart Object claims; Fab and GitHub have cover-point plugins with per-point claim/release (CoverPoints, UE 5.8) and squad frameworks whose allocation method isn't described. **None of the sources reviewed publishes measurements.** This project adds an engine-native implementation on UE 5.8's own EQS and Smart Object claims, an optimal joint assignment compared with greedy claiming, and a **controlled study of cost and decision quality** at 8 / 32 / 128 agents. Gap strength: weak for the idea, medium for the measured study. Full comparison: [01b §5](Explanation/01b_Research_Findings.md).

## Architecture diagram

Final (v1.0): [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md) §5 (modules and data flow) and §6 (one threat event, step by step).

## Success criteria (targets, defined before implementation)

Mean and p99 AI ms per frame at 8 / 32 / 128 agents; item tests per second; assignment conflicts per minute (**target 0**); decision switches per agent per threat event (target ≤ 1, no oscillations); time-to-cover after a threat (p95 no worse than the best baseline, ≥ 95% in cover by the next event); solver correctness against brute force. The exact thresholds are SC1–SC7 in [01_Problem_Analysis.md §6.4](Explanation/01_Problem_Analysis.md).

## Before / after comparison

*(pending, Phase 8)*. Four configurations on one seeded arena: B0, B1, B2, SQ.

## Benchmark results

*(pending, Phase 8)*. Packaged Development build, fixed step, every run pinned to the performance cores. Raw data and the run log: [Benchmarks/](Benchmarks/README.md).

## Recorded demonstration

*(pending, Phase 10)*. B0 vs SQ on the same seed, with slot markers, target lines and a live AI-time graph. Saved in `Media/`.

## Main Unreal Engine technologies

EQS (custom contexts, the Smart Object generator), Smart Objects (slot claims), navigation (runtime navmesh, `MoveToLocation`), world subsystems, actor components, CSV profiler, Unreal Insights, Visual Logger; StateTree events as an adapter.

## Why studios care

Game AI appears in 20 of 57 job postings reviewed. Rocksteady: "coordinating combat-related and navigation related NPC behaviours". WB Games Montréal: "State Trees, EQS, navigation" and "time-slicing or behavioral LOD". Ubisoft: "AI budgets, behavioral LODs, agent counts". Epic's NPC team: "scalability of NPC systems". Sources: [Explanation/Research/01_job_market.md](Explanation/Research/01_job_market.md).

## Source code

The UE project is in [SquadQuery/](SquadQuery/) (UE 5.8.4, C++):
- the reusable system is the plugin `SquadQueryCore`;
- the arena, agents, baselines and benchmark are in the game module `SquadQuery`.

Phase 4 contains the skeleton only (descriptors, targets, modules, config). The classes are designed in [02 §4](Explanation/02_System_Architecture.md) and written step by step from S1 ([03](Explanation/03_Implementation_Roadmap.md)).

## Documentation

| File | Contents |
|---|---|
| [00_Project_Overview](Explanation/00_Project_Overview.md) | Identity, demonstration, presentation, industry relevance |
| [01_Problem_Analysis](Explanation/01_Problem_Analysis.md) | Problem, existing work, root-cause hypotheses H1–H7, success criteria SC1–SC7, measurement plan |
| [01b_Research_Findings](Explanation/01b_Research_Findings.md) | Phase 2: verified engine behaviour (UE 5.8.4), existing work, and what it changed in the design |
| [02_System_Architecture](Explanation/02_System_Architecture.md) | Final v1.0: modules, classes, data flow, defaults, safety rules, rejected alternatives |
| [03_Implementation_Roadmap](Explanation/03_Implementation_Roadmap.md) | Small, ordered build steps; baselines captured before SquadQuery code |
| [04](Explanation/04_CPP_Fundamentals.md) · [05](Explanation/05_Line_By_Line_Code_Explanation.md) · [06](Explanation/06_Unreal_APIs_And_Macros.md) · [07](Explanation/07_Common_Mistakes.md) | C++ fundamentals, line-by-line code, APIs and macros, common mistakes. Written with the code (current to Phase 4) |
| [09](Explanation/09_Testing_And_Benchmarks.md) | Every check so far, with its command and result |
| [08](Explanation/08_Debugging_And_Profiling.md) · [10](Explanation/10_Interview_Questions.md) · [11](Explanation/11_Optimization_Decisions.md) · [12](Explanation/12_Future_Scalability.md) | Debugging and profiling, interview questions, optimisation decisions, future scalability. Filled in during Phases 7–10 |
| [Phase2_Raw_Notes/](Explanation/Phase2_Raw_Notes/README.md) | The Phase 2 audit trail: every engine line and web page behind `01b` |
| [Research/](Explanation/Research/) | The portfolio-wide research this project draws on: job market, technical challenges, existing solutions, local engine verification |

## Folder layout

```text
P07_SquadQuery\
├── README.md        ← this page (the project's portfolio page)
├── Explanation\     ← 00–12, Phase 2 raw notes, research copies
├── Benchmarks\      ← run protocol + Raw\ + Summaries\
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← build, package, smoke; analysis and run scripts
└── SquadQuery\      ← the Unreal Engine project
```

## License

MIT, see [LICENSE](LICENSE). Unreal Engine itself is not included and is covered by Epic Games' own license.
