# BowlingPlus collection: adding a pattern from a Kegel data sheet

Patterns live in `src/OilCollection.mm`. Mark them `"exact": @YES`, so BowlingPlus's engine uses each step's End distance from the sheet. Each step is `{ start board, stop board, loads, speed, travel-to ft }`, copied from the sheet's **Forward** and **Reverse** tables:

- **Boards** are 1–39 from the left: nL = n, nR = 40 − n (2R = 38, 13R = 27).
- **Travel-to** is the sheet's **End** column, and only matters on zero-load rows.
- **Also record** the sheet's distance, volume, oil per board, and **Reverse Brush Drop** (`drop`). The engine only lays reverse oil short of the brush drop.

Before adding a pattern, check it against the sheet:

1. **Boards crossed:** sum over steps of `loads × (stop − start + 1)` must match the sheet's forward and reverse "Boards Crossed".
2. **Volume:** total boards crossed × oil per board (µL) must match the sheet's "Volume Oil Total".
3. **End distances:** the engine's math must match each step's "End" value.
   - Forward, first step: `(loads − 1) × speed × 17/120`.
   - Later steps: previous end ± `loads × speed × 17/120` (+ forward, − reverse).
   - Zero-load steps: travel-to.
   - Kegel cuts each step's distance to 0.1 ft, so reverse ends can differ by a few tenths.

What's needed from each sheet: both step tables (Forward and Reverse tabs), plus the header (distance, oil per board, volume, boards crossed). A screenshot like the Baton Rouge one is perfect.
