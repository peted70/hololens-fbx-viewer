#pragma once
#include "DagNode.h"
#include "Mesh.h"
#include <vector>

using namespace std;

class Model :
	public DagNode
{
public:
	Model();
	virtual ~Model();

	void AddMesh(shared_ptr<Mesh> mesh);
	void SetIndexBuffer(GLuint *indices, int numIndices);

	void SetPositionAttribLocation(GLint positionAttribLocation);
	void SetColorAttribLocation(GLint colorAttribLocation);

	void PreRender(bool isHolographic);

	virtual void Render(bool isHolographic);

	void Loaded() { _loaded = true; }

private:
	GLint _positionAttribLocation;
	GLint _colorAttribLocation;

	vector<shared_ptr<Mesh>> _meshes;
	bool _loaded;
};

