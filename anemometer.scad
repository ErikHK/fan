$fn = 256;
id = 8.6;

w = id*2+14;

h = 12;

th = 2;

wth = 2;


difference()
{
union()
{
    
    cylinder(d=id+wth*2, h=h);
    
    translate([-w/2,-th/2,0])
    cube([w,th,h]);

    rotate([0,0,45])
    translate([-w/2,-th/2,0])
    cube([w,th,h]);

    rotate([0,0,-45])
    translate([-w/2,-th/2,0])
    cube([w,th,h]);


    rotate([0,0,90])
    translate([-w/2,-th/2,0])
    cube([w,th,h]);
    
}

cylinder(d=id, h=h+1);
}