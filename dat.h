#define DEG 0.01745329251994330

enum {
	STACKSZ = 8192,
	SEC = 1000,
	THRUST = 50,
	BSPEED = 300,
	FPS = 60,
	Maxbisect = 4
};

enum {
	K↑,
	K←,
	K→,
	Kfire,
	Knav,
	Kquit,
	Ke
};

enum {
	Casteroid,
	Cthrust,
	Cbullet,
	Cprov,
	Cretrov,
	Cend
};

enum {
	Sscore,
	Sshield,
	Sammo,
	Se
};

typedef struct Vector Vector;
typedef struct Triangle Triangle;
typedef struct Particle Particle;
typedef struct Asteroid Asteroid;
typedef struct Bullet Bullet;
typedef struct Spacecraft Spacecraft;

struct Vector {
	double x, y;
};

struct Triangle {
	Point p0, p1, p2;
};

struct Particle {
	Vector p, v;
	double yaw;
};

struct Asteroid {
	Particle;
	int stillin, bisectno;
	Asteroid *prev, *next;
};

struct Bullet {
	Particle;
	int fired;
};

struct Spacecraft {
	Particle;
	int shields;
	Bullet ammo[5];
};
