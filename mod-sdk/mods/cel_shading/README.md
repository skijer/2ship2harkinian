# Wind Waker Style: Cel Shading for 2Ship

Enable the package in the mod manager, restart, then open **Enhancements → Cel Shading**.
The effect starts enabled with the reference look: boundary 0.5, softness 0.02,
highlight 0.6 and shadow 0.6. It applies to actors, including custom actors, while
scene geometry and doors keep native lighting. Unlit effects keep their own colors.

The mod selects the nearest point light within its radius, falling back to the
brighter environment directional light. Direction and color ease between sources.
Settings control the band boundary, softness, light/shadow intensity, point-light
range, transition time, item shading and a monochrome band diagnostic.

The package requires the accompanying 2Ship renderer update and regenerated
`2ship.o2r`. The mod owns all selection rules and settings; the engine only exposes
an actor draw-begin hook and renderer ramp parameters. Disable the package and
restart to remove the effect, or untick Enable Cel Shading for immediate comparison.

This ports the cel-lighting feature from roborich's
`wind-waker-style-cel-shading` branch (Fast3D transport commit `a001ff42`).
Light-casting pools, silhouette shadows and the Wind Waker sky are separate
features of that fork and are not part of this package.

Validation includes native Direct3D compilation of 64 shader combinations (128
vertex/pixel stages), with and without toon lighting, primitive depth, fog,
grayscale, alpha and two-cycle combiners. Visual gameplay still needs checking.
