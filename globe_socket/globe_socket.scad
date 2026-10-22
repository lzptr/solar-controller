// String-light socket for the 75 mm solar-system lithophane globes (mobile version).
//
// Use: the globe hangs with its threaded opening up. The socket screws into that
// opening, the LED ring + adapter stack hangs below it inside the globe (LEDs
// pointing down), and the two string cables enter the crown from the sides
// (-x: from the previous globe / controller, +x: to the next globe). The cap
// presses both cables into their channels, so the globe hangs on the cable
// jackets, never on the PHD connector.
//
// Frame: z up in use, z = 0 is the globe's top face (flange underside),
// x = cable axis, mounting holes of the LED stack on x as well.
//
// Assembly
//   1. Push the cable's PHD housing down through the socket bore, mate it with
//      the adapter's J1 below the socket.
//   2. Screw the ring + adapter to the two posts: 2x M2x8 from the LED side.
//   3. Lay the cables into the side channels, cap on, 2x M2x8 countersunk.
//   4. Screw the socket into the globe.

/* [Output] */
// assembly / cutaway / exploded for viewing; body, cap = print-oriented single parts
part = "cutaway"; // [assembly, cutaway, exploded, body, cap, print]
show_globe = true;
show_stack = true;

/* [Cable] */
cable_d = 4.0;            // string cable outer diameter (jacket)
cable_squeeze = 0.6;      // the cap compresses the jacket by this much

/* [LED stack] */
led_drop = 4.0;           // plug bottom to adapter top (posts); more = LEDs deeper in the globe
mated_h = 11.0;           // adapter top to top of the mated PHD housing (wafer 8.1 + housing 7.6, engaged)
wire_bend = 5.0;          // housing top to cable axis, room for the wires to turn

/* [Fit] */
thread_clearance = 0.0;   // radial, taken off the socket thread (0 = profile of the original generic socket)
post_hole_d = 1.75;       // M2 self-tapping into the posts
post_hole_depth = 8.0;
cap_screw_d = 2.4;        // M2 clearance in the cap
cap_csk_d = 4.2;          // M2 countersunk head (ISO 10642: 3.8)
cap_pilot_d = 1.75;       // M2 self-tapping into the crown
tongue_gap = 0.15;        // per side, cap tongue in the cable slot
hang_loop = false;        // eyelet on the cap for an extra hanging thread

/* [Hidden] */
$fa = 3;
$fs = 0.25;
eps = 0.01;

// Globe opening (measured on venus_surface_mobile): right-hand internal thread, pitch 2.0,
// crest d 26.4, root d 27.8, 7.5 mm deep incl. a 1 mm lead-in chamfer, flat face out to d 37.
// Socket thread = the author's generic socket: root d 25.52, crest d 27.42, pitch 2.0,
// per pitch 0.74 root flat, 0.55 flank, 0.16 crest flat, 0.55 flank.
pitch = 2.0;
th_root = 12.76;
th_crest = 13.71;
th_len = 6.5;
bore_d = 20.0;            // through bore: PHD housing (8 x 5.05) and wires pass

// LED ring + adapter (23 mm, 2x 2.9 mm holes 19.2 apart, 1.6 mm boards, LEDs 1.6 mm)
ring_d = 23.0;
hole_x = 9.6;
pcb_t = 1.6;
led_h = 1.6;
post_d = 5.0;
conn_x = -0.8;            // adapter J1 centre (8.0 along y, 5.55 along x, 8.1 tall)

// Heights
z_a = -th_len - led_drop;             // adapter top (posts end here)
z_c = z_a + mated_h + wire_bend;      // cable axis
flange_d = 36.0;
flange_h = 1.2;
crown_d = 32.0;
crown_top = z_c + cable_d / 2 + 1.0;
cap_t = 2.0;
clamp_r = cable_d / 2;
cap_holes = [[0, 13], [0, -13]];

assert(z_c - cable_d / 2 > flange_h + 2.0, "cable channel runs into the flange: raise mated_h/wire_bend or lower led_drop");

// ---------------------------------------------------------------------------- thread

function prof(u) = let(v = u - pitch * floor(u / pitch))
    v < 0.74 ? th_root :
    v < 1.29 ? th_root + (v - 0.74) / 0.55 * (th_crest - th_root) :
    v < 1.45 ? th_crest :
    th_crest - (v - 1.45) / 0.55 * (th_crest - th_root);

// horizontal section at z = 0 of a right-hand thread r(a, z) = prof(z - a * pitch / 360)
module thread_section(n = 180) {
    polygon([for (i = [0:n - 1]) let(a = i * 360 / n)
        (prof(-a * pitch / 360) - thread_clearance) * [cos(a), sin(a)]]);
}

// twisting the section counter-clockwise with z (negative twist in OpenSCAD) gives the helix
module thread(h) {
    linear_extrude(h, twist = -360 * h / pitch, slices = ceil(h / pitch * 90), convexity = 10)
        thread_section();
}

// ---------------------------------------------------------------------------- body

module body_outer() {
    // plug: thread with a 45 degree lead-in at the bottom
    intersection() {
        translate([0, 0, -th_len]) thread(th_len + eps);
        translate([0, 0, -th_len]) cylinder(h = th_len + 2 * eps, r1 = th_root, r2 = th_root + th_len);
    }
    // flange on the globe face, 45 degree cone to the crown (prints upside down without support)
    rotate_extrude() polygon([
        [0, 0], [flange_d / 2 - 0.4, 0], [flange_d / 2, 0.4], [flange_d / 2, flange_h],
        [crown_d / 2, flange_h + (flange_d - crown_d) / 2],
        [crown_d / 2, crown_top - 0.6], [crown_d / 2 - 0.6, crown_top], [0, crown_top]]);
}

module cable_channel() {
    // U-channel along x, open towards the cap
    translate([0, 0, z_c]) rotate([0, 90, 0]) cylinder(r = clamp_r, h = crown_d + 10, center = true);
    translate([-(crown_d + 10) / 2, -clamp_r, z_c]) cube([crown_d + 10, 2 * clamp_r, crown_top - z_c + 1]);
}

// two ribs across each channel floor, 0.5 mm high
module grip_ribs() {
    for (s = [-1, 1], xr = [11.5, 14.5]) intersection() {
        translate([s * xr - 0.4, -clamp_r, z_c - clamp_r - 1]) cube([0.8, 2 * clamp_r, clamp_r + 1]);
        translate([0, 0, z_c]) rotate([0, 90, 0]) difference() {
            cylinder(r = clamp_r + eps, h = crown_d + 10, center = true);
            cylinder(r = clamp_r - 0.5, h = crown_d + 12, center = true);
        }
    }
}

module body() {
    difference() {
        union() {
            difference() {
                body_outer();
                translate([0, 0, -th_len - 1]) cylinder(d = bore_d, h = crown_top + th_len + 2);
            }
            // posts for the LED stack, continuing as ribs up the bore wall
            for (s = [-1, 1]) translate([s * hole_x, 0, z_a]) cylinder(d = post_d, h = -z_a);
        }
        for (s = [-1, 1]) translate([s * hole_x, 0, z_a - eps]) {
            cylinder(d = post_hole_d, h = post_hole_depth);
            cylinder(d1 = post_hole_d + 0.6, d2 = post_hole_d, h = 0.3);
        }
        cable_channel();
        for (h = cap_holes) translate([h.x, h.y, flange_h]) cylinder(d = cap_pilot_d, h = crown_top);
    }
    grip_ribs();
}

// ---------------------------------------------------------------------------- cap

module cap() {
    translate([0, 0, crown_top]) difference() {
        union() {
            hull() {
                cylinder(d = crown_d, h = cap_t - 0.6);
                cylinder(d = crown_d - 1.2, h = cap_t);
            }
            // tongues fill the channels above the cable
            for (s = [-1, 1]) translate([s > 0 ? bore_d / 2 + 0.3 : -crown_d / 2, -(clamp_r - tongue_gap), z_c - crown_top])
                cube([crown_d / 2 - bore_d / 2 - 0.3, 2 * (clamp_r - tongue_gap), crown_top - z_c + eps]);
            // locating ring in the bore
            translate([0, 0, -1.5]) difference() {
                cylinder(d = bore_d - 0.4, h = 1.5 + eps);
                translate([0, 0, -1]) cylinder(d = bore_d - 3.0, h = 3);
            }
            if (hang_loop) translate([0, 0, cap_t]) rotate([90, 0, 0]) difference() {
                hull() {
                    translate([0, 4]) cylinder(r = 3.5, h = 3, center = true);
                    translate([-4.5, 0, -1.5]) cube([9, eps, 3]);
                }
                translate([0, 4]) cylinder(r = 2, h = 4, center = true);
            }
        }
        // cable notch: centre lowered by the squeeze
        translate([0, 0, z_c - cable_squeeze - crown_top]) rotate([0, 90, 0]) cylinder(r = clamp_r, h = crown_d + 10, center = true);
        // keep the locating ring clear of the channels
        translate([-bore_d / 2, -clamp_r - 0.3, -2]) cube([bore_d, 2 * clamp_r + 0.6, 2 - eps]);
        for (h = cap_holes) translate([h.x, h.y, -1]) {
            cylinder(d = cap_screw_d, h = cap_t + 2);
            translate([0, 0, 1 + cap_t - (cap_csk_d - cap_screw_d) / 2]) cylinder(d1 = cap_screw_d, d2 = cap_csk_d, h = (cap_csk_d - cap_screw_d) / 2 + eps);
        }
    }
}

// ---------------------------------------------------------------------------- reference models

module led_stack() {
    color("DarkGreen") translate([0, 0, z_a - pcb_t]) difference() {
        cylinder(d = ring_d, h = pcb_t);
        for (s = [-1, 1]) translate([s * hole_x, 0, -1]) cylinder(d = 2.9, h = 4);
    }
    color("#202020") translate([0, 0, z_a - 2 * pcb_t]) difference() {
        cylinder(d = ring_d, h = pcb_t);
        for (s = [-1, 1]) translate([s * hole_x, 0, -1]) cylinder(d = 2.9, h = 4);
    }
    color("LightYellow") for (p = concat([[0, 0]], [for (a = [30:60:330]) 7.7 * [cos(a), sin(a)]]))
        translate([p.x, p.y, z_a - 2 * pcb_t - led_h / 2]) cube([5, 5, led_h], center = true);
    // PHD wafer and mated housing
    color("Ivory") translate([conn_x - 2.775, -4, z_a]) cube([5.55, 8, 8.1]);
    color("White") translate([conn_x - 2.525, -4, z_a + mated_h - 7.6]) cube([5.05, 8, 7.6]);
    // screws
    color("Silver") for (s = [-1, 1]) translate([s * hole_x, 0, z_a - 2 * pcb_t - 1.3]) {
        cylinder(d = 3.8, h = 1.3);
        cylinder(d = 2, h = 8);
    }
}

module cables() {
    color("#303030") for (s = [-1, 1]) translate([0, 0, z_c]) rotate([0, s * 90, 0])
        translate([0, 0, 9]) cylinder(d = cable_d, h = 25);
}

// Globe around the opening, flipped to hang opening-up (measured outline, relief ignored)
g_c = -34.83;             // globe centre
module globe() {
    color("Wheat", 0.35) difference() {
        union() {
            intersection() {
                translate([0, 0, g_c]) sphere(r = 37.2);
                translate([0, 0, g_c - 40]) cylinder(r = 40, h = 40 - g_c);
            }
            rotate_extrude() polygon([[13.2, 0], [19.6, 0], [22.8, -2], [25.3, -4], [27.4, -6], [29.1, -8.5], [13.2, -8.5]]);
        }
        translate([0, 0, g_c]) intersection() {
            sphere(r = 35.6);
            translate([0, 0, -40]) cylinder(r = 40, h = 40 + 34.83 - 8.5);
        }
        translate([0, 0, -8.6]) cylinder(r = 13.55, h = 9);
        translate([0, 0, -1]) cylinder(r1 = 13.55, r2 = 14.92, h = 1 + eps);
    }
}

// ---------------------------------------------------------------------------- output

module body_print() { translate([0, 0, crown_top]) rotate([180, 0, 0]) body(); }
module cap_print() {
    if (hang_loop) translate([0, 0, 1.5 - crown_top]) cap();
    else translate([0, 0, crown_top + cap_t]) rotate([180, 0, 0]) cap();
}

module cut() { translate([-60, -120, -100]) cube([120, 120, 200]); }

if (part == "assembly") {
    color("WhiteSmoke") body();
    color("Gainsboro") cap();
    if (show_stack) { led_stack(); cables(); }
    if (show_globe) globe();
} else if (part == "cutaway") {
    difference() { color("WhiteSmoke") body(); cut(); }
    difference() { color("Gainsboro") cap(); cut(); }
    if (show_stack) { led_stack(); cables(); }
    if (show_globe) difference() { globe(); cut(); }
} else if (part == "exploded") {
    color("WhiteSmoke") body();
    color("Gainsboro") translate([0, 0, 14]) cap();
    if (show_stack) { translate([0, 0, -14]) led_stack(); cables(); }
} else if (part == "body") {
    body_print();
} else if (part == "cap") {
    cap_print();
} else if (part == "print") {
    body_print();
    translate([42, 0, 0]) cap_print();
}
