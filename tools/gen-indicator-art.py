"""The two indicators' art, drawn from scratch (2026-09-30; the owner: "some better art for the damage indicator bars.
Something that looks radial. See if you can make it from scratch and import it" and, for the sneak marks, "something
that's slightly curved so it makes sense for the radial appearance").

Each texture is one segment of a ring, WHITE (the mod tints it: red for damage; white / yellow / red for detection),
with the ring's centre below the texture, so the arc bulges away from the screen's centre. The ring radius in the
texture is R_TEX pixels; the mod sizes a mark so that R_TEX matches the indicator's Radius setting, which keeps the
curvature of every mark true to the ring it sits on (width = radius * W / R_TEX, height = radius * H / R_TEX).

The mod loads them at run time through the engine's own KismetRenderingLibrary::ImportFileAsTexture2D.
Output: dist/OblivionRemastered/Binaries/Win64/OBSE/Plugins/HUDPositionManager/Art/*.png
"""
import os

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "dist", "OblivionRemastered", "Binaries", "Win64", "OBSE", "Plugins", "HUDPositionManager", "Art")
R_TEX = 420.0   # must match kArtRingRadius in Hud.cpp
SS = 4          # supersampling


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def arc(w, h, half_span_deg, thick_mid, thick_end, glow, glow_alpha, core_shade):
    """A white ring segment: its middle at the texture's centre, the ring's centre R_TEX below it."""
    W, H = w * SS, h * SS
    y, x = np.mgrid[0:H, 0:W].astype(np.float64)
    x = (x + 0.5) / SS
    y = (y + 0.5) / SS
    cx, cy = w / 2.0, h / 2.0 + R_TEX
    dx, dy = x - cx, cy - y
    r = np.hypot(dx, dy)
    theta = np.degrees(np.arctan2(dx, dy))            # 0 straight up, + to the right
    t = np.clip(np.abs(theta) / half_span_deg, 0.0, 1.5)
    profile = np.clip(1.0 - t * t, 0.0, 1.0) ** 0.7     # full in the middle, tapering to the ends
    thick = thick_end + (thick_mid - thick_end) * profile
    d = np.abs(r - R_TEX)
    core = smoothstep(thick / 2 + 0.9, thick / 2 - 0.9, d)
    halo = np.exp(-((d / (thick / 2 + glow)) ** 2)) * glow_alpha
    ends = smoothstep(1.0, 0.72, t)                     # soft ends, no hard cut
    alpha = np.clip(np.maximum(core, halo) * ends, 0.0, 1.0)
    # the stroke is brightest along its centre line: a slight bevel so the tint reads as a lit edge, not a flat band
    shade = np.where(core > 0.01, core_shade + (1.0 - core_shade) * (1.0 - np.clip(d / (thick / 2 + 1e-6), 0, 1)) , 1.0)
    rgb = np.clip(shade, 0.0, 1.0)
    img = np.dstack([rgb, rgb, rgb, alpha]) * 255.0
    im = Image.fromarray(img.astype(np.uint8), "RGBA").resize((w, h), Image.LANCZOS)
    return im


def main():
    os.makedirs(OUT, exist_ok=True)
    # damage: a broad blood arc, ~56 degrees of the ring, thick in the middle with a wide soft halo
    dmg = arc(480, 150, 28.0, thick_mid=30.0, thick_end=5.0, glow=16.0, glow_alpha=0.55, core_shade=0.72)
    dmg.save(os.path.join(OUT, "DamageArc.png"))
    # sneak: a slim arc, ~26 degrees, tinted white / yellow / red by how well that actor sees the player
    snk = arc(220, 70, 13.0, thick_mid=15.0, thick_end=4.0, glow=7.0, glow_alpha=0.5, core_shade=0.85)
    snk.save(os.path.join(OUT, "SneakArc.png"))
    for n, im in (("DamageArc", dmg), ("SneakArc", snk)):
        print(n, im.size, "->", os.path.join(OUT, n + ".png"))


if __name__ == "__main__":
    main()
