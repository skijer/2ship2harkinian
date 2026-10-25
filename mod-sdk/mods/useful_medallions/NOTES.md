# useful_medallions: 2Ship port

Built and linked in Release on 2026-10-12. Gameplay remains unverified.

The six medallions register standalone custom items and spell actors. Spell animations are resolved from packaged/game resources. Fire, ice and light arrows use native MM arrow actions; dark, soul and wind effects attach custom SW97 actors to normal arrows. Trails use the native arrow drawing and destruction lifecycle.

Select the bow in Kaleido and press A to cycle owned elemental medallions. The selected element appears in button icons and below the bow cell. The position follows the configured Kaleido placement.

Spirit flight uses a custom MM EnElf actor controlled by the player's stick, A/R for height and B to leave. It restores the player action and appearance when flight ends. It does not require the SoH Ivan system.

MM has no slingshot or OoT mud wall: elemental seeds and mud-wall melting are not implemented. The stored slingshot element field is retained for data compatibility. Costs: Forest 12, Spirit 24, Shadow 24, Water 12, Light 24, Fire 12.
