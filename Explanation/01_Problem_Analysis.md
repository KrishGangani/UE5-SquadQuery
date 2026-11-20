# 01 · Problem Analysis: SquadQuery

**Phase:** 1 (Problem Analysis), written 2026-11-21 from the Step 0 plan; updated the same day after Phase 2 (evidence in [01b_Research_Findings.md](01b_Research_Findings.md)). Two Phase 2 findings changed this file: the single-result shortcut (§4, H2) and the existing designs (§3).
**Engine:** UE **5.8.4** (CL 58902052).
**Status of every claim:** **[verified]** (read in engine source or a cited source), **[hypothesis]** (to be confirmed by measurement), or **[target]** (a pass/fail threshold, not a result).

---

## 1. The problem in one paragraph

In a shooter, a squad of eight soldiers comes under fire and must take cover. In Unreal the usual way is that **each soldier runs its own Environment Query** (EQS): generate nearby cover points, throw away the ones the threat can see, score the rest by distance, pick the best. That has two costs.

- **CPU.** The question "can the threat see this cover point?" has the same answer for every soldier, yet each soldier traces it again. Queries also re-run on a timer when nothing has changed. All of it shares one engine time slice, so at high agent counts queries queue up and answers arrive late.
- **Decision quality.** Soldiers choose independently, so neighbours pick the **same** cover and push each other out of it, or keep switching between two points. The engine's fix, Smart Object claims, stops two soldiers holding one point, but whoever asks first wins. Claims don't decide who *should* go where, and a soldier who loses the race has to query again.

SquadQuery tests whether doing the agent-independent part **once per squad**, then giving each soldier a cheap personal score and assigning positions **jointly**, lowers both costs, and **by how much**, against the engine's own tools.

## 2. Where this happens in real games

| Evidence | What it shows | Source |
|---|---|---|
| F.E.A.R. (Monolith, 2005) | A squad coordinator orders soldiers to cover and must "ensure that other A.I. are not ordered to go to the same node" | [Phase2_Raw_Notes/D](Phase2_Raw_Notes/D_existing_work.md) D4.1 (paper opened) |
| Killzone 3 bots (Guerrilla) | The squad constrains each bot's position picking, "thus improving performance" | D4.2 (chapter opened) |
| UE community, 2014 | A "squad helper object" scores cover once and shares it, because per-agent scoring was the norm | D3 (thread opened) |
| Fab, 2026 | A current combat framework still ships per-enemy EQS cover queries; a 5.8 cover plugin adds per-point claim/release so "two never share one" | D2.1, D2.2 (opened) |
| Forum, UE4 era | "Is there any way for EQS to share results?": no; EQS "does not scale well when running many instances" | [research/02](Research/02_technical_challenges.md) [T9-E] |
| Job postings, 2026 | Rocksteady: "coordinating combat-related and navigation related NPC behaviours". WB Games Montréal: "State Trees, EQS, navigation" plus "CPU/memory budgets, time-slicing or behavioral LOD". Ubisoft: "AI budgets, behavioral LODs, agent counts". Epic NPC team: "scalability of NPC systems" | [research/01](Research/01_job_market.md) §2 (WB1, WB2, U12, E2) |

**What these share:** squads must look coordinated, and the cost of AI grows with *how often* agents decide and sense, not with how clever one decision is ([research/02](Research/02_technical_challenges.md), Topic 9 takeaway).

## 3. What already exists, and what it leaves open

| Existing tool | What it does | What it doesn't do | Evidence |
|---|---|---|---|
| EQS, per agent | One query instance per querier, stepped breadth-first in a **global 10 ms/frame slice**; caches **templates, not results**; single-result queries stop the expensive filter at the first passing item | No result sharing between queriers; no notion of a squad | [verified] `01b` #1–#4 |
| Behavior Tree "Run EQS query on a regular basis" | Re-runs the query every 0.5 s ± 0.1 s | Polls whether or not anything changed | [verified] `01b` #16 |
| StateTree "Run Env Query" task | Runs a per-agent query on state entry; 5.8 trees can sleep until an event | Still one query per agent | [verified] `01b` #12; `01b` §3 |
| Smart Object claims | Exclusive slot reservation with priorities and preemption | No arbitration of who gets which slot; claimability is checked at generation, the claim comes later (race) | [verified] `01b` #7, #8 |
| AI Perception (sight) | Time-sliced sensing (6 traces/tick, 5 ms) | Not evaluation; `UAISense_Team` shares *stimuli*, not scores | [verified] `01b` #14 |
| Mass EQS (5.8) | EQS work for Mass entities in processors | Per query; Mass only | `01b` §2 |
| CoverPoints (Fab/GitHub, UE 5.8) | Cover generation, trace-free scoring, per-point claim/release | Per-agent "Find Best Cover"; no joint assignment; no published numbers | `01b` §5 |
| SquadAI (GitHub), Squad & Team Combat AI (Marketplace) | Squad cover allocation / leader orders | Method not described; no numbers | `01b` §5 |
| Frozenfire's squad helper (2014), F.E.A.R. (2005) | **Shared scoring with per-agent weights; squad-level duplicate avoidance** | Not built on today's EQS/Smart Objects; no measurements | `01b` §5 |

**The gap, after Phase 2 (honest version):**
- The *idea* is not new. Shared squad scoring and conflict-free squad selection have existed since 2005–2014.
- What none of the reviewed sources provides is (a) the idea built **on UE 5.8's own EQS and Smart Object claims** as a reusable plugin, (b) **optimal** joint assignment compared with greedy claiming, and (c) **controlled measurements of both cost and decision quality** against the engine's per-agent options at several agent counts.
- Gap strength: **weak for the idea, medium for the measured study.** SquadQuery is presented as that study plus the plugin. It is never presented as a new technique.

## 4. Root-cause hypotheses

Each hypothesis has a measurement that can confirm or reject it. The baselines are captured before any SquadQuery code exists (`03`).

| # | Hypothesis | Why we suspect it | How we test it |
|---|---|---|---|
| H1 | **Per-agent queries repeat agent-independent work.** Visibility from the threat and distance to the threat are the same for every squad member | EQS has no result sharing [verified, `01b` #4] | Item tests per second and traces per second, B0–B2 vs SQ, at N = 8 / 32 / 128 |
| H2 | **The single-result shortcut caps per-agent work**, so against an event-driven per-agent baseline sharing may *not* reduce traces. A shared query must trace every candidate; a per-agent query traces only until its first hidden item | Phase 2 code reading [verified, `01b` #3] | Traces per query (from EQS debug data) and traces per second: B2 vs SQ. **This is the main risk to SC1 and SC3, stated before measuring** |
| H3 | **Polling repeats work when nothing has changed** | The engine's default service interval is 0.5 s [verified, `01b` #16] | AI ms and item tests per second: polled (B0, B1) vs event-driven (B2) |
| H4 | **Independent choices collide.** Neighbours share the same nearest hidden cover | No coordination in per-agent EQS | Assignment conflicts per minute in B0 vs N |
| H5 | **Claims remove collisions but not their cost.** The first caller wins, results are stale by the time of the claim, and the loser queries again; the joint outcome (total and worst travel) is worse than an optimal assignment | Claimability is checked at generation, the claim comes later [verified, `01b` #8] | Claim failures per minute; time-to-cover and travel distance in B1/B2 vs SQ |
| H6 | **Event-driven work bunches into event frames.** Every squad re-evaluates in the frame the threat moves | The design itself | AI ms on event frames vs other frames; p99 per frame. SQ spreads evaluations over frames (`02` §7) |
| H7 | **The global EQS slice adds queueing latency** at high query counts, which delays decisions | One 10 ms slice for all queries, breadth first [verified, `01b` #1–#2] | Query latency (request → result, ms) distribution per configuration; its share of time-to-cover |

**Not accepted without checking:** "the GPU is the bottleneck" (each capture records GPU time), and "movement dominates AI cost" (movement is reported separately through `GameThreadTime`, never mixed into AI ms).

## 5. The cost of leaving it unsolved

- **CPU.** *Arithmetic illustration only, not a measurement:* 128 agents polling every 0.5 s run 256 queries per second. If one query costs 50 µs, that is 12.8 ms of CPU per second, about 0.21 ms per frame at 60 fps on average, but the global slice lets a burst take up to 10 ms in a single frame.
- **Quality.** Players notice two soldiers fighting over one crate, or a soldier who keeps changing his mind. Designers then add special cases, cut agent counts, or slow the AI down.
- **Latency.** When queries queue behind each other, the squad reacts late. That reads as "dumb AI" even though each decision is correct.

## 6. What "solved" means: success criteria, defined now

### 6.1 The workload (identical in every run)

- **Arena** built in code from a fixed seed: a 120 m × 120 m floor; 100 cover blocks (3.0 m × 1.0 m × 1.3 m) on a jittered 10 × 10 grid with random 90° turns; **6 cover slots per block** (two per long side, one per short side), 600 slots in total, at least 1.5 m apart. The slots are Smart Object slots. A runtime navmesh covers the arena and is fully built before agents spawn.
- **Agents:** N ∈ {8, 32, 128} in **squads of 8** (1, 4 and 16 squads), spawned in seeded clusters. Characters with the engine's character movement, walk speed 450 cm/s, moved by `AAIController::MoveToLocation`.
- **Threat:** one scripted threat that **relocates every 6 s** between 10 seeded firing positions. Each relocation is a **threat event** delivered to every squad (no perception, `01b` #14).
- **Task:** every agent must get to a cover slot the threat can't see, at least 8 m from the threat, without sharing a slot. "Hidden" = a visibility-channel trace from the threat's eye (1.7 m) to the slot (0.9 m above the floor) is blocked.
- **Run:** 10 s warm-up (excluded), then a **60 s capture = 10 threat events**.

### 6.2 Compared configurations (one build, switched by `-SQMode=`)

| ID | Configuration | Why it is included |
|---|---|---|
| B0 | **Per-agent EQS, polled**: every agent runs its own single-result cover query every 0.5 s ± 0.1 s; no reservation | The common default (a Behavior Tree "Run EQS" service) |
| B1 | **B0 + Smart Object claims**: the generator lists only claimable slots; the agent claims its pick; a failed claim triggers a new query | The engine's own fix for conflicts. Leaving it out would be unfair |
| B2 | **Per-agent EQS + claims, event-driven**: an agent queries only after a threat event or when it lost its slot | The strongest per-agent setup. It separates the effect of *events* from the effect of *sharing* |
| **SQ** | **SquadQuery**: one all-matching query per squad over the union of the members' candidate sets; per-agent cost = distance; optimal assignment; claims for exclusivity across squads; event-driven, at most 4 squad evaluations started per frame | Our system |

All four use the **same generator, the same tests, the same thresholds, the same keep-current rule** (a new slot must be at least 3 m closer before an agent gives up a valid one) and the engine's default EQS slice (10 ms, breadth first). The queries are listed in `02` §7.

### 6.3 Metrics

| Metric | Unit | How it is measured |
|---|---|---|
| **AI time per frame**: mean, p95, p99 | ms | `Exclusive/GameThread/EnvQueryManager` (engine CSV column) **+** our exclusive CSV stat around all decision code (polling, result handling, assignment, claims, `MoveToLocation`) |
| AI time on event frames vs other frames | ms | The same, split by the director's event frames |
| Frame time and `GameThreadTime`: median, p95, p99 | ms | CSV profiler |
| **Item tests per second**; traces per second; queries per second | count/s | Sum of `NumProcessedItems` over each finished query's steps (EQS debug data; Development builds only, `01b` #6) |
| Query latency (request → result) | ms | Our timestamps |
| **Assignment conflicts per minute** | count/min | Onsets of "two or more agents target the same slot at the same time", checked every frame |
| **Decision switches** per agent per minute and per threat event; oscillations | count | A switch = an agent's target changes from one slot to another or to none. An oscillation = returning to a slot held within the last 3 s |
| Claim failures per minute (B1, B2, SQ) | count/min | `MarkSlotAsClaimed` returning an invalid handle |
| **Time to cover after a threat event**: median, p95; in-cover-by-next-event rate | s, % | Per agent per event: from the event frame to the first frame where the agent is within 75 cm of its target slot **and** that slot is hidden from the current threat position (a visibility table precomputed per threat position at arena build, so measuring costs no traces) |
| Exposure | % of agent-time | Share of agent-frames not in cover |
| Solver time per solve | µs | Our scope |

### 6.4 Pass/fail criteria [target]

| # | Criterion | Threshold |
|---|---|---|
| SC1 | **Lower average AI cost** | At N = 32 and 128, SQ's **mean** AI ms per frame is lower than the **best** of B0–B2, and outside the run-to-run spread (§7). If not met, we report it and analyse why (H2). No reframing |
| SC2 | **No large spikes** | At N = 128, SQ's p99 AI ms per frame ≤ **2.0 ms** (12% of a 60 fps frame) |
| SC3 | **Less repeated work** | At N = 128, SQ's item tests per second ≤ **50% of B0's**. Against B1/B2 the ratio is reported without a threshold, because H2 predicts it may go either way |
| SC4 | **No conflicts** | SQ: **0** assignment conflicts in every run at every N |
| SC5 | **Stable decisions** | SQ: median switches per agent per threat event ≤ **1.0**, and **0** oscillations in every run |
| SC6 | **At least as responsive** | At N = 32 and 128, SQ's p95 time-to-cover ≤ the **best** baseline's p95, and SQ's in-cover-by-next-event rate ≥ **95%** |
| SC7 | **The solver is correct and cheap** | Unit tests: the solver's total cost equals brute force on 1,000 random instances (n ≤ 6 agents, m ≤ 8 slots), forbidden pairs are never chosen, n > m is handled. In the packaged build, p99 solve time ≤ **50 µs** for 8 agents × 128 candidates |

## 7. Measurement plan (summary; the full protocol goes in `09_Testing_And_Benchmarks.md`)

- **Build:** packaged **Development**, standalone, never PIE. The launcher engine can't build Test, and Shipping drops the CSV profiler. Development's EQS debugger cost (`01b` #6) is present in every configuration; Phase 8 adds one batch with it switched off (`FEnvQueryInstance::bDebuggingInfoEnabled = false`) and reports both.
- **Timing:** fixed step `-deterministic -FPS=60`, real GPU, windowed 1280×720, no VSync, no sound, `-traceautostart=0`. Capture opened and closed by the game (`-csvStartOnEvent=BenchStart`, `BenchEnd`, `-ExitAfterCsvProfiling`). EQS's slice is wall-clock, so decisions can differ between runs (`01b` B-1); decision metrics are therefore reported as distributions over runs.
- **CPU pinning:** every timed run is pinned to the **performance cores** (mask from `performance_core_mask()`, `0xFFFF` on this i7-14650HX). The run script **pins itself** once with `pin_self(mask)` (`SetProcessAffinityMask` on its own process) before it records the environment or starts anything, so every game it launches inherits the mask from its first instruction, engine start-up included. It launches the real game executable `Build\Windows\SquadQuery\Binaries\Win64\SquadQuery.exe` directly with `subprocess.Popen`, waits with a timeout and kills the process tree in a `finally` block. `performance_core_mask()`, `pin_self()` and `applied_mask()` are copied from UE5-UpdateBudget's `Tools\run_bench.py`. `cmd /c start /affinity` is **not** used: it always returns exit code 0, so a crashed run would look like a good one. The mask read back with `applied_mask()` goes into the environment record.
- **Runs:** one discarded priming run per batch, then **4 runs per combination** (one per configuration, so each configuration sits in each order position once), interleaved with the order rotated each round. Headline matrix: 4 configurations × 3 values of N × 4 runs = 48 runs, about 80 minutes.
- **Statistics:** per run, mean, median, p95 and p99 (linear-interpolation percentiles); across runs, the median with the [min–max]. An improvement is claimed only when the ranges don't overlap.
- **Environment recorded with every batch:** CPU i7-14650HX (8P + 8E), GPU RTX 5070 Laptop, 32 GB, Windows 11 build 26200, UE 5.8.4, build configuration, resolution, power plan, AC power, affinity mask.
- **Raw data:** untouched captures in `Benchmarks\Raw\`, summaries in `Benchmarks\Summaries\`, naming in `Benchmarks\README.md`.

## 8. Limits we accept up front

- **One threat, static geometry.** Multiple threats and destructible cover are growth paths (`02` §11).
- **No perception in the measured loop.** The threat is a scripted squad event, so time-to-cover measures decision plus movement. Perception latency is identical in every configuration but grows with N, and would hide what we change (`01b` #14).
- **The personal score is straight-line distance**, not path length. It ignores walls between agent and slot. In a low-cover arena that is a small error; it is the same error in every configuration.
- **Cover points are placed by the arena, not generated.** Cover generation is solved elsewhere (CoverPoints, CoverGenerator) and is out of scope.
- **Development-build numbers include EQS debugger overhead.** Measured and reported separately (§7).
- **Squads of 8, up to 16 squads.** The solver is O(n²·m) per squad; much larger groups would need another algorithm. Measured, not assumed.
- **Small N may show no gain.** At N = 8 (one squad) that is a valid result.
