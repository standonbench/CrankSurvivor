# Rework Weapons

## Brine Splash (AoE Burst -> Constant Aura)
Change from a cooldown-based expanding ring into a permanent area-of-effect aura around the player.
- Enemies inside naturally take damage every X frames.
- Visually represented by a constant dithered boundary ring or static field that follows the player.

## Chain Lightning (Visual Polish)
- Currently renders as simple straight lines.
- Will overhaul the rendering to draw jagged, branching bolts by breaking the line into segments and applying perpendicular random offsets.
- Will add a brief 1-frame screen flash (or high-density XOR flash) on initial strike.

## Riptide (Vortex -> Intuitive Whirlpool)
- Ellipse rendering is confusing. Will replace it with a dense, rotating Archimedean spiral or a sprite-based whirlpool that clearly signals "suck inward."
- Add pronounced internal water particles constantly racing to the center.
- Tighten the pull physics to feel more satisfying (stronger snap towards the exact center), making it obvious what the weapon does.

## Depth Charge (Visual Polish)
- The mine drop will sink and flash with alternating colors/patterns before detonating.
- The explosion will be vastly upgraded: filled expanding circles, screen shake, and bubble particles, leaving a clearly defined slow field.
