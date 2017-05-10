#include "pch.h"
#include "Importer.h"
#include <fbxsdk.h>
#include "utils.h"
#include "Model.h"
#include <iterator>

static wchar_t* currentwidecharbuffer = nullptr;
static int wcharcurrentsize = 0;

const wchar_t *CH_TO_WCH(const char *original)
{
	size_t size = strlen(original) + 1;

	// Just leak a fixed amount..
	if (size > wcharcurrentsize)
	{
		wcharcurrentsize = size;
		currentwidecharbuffer = (wchar_t *)realloc(currentwidecharbuffer, 
			wcharcurrentsize * sizeof(wchar_t));
	}

	size_t outSize;
	mbstowcs_s(&outSize, currentwidecharbuffer, size, original, size - 1);

	return currentwidecharbuffer;
}

Importer::Importer()
{
	_sdkManager = FbxManager::Create();
	_settings = FbxIOSettings::Create(_sdkManager, IOSROOT);
	_numTabs = 0;
}

Importer::~Importer()
{
	if (_scene != nullptr)
		_scene->Destroy(true);

	// Destroy the SDK manager and all the other objects it was handling.
	_sdkManager->Destroy();
}

unique_ptr<Model> Importer::LoadModelFromFile(const char *filename)
{
	// Import the file into an fbx scene
	ImportFile(filename);
	FbxNode *rootNode = _scene->GetRootNode();
	if (rootNode == nullptr)
		return nullptr;

	// Print out details of the whole scene..
	for (int i = 0; i < rootNode->GetChildCount(); i++)
		PrintNode(rootNode->GetChild(i));

	// Convert the scene to triangles..
	FbxGeometryConverter clsConverter(_sdkManager);
	clsConverter.Triangulate(_scene, false);

	auto model = std::make_unique<Model>();
	model->SetPositionAttribLocation(_positionAttribLocation);
	model->SetColorAttribLocation(_colorAttribLocation);

	// Create A Model type, pass it in here and fill it in...
	TraverseScene(rootNode, [this, &model](FbxMesh *fbxMesh)
	{
		DisplayMaterial(fbxMesh);
		
		//auto node = fbxMesh->GetNode();
		//int numMats = node->GetMaterialCount();
		//for (int i = 0; i < numMats; i++)
		//{
		//	auto mt = node->GetMaterial(i);
		//	FbxProperty prop = mt->FindProperty(FbxSurfaceMaterial::sDiffuse);
		//	if (prop.IsValid())
		//	{
		//		auto lKFbxDouble3 = ((FbxSurfacePhong *)mt)->Diffuse;
		//	}
		//}
		// Not sure yet if we are supplied with a mesh per material or what
		// leave this code here for now in case we need to work this out..
		const int polygonCount = fbxMesh->GetPolygonCount();
		int numMaterials = fbxMesh->GetElementMaterialCount();
		for (int i = 0; i < numMaterials; i++)
		{
			auto material = fbxMesh->GetElementMaterial(i);
			if (material)
			{
				// The following call with cause a problem - crashing the scene
				// destroy() method - don't know the issue but leave it alone.
				//auto matIndices = material->GetIndexArray();
				//auto materialMappingMode = material->GetMappingMode();
				//material->

				//if (materialMappingMode == FbxGeometryElement::eByPolygon)
				//{
				//	//int count = matIndices.GetCount();
				//	//int y = count;
				//}
			}

		}
		//if (lMesh->GetElementMaterial()) {

		//	lMaterialIndice = &lMesh->GetElementMaterial()->GetIndexArray();

		//	lMaterialMappingMode = lMesh->GetElementMaterial()->GetMappingMode();

		//	if (lMaterialIndice && lMaterialMappingMode == FbxGeometryElement::eByPolygon) {

		//		FBX_ASSERT(lMaterialIndice->GetCount() == lPolygonCount);

		//		if (lMaterialIndice->GetCount() == lPolygonCount) {

		// Get vertices from the mesh
		int numVertices = fbxMesh->GetControlPointsCount();
		auto verts = make_unique<GLfloat[]>(numVertices * 4);
		auto mesh = make_shared<Mesh>();
		auto normals = make_unique<GLfloat[]>(numVertices * 3);
		for (int j = 0; j < numVertices; j++)
		{
			FbxVector4 coord = fbxMesh->GetControlPointAt(j);

			verts.get()[j * 4 + 0] = (GLfloat)coord.mData[0];
			verts.get()[j * 4 + 1] = (GLfloat)coord.mData[1];
			verts.get()[j * 4 + 2] = (GLfloat)coord.mData[2];
			verts.get()[j * 4 + 3] = 1.0f;

			DebugLog(L"Vertex %d - {%f %f %f}", j, (GLfloat)coord.mData[0],
				(GLfloat)coord.mData[1], (GLfloat)coord.mData[2]);

			Vector3 ret = ReadNormal(fbxMesh, j, j);
			normals.get()[j * 3 + 0] = ret.x;
			normals.get()[j * 3 + 1] = ret.y;
			normals.get()[j * 3 + 2] = ret.z;

			DebugLog(L"Normal %d - {%f %f %f}", j, ret.x, ret.y, ret.z);
		}

		// Transfer ownership to the model..
		mesh->SetVertices(std::move(verts), numVertices);
		mesh->SetNormals(std::move(normals), numVertices);
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

		// Set the vertex colours...
		int numTris = fbxMesh->GetPolygonCount();
		auto triangleMapping = make_unique<int[]>(numVertices);
		for (int i = 0; i < numVertices; i++)
		{
			DebugLog(L"tri %d -> %d", i, triangleMapping[i]);
		}

		ConnectMaterialToMesh(fbxMesh, numTris, triangleMapping.get());

		auto node = fbxMesh->GetNode();

		auto colors = make_unique<GLfloat[]>(numVertices*4);

		// Look up the materials diffuse colours, and...
		for (int i = 0; i < numVertices; i++)
		{
			// The triangle mapping has been stored by looping over each triangle
			// and querying it's material and storing the indices..
			int matIdx = triangleMapping[i];
			auto mt = node->GetMaterial(matIdx);
			FbxProperty prop = mt->FindProperty(FbxSurfaceMaterial::sDiffuse);
			if (prop.IsValid())
			{
				auto diffuse = prop.Get<FbxColor>();
				colors[4 * i + 0] = diffuse.mRed;
				colors[4 * i + 1] = diffuse.mGreen;
				colors[4 * i + 2] = diffuse.mBlue;
				colors[4 * i + 3] = diffuse.mAlpha;
			}
		}

		mesh->SetVertexColors(std::move(colors), numVertices);
		model->AddMesh(mesh);
	});

	model->Loaded();
	return model;
}

void Importer::ConnectMaterialToMesh(FbxMesh* pMesh, int triangleCount, int* pTriangleMtlIndex)
{
	// Get the material index list of current mesh  
	FbxLayerElementArrayTemplate<int>* pMaterialIndices;
	FbxGeometryElement::EMappingMode materialMappingMode = FbxGeometryElement::eNone;

	if (pMesh->GetElementMaterial())
	{
		pMaterialIndices = &pMesh->GetElementMaterial()->GetIndexArray();
		materialMappingMode = pMesh->GetElementMaterial()->GetMappingMode();
		if (pMaterialIndices)
		{
			switch (materialMappingMode)
			{
			case FbxGeometryElement::eByPolygon:
				{
					if (pMaterialIndices->GetCount() == triangleCount)
					{
						for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
						{
							int materialIndex = pMaterialIndices->GetAt(triangleIndex);
							pTriangleMtlIndex[triangleIndex] = materialIndex;
						}
					}
				}
				break;

			case FbxGeometryElement::eAllSame:
				{
					int lMaterialIndex = pMaterialIndices->GetAt(0);

					for (int triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
					{
						int materialIndex = pMaterialIndices->GetAt(triangleIndex);
						pTriangleMtlIndex[triangleIndex] = materialIndex;
					}
				}
			}
		}
	}
}

void Importer::SetShaderAttributes(GLuint positionAttribLocation, GLuint colorAttribLocation)
{
	_positionAttribLocation = positionAttribLocation;
	_colorAttribLocation = colorAttribLocation;
}

void Importer::ImportFile(const char *filename)
{
	if (_importer != nullptr)
		return;
	if (_scene != nullptr)
		_scene->Destroy(true);

	_importer = FbxImporter::Create(_sdkManager, "");

	// Use the first argument as the filename for the importer.
	if (!_importer->Initialize(filename, -1, _sdkManager->GetIOSettings()))
	{
		throw new std::exception("Failed to Import");
	}

	// Create a new scene so that it can be populated by the imported file.
	_scene = FbxScene::Create(_sdkManager, "myScene");

	// Import the contents of the file into the scene.
	_importer->Import(_scene);

	// The file is imported, so get rid of the importer.
	_importer->Destroy();
	_importer = nullptr;
}

void Importer::TraverseScene(FbxNode *node, function<void(FbxMesh *)> callback)
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

Vector3 Importer::ReadNormal(FbxMesh* inMesh, int inCtrlPointIndex, int inVertexCounter)
{
	Vector3 outNormal;
	if (inMesh->GetElementNormalCount() < 1)
	{
		throw std::exception("Invalid Normal Number");
	}

	FbxGeometryElementNormal* vertexNormal = inMesh->GetElementNormal(0);
	switch (vertexNormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(inCtrlPointIndex);
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;

		default:
			throw std::exception("Invalid Reference");
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(inVertexCounter);
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;

		default:
			throw std::exception("Invalid Reference");
		}
		break;
	}
	return outNormal;
}

void Importer::PrintNode(FbxNode* pNode)
{
	PrintTabs();
	const char* nodeName = pNode->GetName();
	FbxDouble3 translation = pNode->LclTranslation.Get();
	FbxDouble3 rotation = pNode->LclRotation.Get();
	FbxDouble3 scaling = pNode->LclScaling.Get();

	// Print the contents of the node.
	DebugLog(L"<node name='%S' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
		CH_TO_WCH(nodeName),
		translation[0], translation[1], translation[2],
		rotation[0], rotation[1], rotation[2],
		scaling[0], scaling[1], scaling[2]
	);
	_numTabs++;

	// Print the node's attributes.
	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
		PrintAttribute(pNode->GetNodeAttributeByIndex(i));

	// Recursively print the children.
	for (int j = 0; j < pNode->GetChildCount(); j++)
		PrintNode(pNode->GetChild(j));

	_numTabs--;
	PrintTabs();
	DebugLog(L"</node>\n");
}

/**
* Print the required number of tabs.
*/
void Importer::PrintTabs()
{
	for (int i = 0; i < _numTabs; i++)
		DebugLog(L"\t");
}

/**
* Return a string-based representation based on the attribute type.
*/
FbxString Importer::GetAttributeTypeName(FbxNodeAttribute::EType type)
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

void Importer::PrintAttribute(FbxNodeAttribute* pAttribute)
{
	if (!pAttribute) return;

	FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
	FbxString attrName = pAttribute->GetName();
	PrintTabs();
	// Note: to retrieve the character array of a FbxString, use its Buffer() method.
	DebugLog(L"<attribute type='%S' name='%S'/>\n", CH_TO_WCH(typeName.Buffer()), CH_TO_WCH(attrName.Buffer()));
}

void Importer::DisplayMaterial(fbxsdk::FbxGeometry* pGeometry)
{
	int lMaterialCount = 0;
	FbxNode* lNode = NULL;
	if (pGeometry) {
		lNode = pGeometry->GetNode();
		if (lNode)
			lMaterialCount = lNode->GetMaterialCount();
	}

	if (lMaterialCount > 0)
	{
		FbxPropertyT<FbxDouble3> lKFbxDouble3;
		FbxPropertyT<FbxDouble> lKFbxDouble1;
		FbxColor theColor;

		for (int lCount = 0; lCount < lMaterialCount; lCount++)
		{
			DebugLog(L"Material Count %d", lCount);

			FbxSurfaceMaterial *lMaterial = lNode->GetMaterial(lCount);

			DebugLog(L"Name: %S", CH_TO_WCH((char *)lMaterial->GetName()));

			//Get the implementation to see if it's a hardware shader.
			const FbxImplementation* lImplementation = GetImplementation(lMaterial, FBXSDK_IMPLEMENTATION_HLSL);
			FbxString lImplemenationType = "HLSL";
			if (!lImplementation)
			{
				lImplementation = GetImplementation(lMaterial, FBXSDK_IMPLEMENTATION_CGFX);
				lImplemenationType = "CGFX";
			}
			if (lImplementation)
			{
				//Now we have a hardware shader, let's read it
				DebugLog(L"Hardware Shader Type: %S\n", CH_TO_WCH(lImplemenationType.Buffer()));
				const FbxBindingTable* lRootTable = lImplementation->GetRootTable();
				FbxString lFileName = lRootTable->DescAbsoluteURL.Get();
				FbxString lTechniqueName = lRootTable->DescTAG.Get();


				const FbxBindingTable* lTable = lImplementation->GetRootTable();
				size_t lEntryNum = lTable->GetEntryCount();

				for (int i = 0; i <(int)lEntryNum; ++i)
				{
					const FbxBindingTableEntry& lEntry = lTable->GetEntry(i);
					const char* lEntrySrcType = lEntry.GetEntryType(true);
					FbxProperty lFbxProp;


					FbxString lTest = lEntry.GetSource();
					DebugLog(L"Entry: %S\n", CH_TO_WCH(lTest.Buffer()));


					if (strcmp(FbxPropertyEntryView::sEntryType, lEntrySrcType) == 0)
					{
						lFbxProp = lMaterial->FindPropertyHierarchical(lEntry.GetSource());
						if (!lFbxProp.IsValid())
						{
							lFbxProp = lMaterial->RootProperty.FindHierarchical(lEntry.GetSource());
						}
					}
					else if (strcmp(FbxConstantEntryView::sEntryType, lEntrySrcType) == 0)
					{
						lFbxProp = lImplementation->GetConstants().FindHierarchical(lEntry.GetSource());
					}
					if (lFbxProp.IsValid())
					{
						if (lFbxProp.GetSrcObjectCount<FbxTexture>() > 0)
						{
							//do what you want with the textures
							for (int j = 0; j<lFbxProp.GetSrcObjectCount<FbxFileTexture>(); ++j)
							{
								FbxFileTexture *lTex = lFbxProp.GetSrcObject<FbxFileTexture>(j);
								DebugLog(L"File Texture: %S\n", CH_TO_WCH(lTex->GetFileName()));
							}
							for (int j = 0; j<lFbxProp.GetSrcObjectCount<FbxLayeredTexture>(); ++j)
							{
								FbxLayeredTexture *lTex = lFbxProp.GetSrcObject<FbxLayeredTexture>(j);
								DebugLog(L"Layered Texture: %S\n", CH_TO_WCH(lTex->GetName()));
							}
							for (int j = 0; j<lFbxProp.GetSrcObjectCount<FbxProceduralTexture>(); ++j)
							{
								FbxProceduralTexture *lTex = lFbxProp.GetSrcObject<FbxProceduralTexture>(j);
								DebugLog(L"Procedural Texture: %Sn", CH_TO_WCH(lTex->GetName()));
							}
						}
						else
						{
							FbxDataType lFbxType = lFbxProp.GetPropertyDataType();
							FbxString blah = lFbxType.GetName();
							if (FbxBoolDT == lFbxType)
							{
								DebugLog(L"                Bool: %S", lFbxProp.Get<FbxBool>() ? L"YES" : L"NO");
							}
							else if (FbxIntDT == lFbxType || FbxEnumDT == lFbxType)
							{
								DebugLog(L"                Int: %d", lFbxProp.Get<FbxInt>());
							}
							else if (FbxFloatDT == lFbxType)
							{
								DebugLog(L"                Float: %f", lFbxProp.Get<FbxFloat>());

							}
							else if (FbxDoubleDT == lFbxType)
							{
								DebugLog(L"                Double: %f", lFbxProp.Get<FbxDouble>());
							}
							else if (FbxStringDT == lFbxType
								|| FbxUrlDT == lFbxType
								|| FbxXRefUrlDT == lFbxType)
							{
								DebugLog(L"                String: %S", CH_TO_WCH(lFbxProp.Get<FbxString>().Buffer()));
							}
							else if (FbxDouble2DT == lFbxType)
							{
								FbxDouble2 lDouble2 = lFbxProp.Get<FbxDouble2>();
								FbxVector2 lVect;
								lVect[0] = lDouble2[0];
								lVect[1] = lDouble2[1];

								DebugLog(L"                2D vector: %f %f", lVect[0], lVect[1]);
							}
							else if (FbxDouble3DT == lFbxType || FbxColor3DT == lFbxType)
							{
								FbxDouble3 lDouble3 = lFbxProp.Get<FbxDouble3>();
								FbxVector4 lVect;
								lVect[0] = lDouble3[0];
								lVect[1] = lDouble3[1];
								lVect[2] = lDouble3[2];
								DebugLog(L"                3D vector: %f %f %f", lVect[0], lVect[1], lVect[2]);
							}

							else if (FbxDouble4DT == lFbxType || FbxColor4DT == lFbxType)
							{
								FbxDouble4 lDouble4 = lFbxProp.Get<FbxDouble4>();
								FbxVector4 lVect;
								lVect[0] = lDouble4[0];
								lVect[1] = lDouble4[1];
								lVect[2] = lDouble4[2];
								lVect[3] = lDouble4[3];
								DebugLog(L"                4D vector: %f %f %f %f", lVect[0], lVect[1], lVect[2], lVect[3]);
							}
							else if (FbxDouble4x4DT == lFbxType)
							{
								FbxDouble4x4 lDouble44 = lFbxProp.Get<FbxDouble4x4>();
								for (int j = 0; j<4; ++j)
								{

									FbxVector4 lVect;
									lVect[0] = lDouble44[j][0];
									lVect[1] = lDouble44[j][1];
									lVect[2] = lDouble44[j][2];
									lVect[3] = lDouble44[j][3];
									DebugLog(L"                4x4D vector: %f %f %f %f", lVect[0], lVect[1], lVect[2], lVect[3]);
								}

							}
						}

					}
				}
			}
			else if (lMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
			{
				// We found a Phong material.  Display its properties.

				// Display the Ambient Color
				lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Ambient;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Ambient: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				// Display the Diffuse Color
				lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Diffuse;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Diffuse: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				// Display the Specular Color (unique to Phong materials)
				lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Specular;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Specular: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				// Display the Emissive Color
				lKFbxDouble3 = ((FbxSurfacePhong *)lMaterial)->Emissive;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Emissive: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				//Opacity is Transparency factor now
				lKFbxDouble1 = ((FbxSurfacePhong *)lMaterial)->TransparencyFactor;
				DebugLog(L"            Opacity: %f", 1.0 - lKFbxDouble1.Get());

				// Display the Shininess
				lKFbxDouble1 = ((FbxSurfacePhong *)lMaterial)->Shininess;
				DebugLog(L"            Shininess: %f", lKFbxDouble1.Get());

				// Display the Reflectivity
				lKFbxDouble1 = ((FbxSurfacePhong *)lMaterial)->ReflectionFactor;
				DebugLog(L"            Reflectivity: %f", lKFbxDouble1.Get());
			}
			else if (lMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId))
			{
				// We found a Lambert material. Display its properties.
				// Display the Ambient Color
				lKFbxDouble3 = ((FbxSurfaceLambert *)lMaterial)->Ambient;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Ambient: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				// Display the Diffuse Color
				lKFbxDouble3 = ((FbxSurfaceLambert *)lMaterial)->Diffuse;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Diffuse: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				// Display the Emissive
				lKFbxDouble3 = ((FbxSurfaceLambert *)lMaterial)->Emissive;
				theColor.Set(lKFbxDouble3.Get()[0], lKFbxDouble3.Get()[1], lKFbxDouble3.Get()[2]);
				DebugLog(L"            Emissive: {%f %f %f %f}", theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

				// Display the Opacity
				lKFbxDouble1 = ((FbxSurfaceLambert *)lMaterial)->TransparencyFactor;
				DebugLog(L"            Opacity: %f", 1.0 - lKFbxDouble1.Get());
			}
			else
				DebugLog(L"Unknown type of Material");

			FbxPropertyT<FbxString> lString;
			lString = lMaterial->ShadingModel;
			char *intext = lString.Get().Buffer();
			wchar_t *conv = const_cast<wchar_t *>(CH_TO_WCH(intext));
			DebugLog(L"            Shading Model: %S", conv);
			DebugLog(L"");
		}
	}
}
