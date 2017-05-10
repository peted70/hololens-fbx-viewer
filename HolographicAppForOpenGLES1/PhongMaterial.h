#pragma once
#include "Material.h"
#include "Colour.h"

class PhongMaterial : public Material
{
public:
	PhongMaterial();
	~PhongMaterial();

private:
	Colour _ambient;
	Colour _diffuse;
	Colour _specular;
	Colour _emissive;

	double _opacity;
	double _shininess;
	double _reflectivity;
};

