#include "pch.h"
#include "Model.h"

Model::Model() : _loaded(false)
{
}

Model::~Model()
{
}

void Model::AddMesh(shared_ptr<Mesh> mesh)
{
	mesh->SetPositionAttribLocation(_positionAttribLocation);
	mesh->SetColorAttribLocation(_colorAttribLocation);
	_meshes.push_back(mesh);
}

void Model::SetPositionAttribLocation(GLint positionAttribLocation)
{
	_positionAttribLocation = positionAttribLocation;
}

void Model::SetColorAttribLocation(GLint colorAttribLocation)
{
	_colorAttribLocation = colorAttribLocation;
}

void Model::PreRender(bool isHolographic)
{
	if (!_loaded)
		return;

	for (auto mesh : _meshes)
	{
		mesh->PreRender(isHolographic);
	}
}

void Model::Render(bool isHolographic)
{
	if (!_loaded)
		return;

	for (auto mesh: _meshes)
	{
		mesh->Render(isHolographic);
	}
}

