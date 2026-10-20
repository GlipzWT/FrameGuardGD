# FrameGuard — experimental alpha

Practice-only timing assist for single, forward-moving Wave. Correction window: 0–10 observed physics updates. F7 opens the panel; F8 toggles assist. Both keys are configurable in Geode Settings.

**This is NOT a finished or game-tested mod.** The planner uses an approximate, swept AABB model, not GD's complete collision/physics engine. It can miss or misclassify a safe path.

Local rollback is off by default. When enabled, it restores only the player checkpoint and reconstructs a linear Wave path; the level timeline, camera, music and trigger state are not rewound. A short collision-death grace period is used to accept late input. This is temporary collision suppression, not guaranteed survival. Whole-level static-object auditing is deliberately restrictive.

Triggers, slopes, dynamic objects, dual, platformer, reverse and sideways movement are unsupported. Use only in disposable training levels and Practice Mode. See README_RU.md and docs/TEST_PLAN_RU.md in the source project.
