# Item and collectable parity contract

User decisions, 2026-10-23:

- Equivalent items share one acquisition/ownership identity by default in a future Combo run. An explicit option separates game copies. Native numeric IDs are never the cross-game identity.
- Swords and shields must support transfer or decoupling according to that option. Equivalence mappings still require item-by-item verification; similar names alone do not establish equivalence.
- Native MM item improvements are independent mods. Preserve native acquisition, inventory entries and randomizer rewards; do not add a second copy merely to enable improved behavior.
- Useful remains must be split per remain in OoT as well as MM. Four independently selectable improvements: Odolwa, Goht, Gyorg and Twinmold.
- Mask improvements include Circus Leader taxes, Great Fairy teleport and Couple's Mask healing. Adapt the existing authoritative OoT effects to MM's native actors, scenes and state.
- New per-form item dependencies are deferred. Preserve existing restrictions until the user specifies future form behavior.
- Future Combo engine runtime remains outside the current standalone parity implementation.

Completion requires verified item/collectable identity, ownership, give/remove, persistence, usable behavior, presentation, randomizer pool, progressive state, logic effects and networking for each entry. An assigned API pointer does not satisfy this contract by itself.

## Reward routing decisions

- Great Fairy Mask travel destinations unlock only after the corresponding fountain reward has actually been received. In randomizer, use check completion at that fountain; owning its vanilla reward item from another location is insufficient.
- Circus Leader taxes grant the corresponding existing minigame reward, resolving its randomized placement when randomizer is active. Do not create a new tax check or a duplicate acquisition.
- Each adapter must preserve the original minigame's completion/repeat-prize state and grant its reward only once according to that check. Marking a reward eligible is not equivalent to receiving it.
