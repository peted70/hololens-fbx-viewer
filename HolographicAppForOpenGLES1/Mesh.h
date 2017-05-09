#pragma once

using namespace std;

class Mesh
{
public:
	Mesh();
	~Mesh();

	void SetVertices(unique_ptr<GLfloat[]> vertices, int numVertices);
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

	unique_ptr<GLfloat[]> _vertices;
	GLfloat *vertices2;
	unique_ptr<unsigned short[]> _vertex_indices;
	int _numIndices;
	GLuint _index_vbo;
};

