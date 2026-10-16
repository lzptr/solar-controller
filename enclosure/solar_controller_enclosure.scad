// Solar controller enclosure: base plate, top shell, knob.
//
// Frame: PCB coordinates. x across the board (0..38), y along it
// (0 = USB-C edge, 70 = LED connector edge), z up from the back of the base
// plate (the side that goes onto the bed post).
//
// Assembly: PCB sits on the base plate standoffs, the top shell drops over it,
// 4x M2.5x10 countersunk screws go in from the back through base plate and PCB
// into heat-set inserts (M2.5 x 5 x 3.5) in the shell posts.
//
// The lid artwork is engraved 0.8 mm and filled by a separate inlay part, so
// the top shell prints in two colours (lid face-down, inlay in the first layers).

/* [Output] */
// assembly / exploded for viewing; base, shell, inlay, knob = print-oriented single parts
part = "exploded"; // [assembly, exploded, base, shell, inlay, knob, print]
show_pcb = true;

/* [Fit] */
insert_hole_d = 3.3;     // heat-set insert pocket
insert_depth = 6.0;      // 5 mm insert + 1 mm for displaced plastic
screw_d = 2.9;           // M2.5 clearance
csk_d = 5.3;             // countersink top diameter (DIN 7991 M2.5 head = 5.0)
plate_gap = 0.2;         // base plate to inner wall, per side
knob_bore_d = 6.2;       // encoder shaft 5.975
knob_flat = 4.6;         // D-flat 4.45 across
cable_d = 5.0;           // LED cable incl. sleeve
cable_squeeze = 0.6;     // clamp is this much smaller than the cable

/* [Hidden] */
$fa = 4;
$fs = 0.3;
eps = 0.01;

// PCB
pcb_w = 38;
pcb_l = 70;
pcb_t = 1.6;
holes = [[3.5, 3.5], [34.5, 3.5], [34.5, 66.5], [3.5, 66.5]];
enc = [19, 42];
usb_x = 19;
cable_x = 19;

// Stack
base_t = 2.0;
standoff_h = 3.0;
pcb_z = base_t + standoff_h;      // PCB bottom
pcb_top = pcb_z + pcb_t;
post_gap = 0.1;                   // posts stop short so the board is always clamped
shell_h = 22;
lid_t = 2.0;
lid_z = shell_h - lid_t;          // underside of the lid

// Outline: 44 x 90, flush with the 44 mm bed post
out = [-3, -3, pcb_w + 3, 87];
out_r = 5;
top_chamfer = 1.2;
// Cavity: 1 mm beside the board, USB end flush with the USB-C front (1 mm overhang)
inn = [-1, -1.1, pcb_w + 1, 85];
inn_r = 3;

// Posts / supports
post_d = 6.5;
enc_support_d = 6.8;

// Encoder and knob (ALPS EC11J1525402: shaft tip 26.1 above PCB bottom, 7 mm D-flat)
shaft_tip_z = pcb_z + 26.1;
lid_hole_d = 10;                  // the 9 mm shaft collar ends inside the lid
knob_d = 23;
knob_h = 11;
knob_gap = 2.0;                   // above the lid; push travel is 1.5
knob_z = shell_h + knob_gap;
knob_flutes = 24;

// USB-C opening (fits plug overmolds up to ~12.6 x 7.0)
usb_open = [13.0, 7.4];
usb_open_r = 3.0;
usb_zc = pcb_z + 3.18;

// LED cable clamp
cable_zc = 8.0;
clamp_r = (cable_d - cable_squeeze) / 2;
saddle_w = 9;
saddle_y0 = 79.8;
rib_y = 80.6;
rib_t = 1.6;

// C404's plastic base comes within 3.0 mm of the H103 post axis, 0.4-0.9 mm above
// the PCB (the can itself stays 4.2 mm away): that post gets a thinner neck at its end
neck_holes = [2];                 // index into holes: H103
neck_d = 5.5;
neck_h = 2.0;

// Artwork on the lid, seen with the LED cable up: moon left, sun right
art_depth = 0.8;               // 4 layers of white at 0.2 mm
art_line = 0.9;
art_r = 17;
icon_r = 17;

// ---------------------------------------------------------------------------

module rrect(b, r) {
    hull() for (x = [b[0] + r, b[2] - r], y = [b[1] + r, b[3] - r])
        translate([x, y]) circle(r = r);
}

module shell_outer() {
    hull() {
        linear_extrude(shell_h - top_chamfer) rrect(out, out_r);
        linear_extrude(shell_h) offset(delta = -top_chamfer) rrect(out, out_r);
    }
}

module artwork() {
    // arc over the top of the knob
    translate(enc) {
        intersection() {
            difference() {
                circle(r = art_r + art_line / 2);
                circle(r = art_r - art_line / 2);
            }
            polygon([[0, 0], 50 * [cos(50), sin(50)], [0, 50], 50 * [cos(130), sin(130)]]);
        }
        for (a = [50, 130]) translate(art_r * [cos(a), sin(a)]) circle(d = art_line);
    }
    // moon (lit limb towards the sun)
    translate(enc + icon_r * [cos(150), sin(150)]) difference() {
        circle(r = 2.6);
        translate([-1.1, 0.7]) circle(r = 2.2);
    }
    // sun
    translate(enc + icon_r * [cos(30), sin(30)]) {
        circle(r = 1.7);
        for (i = [0:7]) rotate(i * 45) translate([2.4, -0.4]) square([1.3, 0.8]);
    }
}

// Cable channel along y at the clamp height (lower and upper half)
module cable_channel(y0, y1) {
    translate([cable_x, y0, cable_zc]) rotate([-90, 0, 0]) cylinder(r = clamp_r, h = y1 - y0);
}

module shell() {
    difference() {
        union() {
            difference() {
                shell_outer();
                translate([0, 0, -eps]) linear_extrude(lid_z + eps) rrect(inn, inn_r);
            }
            for (h = holes) translate([h[0], h[1], pcb_top + post_gap]) {
                cylinder(d = post_d, h = lid_z - pcb_top - post_gap + eps);
                translate([0, 0, lid_z - pcb_top - post_gap - 2])
                    cylinder(d1 = post_d, d2 = post_d + 3, h = 2 + eps);
            }
            // pinch rib: presses the cable into the base plate saddle
            translate([cable_x - saddle_w / 2, rib_y - rib_t / 2, cable_zc + 0.2])
                cube([saddle_w, rib_t, lid_z - cable_zc - 0.2 + eps]);
        }
        // insert pockets with a small lead-in
        for (h = holes) translate([h[0], h[1], pcb_top + post_gap - eps]) {
            cylinder(d = insert_hole_d, h = insert_depth);
            cylinder(d1 = insert_hole_d + 0.6, d2 = insert_hole_d, h = 0.3);
        }
        // knob shaft
        translate([enc.x, enc.y, lid_z - 1]) cylinder(d = lid_hole_d, h = lid_t + 2);
        // USB-C
        translate([usb_x, out[1] - 1, usb_zc]) rotate([-90, 0, 0])
            linear_extrude(inn[1] - out[1] + 1 + eps)
                rrect([-usb_open.x / 2, -usb_open.y / 2, usb_open.x / 2, usb_open.y / 2], usb_open_r);
        // LED cable: slot in the end wall (open towards the base plate) + notch in the rib
        cable_channel(rib_y - rib_t, out[3] + 1);
        translate([cable_x - clamp_r, inn[3] - eps, -1]) cube([2 * clamp_r, out[3] - inn[3] + 1, cable_zc + 1]);
        // C404 clearance: neck at the PCB end of the H103 post
        for (i = neck_holes) translate([holes[i][0], holes[i][1], pcb_top + post_gap - eps]) difference() {
            cylinder(d = post_d + 1, h = neck_h + eps);
            translate([0, 0, -eps]) cylinder(d = neck_d, h = neck_h + 3 * eps);
        }
        // lead-in chamfer for the base plate
        hull() {
            translate([0, 0, -eps]) linear_extrude(eps) offset(delta = 0.4) rrect(inn, inn_r);
            linear_extrude(0.4) rrect(inn, inn_r);
        }
        translate([0, 0, shell_h - art_depth]) linear_extrude(art_depth + 1) artwork();
    }
}

module base() {
    plate = [inn[0] + plate_gap, inn[1] + plate_gap, inn[2] - plate_gap, inn[3] - plate_gap];
    difference() {
        union() {
            linear_extrude(base_t) rrect(plate, inn_r - plate_gap);
            for (h = holes) translate([h[0], h[1], 0]) cylinder(d = post_d, h = pcb_z);
            translate([enc.x, enc.y, 0]) cylinder(d = enc_support_d, h = pcb_z);
            // cable saddle and the tongue that fills the end wall slot below the cable
            translate([cable_x - saddle_w / 2, saddle_y0, 0])
                cube([saddle_w, plate[3] - saddle_y0, cable_zc - 0.2]);
            translate([cable_x - clamp_r + 0.15, plate[3] - eps, 0])
                cube([2 * clamp_r - 0.3, out[3] - plate[3] + eps, cable_zc - clamp_r]);
        }
        for (h = holes) translate([h[0], h[1], 0]) {
            translate([0, 0, -1]) cylinder(d = screw_d, h = pcb_z + 2);
            translate([0, 0, -eps]) cylinder(d1 = csk_d, d2 = screw_d, h = (csk_d - screw_d) / 2);
        }
        translate([enc.x, enc.y, base_t]) cylinder(d = 3, h = pcb_z);  // encoder peg relief
        cable_channel(saddle_y0 - 1, out[3] + 1);
    }
    // two ridges in the saddle groove to grip the sleeve
    for (y = [82, 84]) intersection() {
        translate([cable_x, y - 0.3, cable_zc]) rotate([-90, 0, 0]) difference() {
            cylinder(r = clamp_r + eps, h = 0.6);
            translate([0, 0, -1]) cylinder(r = clamp_r - 0.5, h = 3);
        }
        translate([cable_x - 5, y - 1, 0]) cube([10, 2, cable_zc - 0.2]);
    }
}

module knob() {
    translate([enc.x, enc.y, knob_z]) difference() {
        rotate_extrude() polygon([
            [0, 0], [knob_d / 2 - 0.6, 0], [knob_d / 2, 0.6],
            [knob_d / 2, knob_h - 1.0], [knob_d / 2 - 1.0, knob_h], [0, knob_h]]);
        for (i = [0:knob_flutes - 1]) rotate(i * 360 / knob_flutes)
            translate([knob_d / 2 + 0.35, 0, -1]) cylinder(r = 1.0, h = knob_h + 2);
        // round entry (shaft is round below the flat), then the D-bore
        translate([0, 0, -eps]) cylinder(d = knob_bore_d, h = 0.5);
        translate([0, 0, -eps]) linear_extrude(shaft_tip_z + 0.5 - knob_z) intersection() {
            circle(d = knob_bore_d);
            translate([-knob_bore_d / 2, -knob_bore_d / 2]) square([knob_bore_d, knob_flat]);
        }
    }
}

module pcb() {
    if (show_pcb) translate([0, 0, pcb_z]) color("ForestGreen") import("ref/pcb.stl");
}

// Fills the lid engraving exactly: second part of the top shell for a white filament
module artwork_inlay() {
    translate([0, 0, shell_h - art_depth]) linear_extrude(art_depth) artwork();
}

// Print orientation: base as is, shell and knob upside down (top faces on the bed)
module base_print() { base(); }
module shell_print() { translate([0, 0, shell_h]) rotate([180, 0, 0]) shell(); }
module inlay_print() { translate([0, 0, shell_h]) rotate([180, 0, 0]) artwork_inlay(); }
module knob_print() {
    translate([0, 0, knob_z + knob_h]) rotate([180, 0, 0]) knob();
}

if (part == "assembly") {
    color("DimGray") base();
    color("#3a3a3a", 0.55) shell();
    color("WhiteSmoke") artwork_inlay();
    color("Peru") knob();
    pcb();
} else if (part == "exploded") {
    color("DimGray") translate([0, 0, -18]) base();
    color("#3a3a3a") translate([0, 0, 22]) shell();
    color("WhiteSmoke") translate([0, 0, 22]) artwork_inlay();
    color("Peru") translate([0, 0, 40]) knob();
    pcb();
} else if (part == "base") {
    base_print();
} else if (part == "shell") {
    shell_print();
} else if (part == "inlay") {
    inlay_print();
} else if (part == "knob") {
    knob_print();
} else if (part == "check") {
    // assembled position, one object per part (base, shell, inlay, knob, pcb) for ref/assembly_check.3mf
    base();
    shell();
    artwork_inlay();
    knob();
    translate([0, 0, pcb_z]) import("ref/pcb.stl");
} else if (part == "print") {
    // object order matters for the 3MF post-processing: base, shell, inlay, knob
    base_print();
    translate([55, 0, 0]) shell_print();
    translate([55, 0, 0]) inlay_print();
    translate([95, 0, 0]) knob_print();
}
