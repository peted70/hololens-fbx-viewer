//
// This file is used by the template to render a basic scene using GL.
//

#include "pch.h"
#include "SimpleRenderer.h"
#include "MathHelper.h"

// These are used by the shader compilation methods.
#include <vector>
#include <iostream>
#include <fstream>
#include <functional>

#include "fbxsdk.h"
#include "fbxsdk\utils\fbxgeometryconverter.h"
#include "Model.h"
#include "utils.h"

using namespace Platform;
using namespace HolographicAppForOpenGLES1;

#define STRING(s) #s

GLuint CompileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);

    const char *sourceArray[1] = { source.c_str() };
    glShaderSource(shader, 1, sourceArray, NULL);
    glCompileShader(shader);

    GLint compileResult;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);

    if (compileResult == 0)
    {
        GLint infoLogLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<GLchar> infoLog(infoLogLength);
        glGetShaderInfoLog(shader, (GLsizei)infoLog.size(), NULL, infoLog.data());

        std::wstring errorMessage = std::wstring(L"Shader compilation failed: ");
        errorMessage += std::wstring(infoLog.begin(), infoLog.end()); 

        throw Exception::CreateException(E_FAIL, ref new Platform::String(errorMessage.c_str()));
    }

    return shader;
}

GLuint CompileProgram(const std::string &vsSource, const std::string &fsSource)
{
    GLuint program = glCreateProgram();

    if (program == 0)
    {
        throw Exception::CreateException(E_FAIL, L"Program creation failed");
    }

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);

    if (vs == 0 || fs == 0)
    {
        glDeleteShader(fs);
        glDeleteShader(vs);
        glDeleteProgram(program);
        return 0;
    }

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    if (linkStatus == 0)
    {
        GLint infoLogLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<GLchar> infoLog(infoLogLength);
        glGetProgramInfoLog(program, (GLsizei)infoLog.size(), NULL, infoLog.data());

        std::wstring errorMessage = std::wstring(L"Program link failed: ");
        errorMessage += std::wstring(infoLog.begin(), infoLog.end()); 

        throw Exception::CreateException(E_FAIL, ref new Platform::String(errorMessage.c_str()));
    }

    return program;
}

/* Tab character ("\t") counter */
int numTabs = 0;

/**
* Print the required number of tabs.
*/
void PrintTabs() 
{
	for (int i = 0; i < numTabs; i++)
		DebugLog(L"\t");
}

/**
* Return a string-based representation based on the attribute type.
*/
FbxString GetAttributeTypeName(FbxNodeAttribute::EType type) 
{
	switch (type) 
	{
	case FbxNodeAttribute::eUnknown: return "unidentified";
	case FbxNodeAttribute::eNull: return "null";
	case FbxNodeAttribute::eMarker: return "marker";
	case FbxNodeAttribute::eSkeleton: return "skeleton";
	case FbxNodeAttribute::eMesh: return "mesh";
	case FbxNodeAttribute::eNurbs: return "nurbs";
	case FbxNodeAttribute::ePatch: return "patch";
	case FbxNodeAttribute::eCamera: return "camera";
	case FbxNodeAttribute::eCameraStereo: return "stereo";
	case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
	case FbxNodeAttribute::eLight: return "light";
	case FbxNodeAttribute::eOpticalReference: return "optical reference";
	case FbxNodeAttribute::eOpticalMarker: return "marker";
	case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
	case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
	case FbxNodeAttribute::eBoundary: return "boundary";
	case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
	case FbxNodeAttribute::eShape: return "shape";
	case FbxNodeAttribute::eLODGroup: return "lodgroup";
	case FbxNodeAttribute::eSubDiv: return "subdiv";
	default: return "unknown";
	}
}

void PrintAttribute(FbxNodeAttribute* pAttribute) 
{
	if (!pAttribute) return;

	FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
	FbxString attrName = pAttribute->GetName();
	PrintTabs();
	// Note: to retrieve the character array of a FbxString, use its Buffer() method.
	DebugLog(L"<attribute type='%s' name='%s'/>\n", typeName.Buffer(), attrName.Buffer());
}

void PrintNode(FbxNode* pNode) 
{
	PrintTabs();
	const char* nodeName = pNode->GetName();
	FbxDouble3 translation = pNode->LclTranslation.Get();
	FbxDouble3 rotation = pNode->LclRotation.Get();
	FbxDouble3 scaling = pNode->LclScaling.Get();

	// Print the contents of the node.
	DebugLog(L"<node name='%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
		nodeName,
		translation[0], translation[1], translation[2],
		rotation[0], rotation[1], rotation[2],
		scaling[0], scaling[1], scaling[2]
	);
	numTabs++;

	// Print the node's attributes.
	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
		PrintAttribute(pNode->GetNodeAttributeByIndex(i));

	// Recursively print the children.
	for (int j = 0; j < pNode->GetChildCount(); j++)
		PrintNode(pNode->GetChild(j));

	numTabs--;
	PrintTabs();
	DebugLog(L"</node>\n");
}

void TraverseScene(FbxNode *node, std::function<void(FbxMesh *)> callback)
{
	if (node == nullptr)
		return;

	FbxMesh* mesh = node->GetMesh();
	if (mesh != nullptr)
	{
		callback(mesh);
	}

	for (int i = 0; i < node->GetChildCount(); i++)
	{
		TraverseScene(node->GetChild(i), callback);
	}
}

SimpleRenderer::SimpleRenderer(bool isHolographic) :
    mWindowWidth(1268),
    mWindowHeight(720),
    mDrawCount(0)
{

	// Vertex Shader source
    const std::string vs = isHolographic ?
        STRING
    (
        // holographic version

        uniform mat4 uModelMatrix;
        uniform mat4 uHolographicViewProjectionMatrix[2];
        attribute vec4 aPosition;
        attribute vec4 aColor;
        attribute float aRenderTargetArrayIndex;
        varying vec4 vColor;
        varying float vRenderTargetArrayIndex;
        void main()
        {
            int arrayIndex = int(aRenderTargetArrayIndex); // % 2; // TODO: integer modulus operation supported on ES 3.00 only
            gl_Position = uHolographicViewProjectionMatrix[arrayIndex] * uModelMatrix * aPosition;
            vColor = aColor;
            vRenderTargetArrayIndex = aRenderTargetArrayIndex;
        }
    ) : STRING
    (
        uniform mat4 uModelMatrix;
        uniform mat4 uViewMatrix;
        uniform mat4 uProjMatrix;
        attribute vec4 aPosition;
        attribute vec4 aColor;
        varying vec4 vColor;
        void main()
        {
            gl_Position = uProjMatrix * uViewMatrix * uModelMatrix * aPosition;
            vColor = aColor;
        }
    );

    // Fragment Shader source
    const std::string fs = isHolographic ? // TODO: this should not be necessary
        STRING
    (
        precision mediump float;
        varying vec4 vColor;
        varying float vRenderTargetArrayIndex; // TODO: this should not be necessary
        void main()
        {
            gl_FragColor = vColor;
            float index = vRenderTargetArrayIndex;
        }
    ) : STRING
    (
        precision mediump float;
        varying vec4 vColor;
        void main()
        {
            gl_FragColor = vColor;
        }
    );

    // Set up the shader and its uniform/attribute locations.
    mProgram = CompileProgram(vs, fs);
    mPositionAttribLocation = glGetAttribLocation(mProgram, "aPosition");
    mColorAttribLocation = glGetAttribLocation(mProgram, "aColor");
    mRtvIndexAttribLocation = glGetAttribLocation(mProgram, "aRenderTargetArrayIndex");
    mModelUniformLocation = glGetUniformLocation(mProgram, "uModelMatrix");
    mViewUniformLocation = glGetUniformLocation(mProgram, "uViewMatrix");
    mProjUniformLocation = glGetUniformLocation(mProgram, "uProjMatrix");

	_model = std::make_unique<Model>();

	_model->SetPositionAttribLocation(mPositionAttribLocation);
	_model->SetColorAttribLocation(mColorAttribLocation);

	const char *filename = "./Assets/hlscaled.fbx";
	FbxManager *sdkManager = FbxManager::Create();

	FbxIOSettings *settings = FbxIOSettings::Create(sdkManager, IOSROOT);

	FbxImporter* lImporter = FbxImporter::Create(sdkManager, "");

	// Use the first argument as the filename for the importer.
	if (!lImporter->Initialize(filename, -1, sdkManager->GetIOSettings()))
	{
	}

	// Create a new scene so that it can be populated by the imported file.
	FbxScene* lScene = FbxScene::Create(sdkManager, "myScene");

	// Import the contents of the file into the scene.
	lImporter->Import(lScene);

	// The file is imported, so get rid of the importer.
	lImporter->Destroy();

	// Print the nodes of the scene and their attributes recursively.
	// Note that we are not printing the root node because it should
	// not contain any attributes.
	FbxNode* lRootNode = lScene->GetRootNode();

	if (lRootNode)
	{
		for (int i = 0; i < lRootNode->GetChildCount(); i++)
			PrintNode(lRootNode->GetChild(i));

		FbxGeometryConverter clsConverter(sdkManager);
		clsConverter.Triangulate(lScene, false);

		// Create A Model type, pass it in here and fill it in...
		TraverseScene(lRootNode, [this, &sdkManager](FbxMesh *fbxMesh)
		{
			// Get vertices from the mesh
			int numVertices = fbxMesh->GetControlPointsCount();
			auto verts = make_unique<GLfloat[]>(numVertices * 4);
			auto mesh = make_shared<Mesh>();

			for (int j = 0; j < numVertices; j++)
			{
				FbxVector4 coord = fbxMesh->GetControlPointAt(j);

				verts.get()[j * 4 + 0] = (GLfloat)coord.mData[0];
				verts.get()[j * 4 + 1] = (GLfloat)coord.mData[1];
				verts.get()[j * 4 + 2] = (GLfloat)coord.mData[2];
				verts.get()[j * 4 + 3] = 1.0f;

				DebugLog(L"Vertex %d - {%f %f %f}", j, (GLfloat)coord.mData[0],
					(GLfloat)coord.mData[1], (GLfloat)coord.mData[2]);
			}

			// Transfer ownership to the model..
			mesh->SetVertices(std::move(verts), numVertices);

			int numIndices = fbxMesh->GetPolygonVertexCount();
			int* indices = fbxMesh->GetPolygonVertices();

			auto isMesh = fbxMesh->IsTriangleMesh();
			auto count = fbxMesh->GetPolygonCount();
			
			auto idxes = make_unique<unsigned short[]>(numIndices);
			for (int j = 0; j < numIndices; j++)
			{
				idxes[j] = indices[j];
			}

			for (int j = 0; j < numIndices; j++)
			{
				DebugLog(L"Index %d - {%d}", j, idxes[j]);
			}

			mesh->SetIndexBuffer(std::move(idxes), numIndices);
			_model->AddMesh(mesh);
		});

		_model->Loaded();
	}

	// Destroy the SDK manager and all the other objects it was handling.
	sdkManager->Destroy();

    float renderTargetArrayIndices[] = { 0.f, 1.f };
    glGenBuffers(1, &mRenderTargetArrayIndices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mRenderTargetArrayIndices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(renderTargetArrayIndices), renderTargetArrayIndices, GL_STATIC_DRAW);

    mIsHolographic = isHolographic;
}

SimpleRenderer::~SimpleRenderer()
{
    if (mProgram != 0)
    {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }

    if (mVertexPositionBuffer != 0)
    {
        glDeleteBuffers(1, &mVertexPositionBuffer);
        mVertexPositionBuffer = 0;
    }

    if (mVertexColorBuffer != 0)
    {
        glDeleteBuffers(1, &mVertexColorBuffer);
        mVertexColorBuffer = 0;
    }

    if (mIndexBuffer != 0)
    {
        glDeleteBuffers(1, &mIndexBuffer);
        mIndexBuffer = 0;
    }
}

void SimpleRenderer::Draw()
{
    glEnable(GL_DEPTH_TEST);

    // On HoloLens, it is important to clear to transparent.
    glClearColor(0.0f, 0.f, 1.f, 0.f);

    // On HoloLens, this will also update the camera buffers (constant and back).
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
    
	if (mProgram == 0)
        return;

    glUseProgram(mProgram);

    MathHelper::Vec3 position = MathHelper::Vec3(0.f, 0.f, -10.f);
    MathHelper::Matrix4 modelMatrix = MathHelper::SimpleModelMatrix((float)mDrawCount / 50.0f, position);
    glUniformMatrix4fv(mModelUniformLocation, 1, GL_FALSE, &(modelMatrix.m[0][0]));

    if (mIsHolographic)
    {
        // Load the render target array indices into an array.
        glBindBuffer(GL_ARRAY_BUFFER, mRenderTargetArrayIndices);
        glVertexAttribPointer(mRtvIndexAttribLocation, 1, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(mRtvIndexAttribLocation);

        // Enable instancing.
        glVertexAttribDivisorANGLE(mRtvIndexAttribLocation, 1);
		_model->Render(mIsHolographic);
	}
    else
    {
        MathHelper::Matrix4 viewMatrix = MathHelper::SimpleViewMatrix();
        glUniformMatrix4fv(mViewUniformLocation, 1, GL_FALSE, &(viewMatrix.m[0][0]));

        MathHelper::Matrix4 projectionMatrix = MathHelper::SimpleProjectionMatrix(float(mWindowWidth) / float(mWindowHeight));
        glUniformMatrix4fv(mProjUniformLocation, 1, GL_FALSE, &(projectionMatrix.m[0][0]));

		_model->Render(mIsHolographic);
	}

    mDrawCount += 1;
}

void SimpleRenderer::UpdateWindowSize(GLsizei width, GLsizei height)
{
    if (!mIsHolographic)
    {
        glViewport(0, 0, width, height);

        mWindowWidth = width;
        mWindowHeight = height;
    }
}
