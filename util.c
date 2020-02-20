#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

double
round(double n)
{
	return floor(n + 0.5);
}

Point
rotatept(Point p, double θ, Point c)
{
	Point r;

	p = subpt(p, c);
	r.x = round(p.x*cos(θ) - p.y*sin(θ));
	r.y = round(p.x*sin(θ) + p.y*cos(θ));
	r = addpt(r, c);
	return r;
}

int
ptincircle(Point p, Point c, double r)
{
	Point d;

	d = subpt(c, p);
	return hypot(d.x, d.y) < r;
}

int
triangleXcircle(Triangle t, Point c, double r)
{
	return ptincircle(t.p0, c, r) ||
		ptincircle(t.p1, c, r) ||
		ptincircle(t.p2, c, r);
}
