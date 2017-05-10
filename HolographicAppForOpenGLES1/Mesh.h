#pragma once
#include <vector>
#include "Material.h"

using namespace std;

class Mesh
{
public:
	Mesh();
	~Mesh();

	void SetVertexColors(unique_ptr<GLfloat[]> colors, int numVertices);

	void SetVertices(unique_ptr<GLfloat[]> vertices, int numVertices);
	void SetNormals(unique_ptr<GLfloat[]> normals, int numVertices);
	void SetIndexBuffer(unique_ptr<unsigned short[]> indices, int numIndices);
	void SetPositionAttribLocation(GLint positionAttribLocation);
	void SetColorAttribLocation(GLint colorAttribLocation);
	void Render(bool isHolographic);
	void PreRender(bool isHolographic);

private:
	GLint _positionAttribLocation;
	GLint _colorAttribLocation;
	GLuint _vertexPositionBuffer;
	GLuint _vertexColorBuffer;
	GLuint _normalsBuffer;

	unique_ptr<GLfloat[]> _vertices;
	GLfloat *vertices2;
	unique_ptr<unsigned short[]> _vertex_indices;
	unique_ptr<GLfloat[]> _normals;
	int _numIndices;
	GLuint _index_vbo;
	vector<unique_ptr<Material>> _materials;
};

