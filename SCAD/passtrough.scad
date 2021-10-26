/******************************************************************************
 * Passtrough module for Schneider Electric Harmony XAL housings
 * Version 1.0 (2021-10-25)
 * ----------------------------------------------------------------------------
 * Copyright (c) Michal A. Valasek, 2021 | www.rider.cz | www.altair.blog
 *               Licensed under terms of the MIT license
 *****************************************************************************/

use <A2D.scad>;
use <_threads.scad>;

/* [Dimensions] */
// Inner diameter of hole in housing
thread_diameter = 20;
// Outer diameter of cable going trough
cable_diameter = 5;
// Maximum outer diameter
washer_diameter = 30;
// Thickness of circular washer part
washer_thickness = 2;
// Height of outer nut (without integrated washer)
nut_height = 3;
// Housing wall thickness
nut_spacing = 4;
// Distance of wrench flats
nut_flat_distance = 27;
// Split inner part to two halves
split_inner = true;

/* [Hidden] */
print_distance = 2;
print_tolerance = .25;
$fudge = 1;

/* Main render ***************************************************************/

translate([-washer_diameter * .6, 0]) part_inner();
translate([+washer_diameter * .6, 0]) part_outer();

/* Parts *********************************************************************/

module part_inner() {
    if(split_inner) {
        // Big slice
        translate([0, -print_distance / 2]) difference() {
            inner();
            wedge();
        }

        // Small slice
        translate([0, +print_distance / 2]) intersection() {
            inner();
            translate([0, print_tolerance]) wedge();
        }
    } else {
        inner();
    }
}

module part_outer() {
    ScrewHole(20, washer_thickness + nut_height) {
        if(nut_flat_distance == 0) {
            linear_extrude(washer_thickness + nut_height) r_square([washer_diameter, washer_diameter], radius = washer_diameter * .25, center = true, $fn = 32);
        } else {
            intersection() {
                translate([-nut_flat_distance / 2, -washer_diameter / 2]) cube([nut_flat_distance, washer_diameter, washer_thickness + nut_height]);
                cylinder(d = washer_diameter, h = washer_thickness + nut_height, $fn = 64);
            }
            cylinder(d = washer_diameter, h = washer_thickness, $fn = 64);
        }
    }
}

/* Helper modules ************************************************************/

module inner() {
    difference() {
        // Outer shape
        union() {
            linear_extrude(washer_thickness) r_square([washer_diameter, washer_diameter], radius = washer_diameter * .25, center = true, $fn = 32);
            translate([0, 0, washer_thickness]) ScrewThread(thread_diameter, nut_height + nut_spacing);
        }

        // Cable hole
        translate([0, 0, -$fudge]) cylinder(d = cable_diameter, h = washer_thickness + nut_height + nut_spacing + 2 * $fudge, $fn = 8);
    }
}

module wedge() {
    translate([0, 0, -$fudge]) linear_extrude(washer_thickness + nut_height + nut_spacing + 2 * $fudge) polygon([[0 , 0], [washer_diameter, washer_diameter], [-washer_diameter, washer_diameter]]);
}