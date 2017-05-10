#pragma once
#include <fbxsdk.h>
#include <fbxsdk\scene\geometry\fbxgeometry.h>
#include <fbxsdk\fileio\fbximporter.h>
#include <functional>
#include "Model.h"
#include "utils.h"

using namespace fbxsdk;
using namespace std;

class Importer
{
public:
	Importer();
	~Importer();

	unique_ptr<Model> LoadModelFromFile(const char * filename);
	void ConnectMaterialToMesh(FbxMesh * pMesh, int triangleCount, int * pTriangleMtlIndex);
	void SetShaderAttributes(GLuint positionAttribLocation, GLuint colorAttribLocation);

protected:
	void ImportFile(const char * filename);
	void TraverseScene(FbxNode * node, function<void(FbxMesh*)> callback);
	Vector3 ReadNormal(FbxMesh * inMesh, int inCtrlPointIndex, int inVertexCounter);
	void PrintNode(FbxNode * pNode);
	void PrintTabs();
	FbxString GetAttributeTypeName(FbxNodeAttribute::EType type);
	void PrintAttribute(FbxNodeAttribute * pAttribute);
	void DisplayMaterial(FbxGeometry * pGeometry);

private:
	FbxManager *_sdkManager;
	FbxIOSettings *_settings;
	FbxImporter *_importer;
	FbxScene *_scene;

	/* Tab character ("\t") counter */
	int _numTabs = 0;
	GLuint _positionAttribLocation;
	GLuint _colorAttribLocation;
};

