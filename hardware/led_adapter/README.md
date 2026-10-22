# LED ring adapter

Small adapter PCB that sits back-to-back on the off-the-shelf 23 mm ring with
seven SK6812 RGBW LEDs used in each planet globe. It gives the ring a keyed,
pluggable connector so the cables between the globes can be made and changed
later instead of being soldered to the ring at the final length.

```
globe interior
  ┆  LEDs face the globe
  ├─ LED ring (D23, 1.6 mm, LEDs 1.6 mm tall)
  ├─ adapter (this board, 1.6 mm)   back-to-back with the ring, pads aligned
  └─ J1  PHD 2.0 mm 2x3 wafer      points away from the globe, into the socket
         cable from the previous globe (odd pins) / cable to the next globe (even pins)
```

Five standard header pins (0.64 mm square, pulled out of a pin header strip)
go through both boards and carry IN, 5V, GND, GND and OUT; 5V and GND are bussed straight through the
adapter, data enters the ring at IN and leaves it at OUT. Two M2.5 screws
through the shared 2.9 mm holes clamp the stack into the printed socket.

## Files

| File | Content |
| --- | --- |
| `led_adapter.kicad_pro/_sch/_pcb` | KiCad 10 project, created from the JLCPCB 1-2 layer template |
| `LedAdapter.kicad_sym` | symbol `LED_Ring_SK6812_7` (the ring as a 5-pin module) |
| `LedAdapter.pretty/Hanxia_HX-PHD2.0-2x3ALT.kicad_mod` | connector footprint from the manufacturer land pattern |
| `LedAdapter.pretty/LED_Ring_SK6812_7_D23_Pins.kicad_mod` | the ring's pad pattern (pins) and mounting holes |
| `datasheets/` | connector drawing |

## LED ring geometry as modelled

Ring: 23.0 mm diameter, 1.6 mm thick, 3.2 mm with LEDs. Mounting holes 2.9 mm,
19.2 mm apart, on the x axis. Pad positions, seen from the **LED side** with
the holes left/right (origin = ring centre, y up):

| Pad | x | y |
| --- | ---: | ---: |
| IN | +2.54 | +4.57 |
| 5V (PWR) | -2.54 | +4.57 |
| GND | -5.33 | 0.00 |
| GND | -2.54 | -4.57 |
| OUT | +2.54 | -4.57 |

The adapter is viewed from the connector side, i.e. the ring is seen from its
**back**, so on the adapter the pattern is mirrored: IN top-left, 5V top-right,
GND right, GND bottom-right, OUT bottom-left (as labelled on the silkscreen).

Positions were measured from the seller's front and back product photos scaled
to the 19.2 mm hole pitch and confirmed with calipers on the real ring: IN to
OUT 9.1 mm, IN to PWR 5.1 mm (model: 9.14 / 5.08). The pattern's position
relative to the mounting holes comes from the photos (about 0.2 mm
uncertainty), which the 0.4 mm clearance of M2.5 screws in the 2.9 mm holes
covers. The ring's pads are 1.6 mm with a hole that takes a standard 0.64 mm
square header pin; the adapter's pin holes are 1.2 mm (2.0 mm pad) so the pins
pass both boards with some slack.

## Connector J1

hanxia **HX-PHD2.0-2x3ALT**, LCSC C53055815, JLCPCB extended part
(435 in stock on 2026-10-20). 2x3 top-entry SMD wafer, 2.0 mm pitch, JST PHD
compatible, keyed, 3 A per contact, housing 8.0 x 5.55 mm, 8.1 mm tall.

Mating side: JST PHDR-06VS housing with SPHD-001T-P0.5 contacts (AWG 22 to 28)
or the equivalent "PHD2.0 2x3" clone housings and crimp terminals.

Same land pattern, usable as drop-in alternatives if the stock runs out:
SHOU HAN PHD2.0-2x3PLT (C48783195), XUNPU WAFER-PHD2.0-6PLB (C55130937).
A 2.54 mm 2x3 SMD pin header (hanxia HX PZ2.54-2x3P TP, C32713283, 44k in
stock) would also fit the board with a new footprint, but it is not keyed.

Pinout (odd row = cable from the previous globe or the controller, even row =
cable to the next globe; the pin-1 mark is next to the odd data pin):

| Pin | Row | Signal |
| --- | --- | --- |
| 1 | odd | DIN (to the ring's IN) |
| 2 | even | DOUT (from the ring's OUT) |
| 3 | odd | GND |
| 4 | even | GND |
| 5 | odd | +5V |
| 6 | even | +5V |

The connector is shifted 0.8 mm towards the DIN/DOUT side so its solder tails
clear the right-hand GND pin pad. Its housing ends cover the inner 0.4 mm of
the four outer pin pads; the holes themselves and the outer half of each pad
stay free, which is enough to solder the pins from the outside.

## Assembly

1. Put the five pins into the adapter from the connector side, solder them on
   the connector side, trim flush on top.
2. Slide the ring onto the protruding pins, back to back with the adapter,
   solder on the LED side between the LEDs, trim.
3. Plug the cables: contacts crimped at both ends; at each globe the three
   wires of the incoming cable go into the odd positions, the outgoing cable
   into the even positions of one PHDR-06VS housing.

## Production

Board: 23 mm round, 2 layers, 1.6 mm, JLCPCB rules from the template. For the
JLCPCB BOM/CPL use the Fabrication Toolkit plugin as for the controller board;
J1 carries the `LCSC Part #` field, J2 (the ring interface) is excluded from
BOM and position files.
