#pragma once
class Colour
{
public:
	Colour() : 
		_r(0), _g(0), _b(0), _a(1) {}
	Colour(double r, double g, double b, double a) : 
		_r(r), _g(g), _b(b), _a(a) {}
	~Colour();

	double _r;
	double _g;
	double _b;
	double _a;
};

