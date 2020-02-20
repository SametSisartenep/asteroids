#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "fns.h"

Vector
Vec(double x, double y)
{
	return (Vector){x, y};
}

Vector
Vpt(Point p)
{
	return (Vector){p.x, p.y};
}

Vector
addvec(Vector v, Vector u)
{
	return (Vector){v.x+u.x, v.y+u.y};
}

Vector
subvec(Vector v, Vector u)
{
	return (Vector){v.x-u.x, v.y-u.y};
}

Vector
mulvec(Vector v, double s)
{
	return (Vector){v.x*s, v.y*s};
}

double
dotvec(Vector v, Vector u)
{
	return v.x*u.x + v.y*u.y;
}

Vector
normvec(Vector v)
{
	double len;

	len = hypot(v.x, v.y);
	if(len == 0)
		return (Vector){0, 0};
	v.x /= len;
	v.y /= len;
	return v;
}
