> **Raw Phase 2 research notes (D_existing_work), UE 5.8.4, 2026-11-21.** Working notes kept as an evidence trail. Every claim cites a URL or a `File:line`. **The claims the design depends on were re-checked and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md). Treat anything here that is not restated there as unverified.**

# P07 SquadQuery: Phase 2 research, part D: existing work on squad cover selection

Research date: 2026-11-21. No plugin was installed or bought; only public pages and papers were read.

**Evidence labels**
- **opened**: I fetched the page or paper and read the text quoted.
- **snippet only**: from a search-result summary; I did not read the page.
- **source read**: I read the code (local engine install, or a public repository).

---

## D1. What the engine ships (summary of parts A and B)

| Feature | What it does for squads | What it doesn't | Evidence |
|---|---|---|---|
| EQS | Evaluates candidate positions per querier, time-sliced under a global 10 ms/frame budget | No result sharing between queriers; caches templates, not results. A single-result query short-circuits its expensive filter | part A (source read) |
| Smart Object claims | Exclusive reservation of slots, with priorities and preemption | No arbitration of *who* should get a slot; no shared search | part B, B1 (source read; 5.8 docs opened) |
| EQS Smart Objects generator | Generates only currently claimable slots | Claimability is checked at generation; the claim happens later (race window) | part B, B2 (source read) |
| StateTree events | Event queue (64 max) that wakes a sleeping tree | Doesn't evaluate anything | part B, B4 (source read) |
| AI Perception (sight) | Time-sliced sensing, 6 traces per tick by default | Not evaluation | part B, B5 (source read) |
| `UAISense_Team` | Broadcasts stimuli to teammates | Shares *sensing*, not *evaluation* | `AIModule/Classes/Perception/AISense_Team.h:14, 50` (source read) |
| Mass EQS (5.8) | Runs EQS generators/tests for Mass entities in processors | Per query instance; Mass entities only | part A, A8 (source read) |

## D2. Fab and GitHub cover plugins

### D2.1 CoverPoints (SimulatedFlow), UE 5.8

- Repository: https://github.com/SimulatedFlow/ue-plugin-CoverPoints (**opened**, README). Seller page linked from it: https://www.fab.com/sellers/Silvan%20Teufel (not opened).
- Description: "Cover point generation, constant-time scoring and exclusive reservation, for Unreal Engine 5.8." "No Behavior Tree. No animation."
- **Generation:** a Cover Volume is sampled on a grid; sweeps find blocked geometry and record the shield normal, high/low and peek directions. Generation is time-sliced with "a sample and millisecond budget".
- **Scoring:** "constant-time" with six weights and "a spatial hash instead of line traces per candidate". A small fixed number of verification traces per frame spot-checks points.
- **Reservation:** `Claim`/`Release` per point with expiry: "Eight agents sent at once end up at eight different points; two never share one."
- **Group evaluation or joint assignment:** none described. "Find Best Cover" is called per agent.
- **EQS:** not used. **License:** "source-available, not open source … buy a license to use it."
- **Numbers:** none published. The README claims cost "would be the same whether the level holds two hundred points or twenty thousand", with no conditions.
- **Overlap with this project:** high on cover generation and per-point reservation, and it attacks per-candidate trace cost by **not tracing** (precomputed cover data). Low on shared squad evaluation and joint assignment. We don't generate cover points (the arena places them), and we keep the engine's own trace test.

### D2.2 Tactical Combat AI Framework (Fab)

- https://www.fab.com/listings/5440b7d2-a9d9-476c-ba51-6ef2541840d2 (**opened**).
- A Behavior Tree framework with a "Dynamic Cover System (EQS)": "enemies intelligently find nearby cover, move to safety, peek out to shoot". Includes an "EQS Cover Query" asset.
- Per enemy. No reservation or squad coordination is mentioned. "Designed specifically for Unreal Engine 5"; no minor version; no performance numbers.
- **Overlap:** this is the per-agent EQS pattern our baseline B0 imitates.

### D2.3 CoverGenerator-UE5 (teella)

- https://github.com/teella/CoverGenerator-UE5/blob/master/README.md (**snippet only**). Analyses level geometry for cover points and exposes them to EQS as an item source; described as a proof of concept, derived from the older forum project "Cover Generator" (https://forums.unrealengine.com/t/cover-generator/87442, **snippet only**).
- **Overlap:** cover generation, which is out of scope here.

### D2.4 Squad & Team Combat AI (Fallen Hell Labs)

- https://www.unrealengine.com/marketplace/en-US/product/squad-team-combat-ai (**snippet only**). Squads and teams "without using behaviour trees"; AI "finds cover automatically in the world geometry and receives orders from the squad leader". Engine versions 4.26–5.4 in the snippet.
- **Overlap:** squad orders. How cover is chosen and shared is not stated in what I saw.

### D2.5 SquadAI (Dagoat000, GitHub)

- https://github.com/Dagoat000/SquadAI (**opened**, README). "A modular tactical AI framework for Unreal Engine 5, written primarily in C++." Lists "Cover allocation" among squad coordination features and says: "The system is designed to prevent every AI agent from simply choosing the nearest available cover position."
- How allocation works (per agent or per squad, greedy or optimal, polled or event-driven) is **not described**. No EQS or Smart Objects mentioned. No measurements ("Performance profiling is performed during development"). License terms unclear (README says "educational and portfolio purposes"; the sidebar shows MIT).
- **Overlap:** the same goal (no nearest-cover pile-ups), method unknown, no numbers.

## D3. The same idea in a 2014 Unreal forum thread

- "Tactical AI ( Cover , Threat and Suppression )", Epic Developer Community Forums, author "Frozenfire", 2014-12-18: https://forums.unrealengine.com/t/tactical-ai-cover-threat-and-suppression/17436 (**opened**).
- "A squad helper object is spawned when none is there. This object scans for active node, scores each node and shares this information" with the squad. "The AI can now accept weight parameters when selecting Cover", and "the full score is calculated by each AI based on weightage."
- No measurements.
- **This is our "evaluate once per squad, cheap per-agent score" idea, described 12 years ago in UE4.** We must not present shared evaluation as new.

## D4. Shipped games and published talks

### D4.1 F.E.A.R. (Monolith, GDC 2006)

- Jeff Orkin, "Three States and a Plan: The A.I. of F.E.A.R.", GDC 2006: https://www.gamedevs.org/uploads/three-states-plan-ai-of-fear.pdf (**opened**, text extracted; the quotes are on pp. 13–14).
- "We have a global coordinator in F.E.A.R. that periodically re-clusters A.I. into squads based on proximity."
- Get-to-Cover: "the squad behavior does not need to analyze the map and determine where there is available cover. This is because each A.I. already has sensors keeping an up to date list of potentially valid cover positions nearby. All the squad behavior needs to do is select one node that the A.I. knows about, and ensure that other A.I. are not ordered to go to the same node."
- **Overlap:** squad-level **conflict-free** assignment, shipped in 2005. Evaluation stays per agent (each AI's sensors), and the squad just avoids duplicates.

### D4.2 Killzone 3 multiplayer bots (Guerrilla)

- Remco Straatman, Tim Verweij, Alex Champandard, "Hierarchical AI for Multiplayer Bots in Killzone 3", *Game AI Pro* (2013), ch. 29: https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter29_Hierarchical_AI_for_Multiplayer_Bots_in_Killzone_3.pdf (**opened**, text extracted).
- "The squad does not micromanage each bot; bots gather information and decide how to fulfill their role. For example, bots can select their own attack target, the position to attack from …" (book p. 379).
- Position picking scores nearby waypoints on "cover from known threats, distance from threats or friendlies, line of fire to threats, travel distance to reach the position" (book p. 383).
- The squad's strategic path is "a corridor for the squad members … Furthermore the corridor restricts the search space of individual path planning, thus improving performance." (book p. 386)
- **Overlap:** a hierarchy where the squad constrains individual position picking, for quality and performance. Individual evaluation remains per bot.

### D4.3 Killzone (Guerrilla, GDC 2005)

- Remco Straatman, Arjen Beij, William van der Sterren, "Killzone's AI: dynamic procedural combat tactics": http://cse.unl.edu/~choueiry/Documents/straatman_remco_killzone_ai.pdf and slides https://www.guerrilla-games.com/media/News/Files/gdce05_killzone_ai.pdf (**snippet only**). Position evaluation functions as "one of the cornerstones"; a weighted sum of factors per candidate.

### D4.4 Tactical position selection (Crytek) and EQS's lineage

- Matthew Jack, "Tactical Position Selection: An Architecture and Query Language", *Game AI Pro* (2013), ch. 26 (**snippet only**, via the publisher page https://www.routledge.com/Game-AI-Pro-360-Guide-to-Tactics-and-Strategy/Rabin/p/book/9780367150884 and Eric Johnson's follow-up chapter https://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter26_Guide_to_Effective_Auto-Generated_Spatial_Queries.pdf, which names CryENGINE's Tactical Point System and UE4's EQS as mature systems of this kind).
- Generation → filtering → weights, per query. The same structure as EQS.

### D4.5 Epic's EQS documentation

- "Environment Query System Overview in Unreal Engine | Unreal Engine 5.8 Documentation": https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-overview-in-unreal-engine?application_version=5.8 (**opened**). Generators produce Items; Tests pick the best; filters run before scoring so "fewer Items are returned and need to be scored". Nothing on time-slicing or sharing results between AI.
- An archived UE4-era forum thread says EQS results can't be shared between agents ([research/02](../Research/02_technical_challenges.md) [T9-E], **opened** in Step 0). Part A confirms that for 5.8.4 from the source.

## D5. Assignment algorithms (textbook background)

- The assignment problem (minimum total cost, one task per agent) is solved exactly by the Hungarian method: H. W. Kuhn, "The Hungarian method for the assignment problem", *Naval Research Logistics Quarterly* 2 (1955); J. Munkres, "Algorithms for the Assignment and Transportation Problems", *J. SIAM* 5 (1957). These are classic results; I did not open the original papers (**not opened**). The shortest-augmenting-path form runs in O(n²·m) for n agents and m candidates, which is tiny at n ≤ 16.
- Greedy "first come, first served" claiming (what per-agent queries plus Smart Object claims produce) has no optimality guarantee for the total or the worst cost **[inferred]**.

## D6. What remains (my assessment)

| Element | Exists already? | Where |
|---|---|---|
| Evaluate once per squad, share, per-agent weights | **Yes**, as an idea and an implementation (2014) | D3 |
| Squad-level conflict-free assignment | **Yes**, shipped (2005) | D4.1 |
| Per-point exclusive reservation | **Yes**, in the engine and in plugins | D1, D2.1 |
| Cheap cover scoring without traces | **Yes** (precomputed cover data) | D2.1 |
| Built **on** UE 5.8's own EQS and Smart Object claims, as a reusable plugin | Not found in what I reviewed | — |
| **Optimal** joint assignment (minimum total cost), compared with greedy claiming | Not found in what I reviewed | — |
| **Controlled measurements** of cost *and* decision quality against per-agent EQS baselines (polled, claimed, event-driven) at 8 / 32 / 128 agents | **Not found** in anything reviewed. No reviewed source publishes numbers | — |

**Honest gap: weak for the idea, medium for the measured study.** The roadmap rated the gap "Medium (re-checked in Phase 2)". After Phase 2 the idea itself is not new (D3, D4.1). What is missing is the evidence: an engine-native implementation and a controlled before/after study that also counts decision quality. The project is repositioned in `01` and `01b` as **a measured study plus a reusable plugin**, not a new technique. A web search can't prove absence, so this is a statement about what was reviewed.
