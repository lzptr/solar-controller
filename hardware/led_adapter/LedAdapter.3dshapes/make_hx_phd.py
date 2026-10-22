"""Simplified 3D model of the hanxia HX-PHD2.0-2x3ALT SMD wafer (LCSC C53055815).

Dimensions from the hanxia customer drawing (datasheets/hanxia_HX-PHD2.0-2x3ALT.pdf,
2x3P column: DIM.A 4.0, DIM.B 8.0). Origin and axes match the KiCad footprint
Hanxia_HX-PHD2.0-2x3ALT: origin at the centre of the pin field, Z up from the
board surface. KiCad 3D models are Y-up while footprints are Y-down, so pin 1
(footprint +2, +2.625) sits at model (+2, -1) with its SMD foot towards -Y.

Run with CadQuery installed:  python make_hx_phd.py
"""

from pathlib import Path

import cadquery as cq

PITCH = 2.0
COLS = 3
LENGTH = 8.0            # DIM.B, housing along the pin rows (X)
DEPTH = 5.2             # housing across the rows (Y)
HEIGHT = 7.1            # board surface to housing top
BASE = 0.5              # housing underside sits on the SMD feet
CAVITY_L = 4.0 + 2.9    # DIM.A + 2.9
CAVITY_D = 4.3
FLOOR = 2.5             # cavity floor above the board
PIN = 0.5               # square pin and foot width
PIN_TOP = 5.6
FOOT_T = 0.5
FOOT_SPAN = 8.11        # foot tip to foot tip
KEY_W = 3.0             # polarising notch in the even-row wall
KEY_DEPTH = 2.5
SLOT_W = 1.1            # latch slots in both end walls
SLOT_DEPTH = 3.1
SLOT_Y = -0.5           # slots sit 0.5 mm towards the odd row

housing = (
    cq.Workplane("XY")
    .box(LENGTH, DEPTH, HEIGHT - BASE, centered=(True, True, False))
    .translate((0, 0, BASE))
    .edges("|Z").chamfer(0.2)
)
housing = housing.cut(
    cq.Workplane("XY")
    .box(CAVITY_L, CAVITY_D, HEIGHT, centered=(True, True, False))
    .translate((0, 0, FLOOR))
)
wall = (DEPTH - CAVITY_D) / 2
housing = housing.cut(
    cq.Workplane("XY")
    .box(KEY_W, wall + 0.2, KEY_DEPTH, centered=(True, False, False))
    .translate((0, DEPTH / 2 - wall - 0.1, HEIGHT - KEY_DEPTH))
)
for sx in (-1, 1):
    housing = housing.cut(
        cq.Workplane("XY")
        .box((LENGTH - CAVITY_L) / 2 + 0.2, SLOT_W, SLOT_DEPTH, centered=(True, True, False))
        .translate((sx * (LENGTH + CAVITY_L) / 4, SLOT_Y, HEIGHT - SLOT_DEPTH))
    )
# pin 1 mark on the top face
housing = housing.cut(
    cq.Workplane("XY")
    .polyline([(0, 0), (-0.8, 0), (0, 0.8)]).close()
    .extrude(0.15)
    .translate((LENGTH / 2 - 0.15, -DEPTH / 2 + 0.15, HEIGHT - 0.15))
)
# standoffs under the end walls
for sx in (-1, 1):
    housing = housing.union(
        cq.Workplane("XY")
        .box(0.6, DEPTH - 0.4, BASE, centered=(True, True, False))
        .translate((sx * (LENGTH / 2 - 0.3), 0, 0))
    )

pins = cq.Workplane("XY")
for i in range(COLS):
    x = PITCH * (COLS - 1) / 2 - i * PITCH
    for sy in (1, -1):
        y = sy * PITCH / 2
        pins = pins.union(
            cq.Workplane("XY").box(PIN, PIN, PIN_TOP, centered=(True, True, False)).translate((x, y, 0))
        )
        y_tip = sy * FOOT_SPAN / 2
        pins = pins.union(
            cq.Workplane("XY")
            .box(PIN, abs(y_tip - y), FOOT_T, centered=(True, False, False))
            .translate((x, min(y, y_tip), 0))
        )

assy = cq.Assembly(name="HX-PHD2.0-2x3ALT")
assy.add(housing, name="housing", color=cq.Color(0.93, 0.90, 0.80))
assy.add(pins, name="pins", color=cq.Color(0.82, 0.82, 0.84))

out = Path(__file__).with_name("Hanxia_HX-PHD2.0-2x3ALT.step")
assy.export(str(out))
print(out)
