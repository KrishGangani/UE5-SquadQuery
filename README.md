# P07 · SquadQuery: shared tactical decisions for squads

**Domain:** AI decision-making at scale · **Difficulty:** ★★★½☆ · **Engine:** UE 5.8.4

> **Status: planned, not started.** The projects in this portfolio are built one at a time, and this one starts after the projects before it are complete. Everything below is the plan. The success criteria are **targets fixed before any code exists, not results**. Real numbers will only come from controlled before/after runs, each traceable to a raw benchmark batch.

## The problem

Every agent runs its own EQS query, so cost is agents × items × tests. Agents pick the same cover and switch decisions back and forth. In 5.8 EQS caches query templates, not results

## Proposed solution

Evaluate each squad's environment once and share it. Each agent applies only a cheap personal score. Conflict-free assignment of positions. Event-driven re-evaluation (StateTree events), not per-frame polling

## Main Unreal Engine technologies

EQS, StateTree, Smart Objects (claims), AI Perception, navigation, async traces, Gameplay Debugger, Visual Logger

## Success criteria (targets, defined before implementation)

AI ms/frame at 8/32/128 agents; item tests/s; assignment conflicts per minute (target 0); decision switches per minute; time-to-cover after a threat, vs a per-agent EQS baseline

## Existing work it must be compared against

EQS (per-querier), Smart Object claims, sight perception time-slicing, Fab cover systems (surveyed in Phase 2)

**The gap it targets:** Squad-level shared evaluation plus conflict-free assignment, measured on cost **and** decision quality (gap strength: Medium (re-checked in Phase 2)).

**Main risk:** Fab cover plugins may overlap. Positioned on shared evaluation, not cover generation

## Why studios care

AI appears in 20/57 postings; Rocksteady "coordinating combat-related and navigation related NPC behaviours"; WB Montréal "State Trees, EQS, navigation"; Epic NPC "scalability"

## Method

Every project in the portfolio follows the same ten phases: problem analysis → research → architecture → foundation → implementation → explanation → testing → profiling → review → portfolio. Each phase is documented in `Explanation/` (files 00–12, including a line-by-line code walkthrough), and every claimed improvement must be measured against the existing solutions above on the same workload. Engine: Unreal Engine 5.8.4, C++.

## Folder layout

```text
P07_SquadQuery\
├── README.md        ← this page (becomes the project's portfolio page)
├── Explanation\     ← 00–12, created when the project starts
├── Benchmarks\      ← Raw\ + Summaries\
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← analysis and run scripts
└── SquadQuery\    ← the Unreal Engine project (created in its Phase 4)
```
