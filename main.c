#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include "dat.h"
#include "fns.h"

Rune keys[Ke] = {
 [K↑]		Kup,
 [K←]		Kleft,
 [K→]		Kright,
 [Kfire]	' ',
 [Knav]		'n',
 [Kquit]	'q'
};

Mousectl *mctl;
int kdown;
Channel *scrsync;
Point orig;
Vector basis;
Spacecraft ship;
Asteroid asteroids;
int nasteroid;
Triangle shipmdl, thrustmdl;
Point asteroidmdl[Maxbisect][16];
Image *colors[Cend];
double t0, Δt;
int shownav, showthrust;
QLock pauselk;
int paused;
int ∞flag;

char stats[Se][32];
int score;

void *
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("malloc: %r");
	memset(p, 0, n);
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void
addasteroid(Asteroid *a)
{
	asteroids.prev->next = a;
	a->prev = asteroids.prev;
	a->next = &asteroids;
	asteroids.prev = a;
}

Asteroid *
delasteroid(Asteroid *a)
{
	Asteroid *an;

	an = a->next;
	a->prev->next = an;
	an->prev = a->prev;
	free(a);
	return an;
}

void
splitasteroid(Asteroid *a)
{
	Asteroid *newa;

	newa = emalloc(sizeof(Asteroid));
	newa->p = a->p;
	newa->v.x = a->v.x*cos(PI/2) - a->v.y*sin(PI/2);
	newa->v.y = a->v.x*sin(PI/2) + a->v.y*cos(PI/2);
	newa->bisectno = a->bisectno;
	addasteroid(newa);
	newa = emalloc(sizeof(Asteroid));
	newa->p = a->p;
	newa->v.x = a->v.x*cos(-PI/2) - a->v.y*sin(-PI/2);
	newa->v.y = a->v.x*sin(-PI/2) + a->v.y*cos(-PI/2);
	newa->bisectno = a->bisectno;
	addasteroid(newa);
	nasteroid += 2;
}

Point
vectopt(Vector v)
{
	return Pt(v.x, v.y);
}

Point
toscreen(Vector p)
{
	return addpt(orig, Pt(p.x*basis.x, p.y*basis.y));
}

Vector
fromscreen(Point p)
{
	return Vec(p.x-screen->r.min.x, screen->r.max.y-p.y);
}

void
wrapcoord(Vector *p)
{
	if(p->x > Dx(screen->r))
		p->x = 0;
	if(p->y > Dy(screen->r))
		p->y = 0;
	if(p->x < 0)
		p->x = Dx(screen->r);
	if(p->y < 0)
		p->y = Dy(screen->r);
}

void
drawstats(void)
{
	int i, nfired;

	snprint(stats[Sscore], sizeof(stats[Sscore]), "SCORE %d", score);
	if(∞flag)
		snprint(stats[Sshield], sizeof(stats[Sshield]), "SHIELDS ∞");
	else
		snprint(stats[Sshield], sizeof(stats[Sshield]), "SHIELDS %d", ship.shields);
	for(i = nfired = 0; i < nelem(ship.ammo); i++)
		if(ship.ammo[i].fired)
			nfired++;
	snprint(stats[Sammo], sizeof(stats[Sammo]), "AMMO %d/%d", nelem(ship.ammo)-nfired, nelem(ship.ammo));
	for(i = 0; i < Se; i++)
		stringn(screen, addpt(screen->r.min, Pt(10, 10 + i*font->height)), display->white, ZP, font, stats[i], strlen(stats[i]));
}

void
drawship(void)
{
	Triangle shipmdltrans, thrustmdltrans;
	int i;

	shipmdltrans = rotatriangle(shipmdl, ship.yaw, Pt(0, 0));
	triangle(screen, Trianpt(
		toscreen(addvec(ship.p, Vpt(shipmdltrans.p0))),
		toscreen(addvec(ship.p, Vpt(shipmdltrans.p1))),
		toscreen(addvec(ship.p, Vpt(shipmdltrans.p2)))
	), 0, display->white, ZP);
	if(showthrust){
		thrustmdltrans = rotatriangle(thrustmdl, ship.yaw, Pt(0, 0));
		filltriangle(screen, Trianpt(
			toscreen(addvec(ship.p, Vpt(thrustmdltrans.p0))),
			toscreen(addvec(ship.p, Vpt(thrustmdltrans.p1))),
			toscreen(addvec(ship.p, Vpt(thrustmdltrans.p2)))
		), colors[Cthrust], ZP);
	}
	for(i = 0; i < nelem(ship.ammo); i++)
		if(ship.ammo[i].fired)
			line(screen, toscreen(ship.ammo[i].p), toscreen(subvec(ship.ammo[i].p, Vec(cos(ship.ammo[i].yaw)*8, sin(ship.ammo[i].yaw)*8))), 0, 0, 0, colors[Cbullet], ZP);
}

void
drawasteroids(void)
{
	Asteroid *ap;
	Point asteroidmdltrans[16];
	int i;

	for(ap = asteroids.next; ap != &asteroids; ap = ap->next){
		for(i = 0; i < nelem(asteroidmdl[ap->bisectno]); i++)
			asteroidmdltrans[i] = toscreen(addvec(ap->p, Vpt(asteroidmdl[ap->bisectno][i])));
		poly(screen, asteroidmdltrans, nelem(asteroidmdltrans), 0, 0, 0, colors[Casteroid], ZP);
	}
}

void
redraw(void)
{

	lockdisplay(display);
	draw(screen, screen->r, display->black, nil, ZP);
	if(shownav){
		line(screen, toscreen(ship.p), toscreen(addvec(ship.p, ship.v)), 0, 0, 0, colors[Cprov], ZP);
		line(screen, toscreen(ship.p), toscreen(addvec(ship.p, mulvec(ship.v, -1))), 0, 0, 0, colors[Cretrov], ZP);
	}
	drawship();
	drawasteroids();
	drawstats();
	if(paused)
		stringn(screen, addpt(screen->r.min, Pt(Dx(screen->r)/2-3*font->width, Dy(screen->r)/2)), display->white, ZP, font, "PAUSED", 6);
	flushimage(display, 1);
	unlockdisplay(display);
}

void
congrats(void)
{
	char *s[] = {
		"CONGRATS!",
		"YOU DID IT!"
	};
	int i;

	lockdisplay(display);
	draw(screen, screen->r, display->black, nil, ZP);
	for(i = 0; i < nelem(s); i++)
		stringn(screen, addpt(divpt(addpt(screen->r.min, screen->r.max), 2), Pt(-strlen(s[i])/2*font->width, i*font->height)), colors[Cprov], ZP, font, s[i], strlen(s[i]));
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(5*SEC);
	threadexitsall(nil);
}

void
gameover(void)
{
	char s[] = "GAME OVER";
	char ss[48];

	snprint(ss, sizeof ss, "FINAL SCORE %d", score);
	lockdisplay(display);
	draw(screen, screen->r, display->black, nil, ZP);
	stringn(screen, addpt(divpt(addpt(screen->r.min, screen->r.max), 2), Pt(-strlen(s)/2*font->width, 0)), colors[Cretrov], ZP, font, s, strlen(s));
	stringn(screen, addpt(divpt(addpt(screen->r.min, screen->r.max), 2), Pt(-strlen(ss)/2*font->width, font->height)), colors[Cretrov], ZP, font, ss, strlen(ss));
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(5*SEC);
	threadexitsall(nil);
}

void
fire(void)
{
	int i;

	for(i = 0; i < nelem(ship.ammo); i++)
		if(!ship.ammo[i].fired){
			ship.ammo[i].p = ship.p;
			ship.ammo[i].v = addvec(ship.v, Vec(cos(ship.yaw)*BSPEED, sin(ship.yaw)*BSPEED));
			ship.ammo[i].yaw = ship.yaw;
			ship.ammo[i].fired++;
			break;
		}
}

void
scrsynproc(void *)
{
	for(;;){
		send(scrsync, nil);
		sleep(SEC/FPS);
	}
}

void
mouse(void)
{
	Vector p;

	Δt = (nsec()-t0)/1e9;
	kdown &= ~(1<<K↑);
	if(mctl->buttons & 1){
		p = subvec(fromscreen(mctl->xy), ship.p);
		ship.yaw = atan2(p.y, p.x);
		kdown |= 1<<K↑;
	}
}

void
kbdproc(void *)
{
	Rune r, *a;
	char buf[128], *s;
	int fd, n;

	threadsetname("kbdproc");
	if((fd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("kbdproc: %r");
	memset(buf, 0, sizeof buf);
	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			if((n = read(fd, buf, sizeof(buf)-1)) <= 0)
				break;
			buf[n-1] = 0;
			buf[n] = 0;
		}
		if(buf[0] == 'c'){
			if(utfrune(buf, Kdel)){
				close(fd);
				threadexitsall(nil);
			}
			if(utfrune(buf, Kesc)){
				if(paused)
					qunlock(&pauselk);
				else
					qlock(&pauselk);
				paused = !paused;
				redraw();
			}
		}
		if(buf[0] != 'k' && buf[0] != 'K')
			continue;
		s = buf+1;
		kdown = 0;
		while(*s){
			s += chartorune(&r, s);
			for(a = keys; a < keys+Ke; a++)
				if(r == *a){
					kdown |= 1 << a-keys;
					break;
				}
		}
	}
}

void
handlekeys(void)
{
	showthrust = 0;
	if(kdown & 1<<Kquit)
		threadexitsall(nil);
	if(kdown & 1<<K←)
		ship.yaw += PI * Δt;
	if(kdown & 1<<K→)
		ship.yaw -= PI * Δt;
	if(kdown & 1<<K↑){
		showthrust = 1;
		ship.v = addvec(ship.v, Vec(
			cos(ship.yaw) * THRUST*Δt,
			sin(ship.yaw) * THRUST*Δt));
	}
	if(kdown & 1<<Kfire)
		fire(), kdown &= ~(1<<Kfire);
	if(kdown & 1<<Knav)
		shownav = !shownav, kdown &= ~(1<<Knav);
}

void
resized(void)
{
	lockdisplay(display);
	if(getwindow(display, Refnone) < 0)
		fprint(2, "can't reattach to window\n");
	unlockdisplay(display);
	orig = Pt(screen->r.min.x, screen->r.max.y);
	redraw();
}

Image *
rgb(u32int c)
{
	Image *i;

	if((i = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, c)) == nil)
		sysfatal("allocimage: %r");
	return i;
}

void
initcolors(void)
{
	colors[Casteroid] = rgb(0xcc8844ff);
	colors[Cthrust] = rgb(DYellow);
	colors[Cbullet] = rgb(DRed);
	colors[Cprov] = rgb(DGreen);
	colors[Cretrov] = rgb(DYellow);
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	Asteroid *ap, *newa;
	char *s;
	double elev, avgelev[Maxbisect];
	int i, j, waspaused;

	nasteroid = 4;
	ARGBEGIN{
	case 'n':
		nasteroid = strtol(EARGF(usage()), &s, 0);
		if(s != nil)
			usage();
		break;
	case L'∞': ∞flag++; break;
	default: usage();
	}ARGEND;

	if(initdraw(nil, nil, "asteroids") < 0)
		sysfatal("initdraw: %r");
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	initcolors();
	srand(time(0));
	orig = Pt(screen->r.min.x, screen->r.max.y);
	basis = Vec(1, -1);
	memset(avgelev, 0, sizeof avgelev);
	for(i = 0; i < Maxbisect; i++){
		for(j = 0; j < nelem(asteroidmdl[i])-1; j++){
			elev = frand()*(80-16*i) + (75-16*i);
			asteroidmdl[i][j] = Pt(
				elev*cos(((double)j/nelem(asteroidmdl[i])) * 2*PI),
				elev*sin(((double)j/nelem(asteroidmdl[i])) * 2*PI)
			);
			avgelev[i] += elev;
		}
		asteroidmdl[i][j] = asteroidmdl[i][0];
		avgelev[i] /= j;
	}
	shipmdl = Trian(
		8, 0,
		-4, 4,
		-4, -4
	);
	thrustmdl = Trian(
		-12, 0,
		-4, 2,
		-4, -2
	);
	ship.p = Vec(Dx(screen->r)/2, Dy(screen->r)/2);
	ship.yaw = 90*DEG;
	ship.shields = 5;
	asteroids.prev = asteroids.next = &asteroids;
	for(i = 0; i < nasteroid; i++){
		srand(nsec());
		newa = emalloc(sizeof(Asteroid));
		newa->p = Vec(
			ntruerand(Dx(screen->r)),
			ntruerand(Dy(screen->r))
		);
		newa->v = Vec(cos(frand() * 2*PI)*50+10, sin(frand() * 2*PI)*50+10);
		addasteroid(newa);
	}
	display->locking = 1;
	unlockdisplay(display);
	scrsync = chancreate(1, 0);
	proccreate(scrsynproc, nil, STACKSZ);
	proccreate(kbdproc, nil, STACKSZ);
	t0 = nsec();
	for(;;){
		enum{MOUSE, RESIZE, SCRSYN};
		Alt a[] = {
			{mctl->c, &mctl->Mouse, CHANRCV},
			{mctl->resizec, nil, CHANRCV},
			{scrsync, nil, CHANRCV},
			{nil, nil, CHANEND}
		};
		switch(alt(a)){
		case MOUSE: mouse(); break;
		case RESIZE: resized(); break;
		case SCRSYN: redraw(); break;
		}
		waspaused = paused;
		qlock(&pauselk);
		if(waspaused)
			t0 = nsec();
		Δt = (nsec()-t0)/1e9;
		handlekeys();
		ship.p = addvec(ship.p, mulvec(ship.v, Δt));
		wrapcoord(&ship.p);
		for(i = 0; i < nelem(ship.ammo); i++)
			if(ship.ammo[i].fired){
				ship.ammo[i].p = addvec(ship.ammo[i].p, mulvec(ship.ammo[i].v, Δt));
				if(!ptinrect(vectopt(ship.ammo[i].p), Rect(0, 0, Dx(screen->r), Dy(screen->r))))
					ship.ammo[i].fired--;
			}
		for(ap = asteroids.next; ap != &asteroids; ap = ap->next){
			ap->p = addvec(ap->p, mulvec(ap->v, Δt));
			wrapcoord(&ap->p);
			if(!∞flag && triangleXcircle(Trianpt(
				addpt(vectopt(ship.p), shipmdl.p0),
				addpt(vectopt(ship.p), shipmdl.p1),
				addpt(vectopt(ship.p), shipmdl.p2)
			   ), vectopt(ap->p), avgelev[ap->bisectno])){
				if(!ap->stillin && --ship.shields == 0)
					gameover();
				ap->stillin = 1;
			}else
				ap->stillin = 0;
			for(i = 0; i < nelem(ship.ammo); i++)
				if(ship.ammo[i].fired && ptincircle(vectopt(ship.ammo[i].p), vectopt(ap->p), avgelev[ap->bisectno])){
					ship.ammo[i].fired--;
					score += 50*ap->bisectno;
					if(++ap->bisectno < Maxbisect)
						splitasteroid(ap);
					ap = delasteroid(ap);
					if(--nasteroid == 0)
						congrats();
				}
		}
		t0 += Δt*1e9;
		qunlock(&pauselk);
	}
}
