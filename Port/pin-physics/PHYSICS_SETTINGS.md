# Exact physics settings used in the pin study

These are the game's own values, read from its files (version 1.907), as recorded in `PIN_PHYSICS_STUDY.md`. The PhysX model in `pinlab/` uses them. BowlingPlus changes these at runtime: the pins' friction (1.7.0), the ball's collision mode (continuous, 1.7.0) and, with the double rate (1.7.1), the physics step.

## What the game actually does (read from its files)

The pins on the lane are `PinsPhys/PinUnity1..10` (`InventaryData.kegels`), not the `PinHolder` pins the old "Pin physics fix" changed (those never move; the lane doesn't use them). Each pin:

| | Value |
|---|---|
| Mass | 1.644 kg (3.62 lb; regulation 3.375-3.66 lb) |
| Centre of mass | 0.1498 m up the axis (`RunPsycsTest.pinCenterMass`) |
| Inertia | 0.013934 (tipping), 0.001915 (spin) kg m² (`pinInertiaTensor`) |
| Colliders | five convex hulls + a 6.1 cm capsule at the belly + a 3.2 cm sphere at the head; 0.379 m tall, belly radius 0.060 m (regulation 0.381 m, 0.0605 m) |
| Materials | "Pin": friction 0.5 sliding / 0.3 starting, bounce 0.65; "PinButtom" (base): bounce 0.4; combine mode Minimum |
| Collision | Discrete |
| At every rack | `InventaryData.ResetRigidBody` destroys the pin's Rigidbody and adds a new one, with max depenetration and max angular velocity 100000 (so no 7 rad/s spin cap) |

The rack is a regulation 12-inch triangle (head pin at y 18.263 m). The ball: radius 0.108 m, "Ball" material (0.3 / 0.3, bounce 0.65). Deck: friction 0.6 / 0.9, bounce 0.7. Kickbacks: friction 0.01, bounce 0.9. Physics: 7.5 ms step, 7/7 solver iterations, contact offset 0.5 mm, PGS solver, patch friction.

**What BowlingPlus can change while the game runs** (both platforms; everything else was stripped from the engine binary too, not just from the scripts): per body mass, centre of mass, inertia, damping, velocities, collision mode, max angular and depenetration velocity; any physics material's friction (not bounciness). Not reachable: the physics step, solver iterations, contact offset, bounciness, combine modes. (`Time.get_fixedDeltaTime` survives natively, so the step could be found and changed by writing memory; that would also change everything the game does per physics step, like the oil, and is left for later.)

