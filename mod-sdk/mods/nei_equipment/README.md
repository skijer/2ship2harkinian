# NEI Equipment layout

This mod declares the NEI equipment page over Masks. It uses the source OoT equipment grid (28-pixel icons, 32-pixel rows), its background, and the row/column declared by each NEI equipment mod. It adds no ownership or item behavior.

The host reads the layout data through RegisterLayoutPage. Defaults last for the loaded mod session; saved user placements and cell edits take priority. L cycles the Masks subpages. On a single-piece cell, A or a C button equips/toggles the selected piece; C-Up opens its description. Shared cells retain the item wheel, with C buttons equipping its selected piece.

The title texture is packaged locally; the remaining background sections use OoT Asset Loader resources. The layout falls back to the native Masks background if any background section is absent.

The page supplies the original OoT IA4 corner textures. Cursor position and perspective follow the rendered cell rather than MM's vanilla Masks grid.
