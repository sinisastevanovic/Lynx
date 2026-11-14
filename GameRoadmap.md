# 🎮 NIGHTBOUND — Prototype Roadmap
### Goal: Playable game loop to validate core fun

---

## Phase 0: Foundation
> *"Can I move and does it feel good?"*

- [ ] Player character (capsule/placeholder is fine)
- [ ] 3D movement: run, sprint, dash
- [ ] Camera system (third-person, handles indoor/outdoor)
- [ ] One test arena (flat, bounded)
- [ ] Basic collision

**✓ Milestone:** You can run around a space and it feels responsive.

---

## Phase 1: Combat Core
> *"Can I kill things and does THAT feel good?"*

- [ ] One basic auto-attack weapon (e.g., spinning claws)
- [ ] Enemy placeholder (walks toward player, deals contact damage)
- [ ] Enemy spawner (timed waves, increasing count)
- [ ] Health system (player + enemies)
- [ ] Death state (player dies → game over)
- [ ] Damage feedback (flash, knockback, sound)
- [ ] Kill feedback (particles, ragdoll or death anim)

**✓ Milestone:** Enemies swarm, you kill them, it feels satisfying. You can die.

---

## Phase 2: Progression Loop
> *"Do I want to keep playing?"*

- [ ] XP gems drop from enemies
- [ ] Gem magnet / collection radius
- [ ] XP bar → Level up trigger
- [ ] Level-up menu: pick 1 of 3 upgrades
- [ ] 5-8 basic upgrades (damage, speed, health, attack speed, AOE size, etc.)
- [ ] Upgrades actually affect gameplay

**✓ Milestone:** Kill → collect → level up → get stronger → repeat. The loop exists.

---

## Phase 3: Session Structure
> *"Is there a beginning and end?"*

- [ ] Game timer (e.g., 5-10 min prototype run)
- [ ] Escalating difficulty curve (more enemies, faster spawns)
- [ ] Win condition (survive the timer)
- [ ] Lose condition (HP reaches 0)
- [ ] End screen: Win/Lose + basic stats (kills, time, level reached)
- [ ] Restart button

**✓ Milestone:** Complete session from start to finish. Replayable.

---

## Phase 4: Second Weapon + Fusion Test
> *"Is our unique mechanic fun?"*

- [ ] Add second weapon (e.g., bat swarm / projectile)
- [ ] Weapon slots (can hold 2+)
- [ ] Basic fusion: combining weapons A+B → weapon C
- [ ] Fusion feels meaningfully different/stronger

**✓ Milestone:** Fusion works and creates an "oh cool!" moment.

---

## Phase 5: Terrain & Momentum Test
> *"Do our other ideas add fun or just complexity?"*

- [ ] One terrain type (e.g., ice = slide boost)
- [ ] Momentum damage bonus (visible feedback)
- [ ] Test arena with terrain variety

**✓ Milestone:** Terrain changes how you play. Momentum feels rewarding.

---

## Phase 6: Polish Pass (Prototype-Level)
> *"Does it FEEL like a game?"*

- [ ] Main menu (Play / Quit)
- [ ] Pause menu
- [ ] Basic UI: HP bar, XP bar, timer, weapon icons
- [ ] Audio: hits, kills, level-up, ambient
- [ ] Screen shake (toggleable)
- [ ] Kill streak feedback

**✓ Milestone:** Someone unfamiliar can pick it up and play a full session.

---

## Prototype Scope Summary

| Include | Exclude (for now) |
|---------|-------------------|
| 1 character | Multiple characters |
| 2-3 weapons + 1 fusion | Full weapon tree |
| 1 arena | Multiple levels |
| ~8 upgrades | Deep meta-progression |
| 1 enemy type (+ maybe 1 elite) | Full mythology enemy sets |
| 5-10 min session | 20-30 min full runs |

---

## Validation Questions for Playtesting

After prototype is playable, answer these:

1. Is movement fun *without* combat?
2. Is killing enemies satisfying?
3. Does leveling up feel rewarding?
4. Does fusion create excitement?
5. Do players *want* to restart after dying?
6. Does momentum/terrain add to fun or feel forced?

If any answer is "no" — iterate on that phase before adding more content.

---

Want me to break any phase into more granular tasks, or help prioritize what to build first?