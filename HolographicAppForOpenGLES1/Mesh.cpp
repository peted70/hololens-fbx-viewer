#include "pch.h"
#include "Mesh.h"
#include "utils.h"

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

void Mesh::SetVertices(unique_ptr<GLfloat[]> vertices, int numVertices)
{
	// assume ownership of the verices passed in..
	_vertices = std::move(vertices);

	glGenBuffers(1, &_vertexPositionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * numVertices, _vertices.get(), GL_STATIC_DRAW);

	auto vertColours = make_unique<GLfloat[]>(numVertices*4);

	// just generate some vertex colours for time being...
	for (int i = 0; i < numVertices * 4; i+=4)
	{
		vertColours[i] = 1.0f;
		vertColours[i+1] = 0.0f;
		vertColours[i+2] = 0.0f;
		vertColours[i+3] = 1.0f;
	}

	glGenBuffers(1, &_vertexColorBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, _vertexColorBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 4 * numVertices, vertColours.get(), GL_STATIC_DRAW);

	checkGlError(L"SetVertices");
}

void Mesh::SetIndexBuffer(unique_ptr<unsigned short[]> indices, int numIndices)
{
	_numIndices = numIndices;
	_vertex_indices = std::move(indices);

	glGenBuffers(1, &_index_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _index_vbo);
	auto ind = _vertex_indices.get();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * numIndices, _vertex_indices.get(), GL_STATIC_DRAW);
	checkGlError(L"SetIndexBuffer");
}

void Mesh::SetPositionAttribLocation(GLint positionAttribLocation)
{
	_positionAttribLocation = positionAttribLocation;
}

void Mesh::SetColorAttribLocation(GLint colorAttribLocation)
{
	_colorAttribLocation = colorAttribLocation;
}

void Mesh::Render(bool isHolographic)
{
	PreRender(isHolographic);

	//glDisable(GL_TEXTURE_2D);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _index_vbo);
	checkGlError(L"glBindBuffer");
	if (isHolographic)
	{
		glDrawElementsInstancedANGLE(GL_TRIANGLES, _numIndices, GL_UNSIGNED_SHORT, 0, 2);
	}
	else
	{
		//GL_TRIANGLES_ADJACENCY
		// GL_TRIANGLE_STRIP
		glDrawElements(GL_TRIANGLES, _numIndices, GL_UNSIGNED_SHORT, 0);
	}

	checkGlError(L"glDrawElements");
}

void Mesh::PreRender(bool isHolographic)
{
	checkGlError(L"Render");
	glBindBuffer(GL_ARRAY_BUFFER, _vertexPositionBuffer);
	checkGlError(L"glBindBuffer");
	glEnableVertexAttribArray(_positionAttribLocation);
	checkGlError(L"glEnableVertexAttribArray");
	glVertexAttribPointer(_positionAttribLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
	checkGlError(L"glVertexAttribPointer");
	glBindBuffer(GL_ARRAY_BUFFER, _vertexColorBuffer);
	checkGlError(L"glBindBuffer");
	glEnableVertexAttribArray(_colorAttribLocation);
	checkGlError(L"glEnableVertexAttribArray");
	glVertexAttribPointer(_colorAttribLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
	checkGlError(L"glVertexAttribPointer");
}

