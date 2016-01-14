#include <fbxsdk.h>
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <stack>
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;
using namespace DirectX;

struct Subset
{
	Subset() : MtlIndex(0), VertexBase(0), IndexStart(0), IndexCount(0)
	{}

	int MtlIndex;
	int VertexBase;
	int IndexStart;
	int IndexCount;
};

struct Vertex
{
	Vertex() : Position(0, 0, 0), Normal(0, 0, 0), Tangent(0, 0, 0), TexUV(0, 0)
	{}

	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT3 Tangent;
	XMFLOAT2 TexUV;
};

enum class EffectType
{
	NoTexture,
	Texture,
	Normal
};

struct Material
{
	Material() : Name("Default"), Ambient(0.2f, 0.2f, 0.2f), Diffuse(0.5f, 0.5f, 0.5f), Specular(0.0f, 0.0f, 0.0f),
		SpecPower(0.0f), Reflectivity(0.0f, 0.0f, 0.0f),
		Effect(EffectType::NoTexture), DiffuseMap("Null"), NormalMap("Null")
	{}

	string Name;
	XMFLOAT3 Ambient;
	XMFLOAT3 Diffuse;
	XMFLOAT3 Specular;
	float SpecPower;
	XMFLOAT3 Reflectivity;
	EffectType Effect;
	string DiffuseMap;
	string NormalMap;
};

struct MeshVI
{
	vector<Vertex> Vertices;
	vector<int> Indices;
	vector<Subset> Subsets;
};

vector<Material> Materials;
vector<MeshVI> VICache;
vector<Subset> Subsets;
vector<Vertex> Vertices;
vector<int> Indices;

ostringstream WarningStream;
int MeshIndex = -1;

XMFLOAT4X4 World;
XMFLOAT4X4 WorldInvTranspose;

#pragma region Help functions
FbxAMatrix GetNodeGeometryTransform(FbxNode* node)
{
	FbxAMatrix matrixGeo;
	matrixGeo.SetIdentity();

	if (node->GetNodeAttribute())
	{
		const FbxVector4 lT = node->GetGeometricTranslation(FbxNode::eSourcePivot);
		const FbxVector4 lR = node->GetGeometricRotation(FbxNode::eSourcePivot);
		const FbxVector4 lS = node->GetGeometricScaling(FbxNode::eSourcePivot);

		matrixGeo.SetT(lT);
		matrixGeo.SetR(lR);
		matrixGeo.SetS(lS);
	}

	return matrixGeo;
}
FbxAMatrix GetNodeWorldTransform(FbxNode* node)
{
	FbxAMatrix matrixL2W;
	matrixL2W.SetIdentity();
	if (node == nullptr)
		return matrixL2W;

	matrixL2W = node->EvaluateGlobalTransform();
	FbxAMatrix matrixGeo = GetNodeGeometryTransform(node);
	matrixL2W *= matrixGeo;

	return matrixL2W;
}
XMMATRIX InverseTranspose(CXMMATRIX M)
{
	// Inverse-transpose is just applied to normals.  So zero out 
	// translation row so that it doesn't get into our inverse-transpose
	// calculation--we don't want the inverse-transpose of the translation.
	XMMATRIX A = M;
	A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	XMVECTOR det = XMMatrixDeterminant(A);
	return XMMatrixTranspose(XMMatrixInverse(&det, A));
}
XMFLOAT4X4 ConvertToXMFloat4X4(const FbxAMatrix& tf)
{
	auto tranforms = XMFLOAT4X4(
		(float)tf[0][0], (float)tf[0][1], (float)tf[0][2], (float)tf[0][3],
		(float)tf[1][0], (float)tf[1][1], (float)tf[1][2], (float)tf[1][3],
		(float)tf[2][0], (float)tf[2][1], (float)tf[2][2], (float)tf[2][3],
		(float)tf[3][0], (float)tf[3][1], (float)tf[3][2], (float)tf[3][3]);
	return tranforms;
}
#pragma endregion

#pragma region Declaration
void ProcessNode(FbxNode* node);
void ProcessMesh(FbxNode* node);
void ConnectMaterial(FbxMesh* mesh, int triangleCount, MeshVI& currentVI);
void LoadMaterialAttribute(FbxSurfaceMaterial* surfaceMtl, Material& mtlCache);
void LoadMaterialTexture(FbxSurfaceMaterial* surfaceMtl, Material& mtlCache, int limit = 1);
void ReadPosition(FbxMesh* mesh, vector<Vertex>& verticesCache);
void ReadIndex(FbxMesh* mesh, vector<int>& indicesCache);
void ReadNormal(FbxMesh* mesh, vector<Vertex>& verticesCache);
void ReadTangent(FbxMesh* mesh, vector<Vertex>& verticesCache, bool reGenerate = true);
void ReadUV(FbxMesh* mesh, vector<Vertex>& verticesCache);
void PackVI();
void WriteX3DText();
void WriteX3DBinary();
#pragma endregion

int main()
{
	// Change the following filename to a suitable filename value.
	const char* filename = "Talia.fbx";

	// Initialize the SDK manager. This object handles all our memory management.
	FbxManager* sdkManager = FbxManager::Create();

	// Create the IO settings object.
	FbxIOSettings *ios = FbxIOSettings::Create(sdkManager, IOSROOT);
	sdkManager->SetIOSettings(ios);

	// Create an importer using the SDK manager.
	FbxImporter* importer = FbxImporter::Create(sdkManager, "");

	// Use the first argument as the filename for the importer.
	if (!importer->Initialize(filename, -1, sdkManager->GetIOSettings()))
	{
		cout << "Call to FbxImporter::Initialize() failed." << endl;
		cout << "Error returned: " << importer->GetStatus().GetErrorString() << endl << endl;
		exit(-1);
	}

	// Create a new scene so that it can be populated by the imported file.
	FbxScene* scene = FbxScene::Create(sdkManager, "myScene");

	// Import the contents of the file into the scene.
	importer->Import(scene);

	// The file is imported; so get rid of the importer.
	importer->Destroy();

	// Convert mesh, NURBS and patch into triangle mesh
	FbxGeometryConverter geomConverter(sdkManager);
	geomConverter.Triangulate(scene, /*replace*/true);
	// Split meshes per material, so that we only have one material per mesh.
	// Sometimes the split mesh process may fail due to the FBX SDK issues.
	// You can use the "LoadStaticModel_old.cpp" to finish the work.
	geomConverter.SplitMeshesPerMaterial(scene, /*replace*/true);

	cout << "Read data ..." << endl;

	// Print the nodes of the scene and their attributes recursively.
	// Note that we are not printing the root node because it should
	// not contain any attributes.
	FbxNode* rootNode = scene->GetRootNode();
	if (rootNode)
	{
		for (int i = 0; i < rootNode->GetChildCount(); i++)
			ProcessNode(rootNode->GetChild(i));
	}
	cout << endl << "Warning Information:" << endl;
	cout << WarningStream.str() << endl;

	// Destroy the SDK manager and all the other objects it was handling.
	sdkManager->Destroy();

	PackVI();

	cout << "Write data ..." << endl;
	WriteX3DText();
	WriteX3DBinary();

	return 0;
}

void ProcessNode(FbxNode* node)
{
	if (node->GetNodeAttribute())
	{
		switch (node->GetNodeAttribute()->GetAttributeType())
		{
		case FbxNodeAttribute::eMesh:
			ProcessMesh(node);
			break;
		}
	}

	for (int i = 0; i < node->GetChildCount(); ++i)
	{
		ProcessNode(node->GetChild(i));
	}
}

void ProcessMesh(FbxNode* node)
{
	FbxMesh* mesh = node->GetMesh();
	if (mesh == nullptr)
		return;

	int controlPointsCount = mesh->GetControlPointsCount();
	int triangleCount = mesh->GetPolygonCount();
	if (triangleCount == 0 || controlPointsCount == 0)
		return;

	++MeshIndex;
	cout << "***************Process mesh " << MeshIndex << "***************" << endl;

	XMMATRIX worldXM, worldInvTransposeXM;
	auto tranforms = ConvertToXMFloat4X4(GetNodeWorldTransform(node));
	worldXM = XMLoadFloat4x4(&tranforms);
	worldInvTransposeXM = InverseTranspose(worldXM);
	XMStoreFloat4x4(&World, worldXM);
	XMStoreFloat4x4(&WorldInvTranspose, worldInvTransposeXM);

	// Load material
	MeshVI currentVI;
	auto& verticesCache = currentVI.Vertices;
	auto& indicesCache = currentVI.Indices;
	verticesCache.resize(controlPointsCount);
	indicesCache.reserve(triangleCount * 3);
	
	// Read normals, tangents, UVs and indices
	ConnectMaterial(mesh, triangleCount, currentVI);
	ReadIndex(mesh, indicesCache);
	ReadPosition(mesh, verticesCache);
	ReadNormal(mesh, verticesCache);
	ReadTangent(mesh, verticesCache, false);
	ReadUV(mesh, verticesCache);

	VICache.push_back(currentVI);
}

// Connect the current mesh to according material
void ConnectMaterial(FbxMesh* mesh, int triangleCount, MeshVI& currentVI)
{
	// Get the material index list of current mesh
	auto elementMaterial = mesh->GetElementMaterial();
	if (!elementMaterial)
	{
		Subset subset;
		subset.MtlIndex = -1;
		subset.VertexBase = 0;
		subset.IndexStart = 0;
		subset.IndexCount = triangleCount * 3;
		currentVI.Subsets.push_back(subset);
		return;
	}

	auto materialMappingMode = elementMaterial->GetMappingMode();
	auto materialIndices = &elementMaterial->GetIndexArray();
	if (materialIndices->GetCount() == 0)
	{
		Subset subset;
		subset.MtlIndex = -1;
		subset.VertexBase = 0;
		subset.IndexStart = 0;
		subset.IndexCount = triangleCount * 3;
		currentVI.Subsets.push_back(subset);
		return;
	}

	// Connect to local material index
	int maxIndex = 0;
	if (materialMappingMode == FbxGeometryElement::eAllSame)
	{
		Subset subset;
		subset.MtlIndex = 0;
		subset.VertexBase = 0;
		subset.IndexStart = 0;
		subset.IndexCount = triangleCount * 3;
		currentVI.Subsets.push_back(subset);
	}
	else if (materialMappingMode == FbxGeometryElement::eByPolygon)
	{
		// Split the current mesh
		int currentMtlIndex = materialIndices->GetAt(0);
		int count = 0;
		int offset = 0;
		maxIndex = currentMtlIndex;

		for (int i = 0; i < materialIndices->GetCount(); ++i)
		{
			if (materialIndices->GetAt(i) != currentMtlIndex)
			{
				Subset subSet;
				subSet.MtlIndex = currentMtlIndex;
				subSet.VertexBase = 0;
				subSet.IndexStart = offset * 3;
				subSet.IndexCount = count * 3;
				currentVI.Subsets.push_back(subSet);
				currentMtlIndex = materialIndices->GetAt(i);
				offset += count;
				count = 0;
				if (currentMtlIndex > maxIndex)
					maxIndex = currentMtlIndex;
			}
			++count;
		}
		if (count != 0)
		{
			Subset subSet;
			subSet.MtlIndex = currentMtlIndex;
			subSet.VertexBase = 0;
			subSet.IndexStart = offset * 3;
			subSet.IndexCount = count * 3;
			currentVI.Subsets.push_back(subSet);
		}
	}
	else
		throw new exception("No such material mapping mode!");

	// Update local material index to global material index
	FbxNode* node = mesh->GetNode();
	int mtlCount = node->GetMaterialCount();
	if (maxIndex > mtlCount)
		throw new exception("Lack material in this mesh!");

	if (mtlCount == 0)
	{
		currentVI.Subsets.clear();
		Subset subset;
		subset.MtlIndex = -1;
		subset.VertexBase = 0;
		subset.IndexStart = 0;
		subset.IndexCount = triangleCount * 3;
		currentVI.Subsets.push_back(subset);
		return;
	}
	if (mtlCount > 1)
	{
		WarningStream << "More than one material in mesh " << MeshIndex << "!" << endl;
	}

	for (int i = 0; i < mtlCount; ++i)
	{
		FbxSurfaceMaterial* surfaceMtl = node->GetMaterial(i);
		string mtlName = surfaceMtl->GetName();
		int globalIndex = -1;

		for (size_t j = 0; j < Materials.size(); ++j)
			if (Materials[j].Name == mtlName)
			{
				globalIndex = j;
				break;
			}

		if (globalIndex == -1)
		{
			Material currentMtl;
			currentMtl.Name = mtlName;
			LoadMaterialAttribute(surfaceMtl, currentMtl);
			Materials.push_back(currentMtl);
			globalIndex = Materials.size() - 1;
		}

		for (size_t j = 0; j < currentVI.Subsets.size(); ++j)
			if (currentVI.Subsets[j].MtlIndex == i)
				currentVI.Subsets[j].MtlIndex = globalIndex;
	}
}

// Load material information
void LoadMaterialAttribute(FbxSurfaceMaterial* surfaceMtl, Material& mtlCache)
{
	FbxDouble3 temp;
	float factor;
	// Phong material
	if (surfaceMtl->GetClassId().Is(FbxSurfacePhong::ClassId))
	{
		FbxSurfacePhong* phong = (FbxSurfacePhong*)surfaceMtl;
		// Ambient Color
		temp = phong->Ambient.Get();
		factor = (float)phong->AmbientFactor.Get();
		mtlCache.Ambient = XMFLOAT3((float)temp[0] * factor, (float)temp[1] * factor, (float)temp[2] * factor);
		// Diffuse Color
		temp = phong->Diffuse.Get();
		factor = (float)phong->DiffuseFactor.Get();
		mtlCache.Diffuse = XMFLOAT3((float)temp[0] * factor, (float)temp[1] * factor, (float)temp[2] * factor);
		// Specular Color
		temp = phong->Specular.Get();
		factor = (float)phong->SpecularFactor.Get();
		mtlCache.Specular = XMFLOAT3((float)temp[0] * factor, (float)temp[1] * factor, (float)temp[2] * factor);
		// Shininess
		mtlCache.SpecPower = (float)phong->Shininess.Get();
		// Reflectivity
		temp = phong->ReflectionFactor.Get();
		factor = (float)phong->ReflectionFactor.Get();
		mtlCache.Reflectivity = XMFLOAT3((float)temp[0] * factor, (float)temp[1] * factor, (float)temp[2] * factor);
	}
	// Lambert material
	else if (surfaceMtl->GetClassId().Is(FbxSurfaceLambert::ClassId))
	{
		FbxSurfaceLambert* lambert = (FbxSurfaceLambert*)surfaceMtl;
		// Ambient Color
		temp = lambert->Ambient.Get();
		factor = (float)lambert->AmbientFactor.Get();
		mtlCache.Ambient = XMFLOAT3((float)temp[0] * factor, (float)temp[1] * factor, (float)temp[2] * factor);
		// Diffuse Color
		temp = lambert->Diffuse.Get();
		factor = (float)lambert->DiffuseFactor.Get();
		mtlCache.Diffuse = XMFLOAT3((float)temp[0] * factor, (float)temp[1] * factor, (float)temp[2] * factor);
	}
	else
	{
		WarningStream << "No support material type in mesh " << MeshIndex << "!" << endl;
		return;
	}
	LoadMaterialTexture(surfaceMtl, mtlCache, 2);
}

// Get texture file name (texture and normal texture)
void LoadMaterialTexture(FbxSurfaceMaterial* surfaceMtl, Material& mtlCache, int limit)
{
	int count = 0;

	for (int textureLayerIndex = 0; textureLayerIndex < FbxLayerElement::sTypeTextureCount; ++textureLayerIndex)
	{
		FbxProperty fbxProperty = surfaceMtl->FindProperty(FbxLayerElement::sTextureChannelNames[textureLayerIndex]);
		if (!fbxProperty.IsValid())
			continue;

		if (count >= limit)
		{
			WarningStream << "Texture count of material overflows in mesh " << MeshIndex << "!" << endl;
			break;
		}

		int textureCount = fbxProperty.GetSrcObjectCount<FbxTexture>();
		if (textureCount == 0)
			continue;

		for (int j = 0; j < textureCount; ++j)
		{
			if (count >= limit)
				break;

			FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(fbxProperty.GetSrcObject<FbxTexture>(j));
			if (fileTexture)
			{
				// Use pTexture to load the attribute of current texture...
				//auto filename = pTexture->GetFileName();
				auto name = string(fileTexture->GetRelativeFileName());
				auto index = name.find_last_of('\\');
				name = name.substr(index + 1);
				index = name.find_last_of('.');
				auto textureFormat = name.substr(index + 1);
				if (textureFormat == "tga" || textureFormat == "gif")
				{
					name.resize(name.size() - 3);
					name += "jpg";
				}

				if (count == 0)
					mtlCache.DiffuseMap = name;
				else
				{
					if (name == mtlCache.DiffuseMap)
						continue;
					mtlCache.NormalMap = name;
				}
				++count;
			}
		}
	}
	if (count == 0)
		mtlCache.Effect = EffectType::NoTexture;
	else if (count == 1)
		mtlCache.Effect = EffectType::Texture;
	else
		mtlCache.Effect = EffectType::Normal;
}

// Read the each control point's position
void ReadPosition(FbxMesh* mesh, vector<Vertex>& verticesCache)
{
	// Read control points
	FbxVector4* pCtrlPoint = mesh->GetControlPoints();
	XMVECTOR tempVec;
	int controlPointsCount = mesh->GetControlPointsCount();
	// Read positions
	for (int i = 0; i < controlPointsCount; ++i)
	{
		verticesCache[i].Position.x = (float)pCtrlPoint[i][0];
		verticesCache[i].Position.y = (float)pCtrlPoint[i][1];
		verticesCache[i].Position.z = (float)pCtrlPoint[i][2];
		tempVec = XMLoadFloat3(&verticesCache[i].Position);
		XMStoreFloat3(&verticesCache[i].Position, XMVector3TransformCoord(tempVec, XMLoadFloat4x4(&World)));
	}
}

// Read the index array
void ReadIndex(FbxMesh* mesh, vector<int>& indicesCache)
{
	int triangleCount = mesh->GetPolygonCount();
	for (int i = 0; i < triangleCount; ++i)
		for (int j = 0; j < 3; j++)
		{
			int ctrlPointIndex = mesh->GetPolygonVertex(i, j);
			indicesCache.push_back(ctrlPointIndex);
		}
}

// Only one normal data is stored for each control point
void ReadNormal(FbxMesh* mesh, vector<Vertex>& verticesCache)
{
	if (mesh->GetElementNormalCount() < 1)
	{
		WarningStream << "Lack Normal in mesh " << MeshIndex << "!" << endl;
		if (!mesh->GenerateNormals())
		{
			WarningStream << "Regenerate normal failed!" << endl;
			return;
		}
	}

	FbxGeometryElementNormal* leNormal = mesh->GetElementNormal(0);
	int controlPointsCount = mesh->GetControlPointsCount();
	int triangleCount = mesh->GetPolygonCount();
	XMVECTOR tempVec;
	int vertexCounter = 0;
	XMMATRIX worldInvTransposeXM = XMLoadFloat4x4(&WorldInvTranspose);

	switch (leNormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (leNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
			for (int i = 0; i < controlPointsCount; ++i)
			{
				verticesCache[i].Normal.x = (float)leNormal->GetDirectArray()[i][0];
				verticesCache[i].Normal.y = (float)leNormal->GetDirectArray()[i][1];
				verticesCache[i].Normal.z = (float)leNormal->GetDirectArray()[i][2];
				tempVec = XMLoadFloat3(&verticesCache[i].Normal);
				XMStoreFloat3(&verticesCache[i].Normal, XMVector3TransformCoord(tempVec, worldInvTransposeXM));
			}
		break;
		case FbxGeometryElement::eIndexToDirect:
			for (int i = 0; i < controlPointsCount; ++i)
			{
				int id = leNormal->GetIndexArray()[i];
				verticesCache[i].Normal.x = (float)leNormal->GetDirectArray()[id][0];
				verticesCache[i].Normal.y = (float)leNormal->GetDirectArray()[id][1];
				verticesCache[i].Normal.z = (float)leNormal->GetDirectArray()[id][2];
				tempVec = XMLoadFloat3(&verticesCache[i].Normal);
				XMStoreFloat3(&verticesCache[i].Normal, XMVector3TransformCoord(tempVec, worldInvTransposeXM));
		}
		break;
		default:
			throw new exception("No such normal reference mode!");
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (leNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
			for (int i = 0; i < triangleCount; ++i)
				for (int j = 0; j < 3; j++)
				{
					if (i == 45608)
						int a = 0;

					int ctrlPointIndex = mesh->GetPolygonVertex(i, j);

					verticesCache[ctrlPointIndex].Normal.x = (float)leNormal->GetDirectArray()[vertexCounter][0];
					verticesCache[ctrlPointIndex].Normal.y = (float)leNormal->GetDirectArray()[vertexCounter][1];
					verticesCache[ctrlPointIndex].Normal.z = (float)leNormal->GetDirectArray()[vertexCounter][2];
					tempVec = XMLoadFloat3(&verticesCache[ctrlPointIndex].Normal);
					XMStoreFloat3(&verticesCache[ctrlPointIndex].Normal, XMVector3TransformCoord(tempVec, worldInvTransposeXM));
					++vertexCounter;
				}
		break;
		case FbxGeometryElement::eIndexToDirect:
			for (int i = 0; i < triangleCount; ++i)
				for (int j = 0; j < 3; j++)
				{
					int ctrlPointIndex = mesh->GetPolygonVertex(i, j);
					int id = leNormal->GetIndexArray()[vertexCounter];
					verticesCache[ctrlPointIndex].Normal.x = (float)leNormal->GetDirectArray()[id][0];
					verticesCache[ctrlPointIndex].Normal.y = (float)leNormal->GetDirectArray()[id][1];
					verticesCache[ctrlPointIndex].Normal.z = (float)leNormal->GetDirectArray()[id][2];
					tempVec = XMLoadFloat3(&verticesCache[ctrlPointIndex].Normal);
					XMStoreFloat3(&verticesCache[ctrlPointIndex].Normal, XMVector3TransformCoord(tempVec, worldInvTransposeXM));
					++vertexCounter;
				}
		break;
		default:
			throw new exception("No such normal reference mode!");
		}
	}
	break;

	default:
		throw new exception("No such normal mapping mode!");
	}

}

// Only one tangent data is stored for each control point
void ReadTangent(FbxMesh* mesh, vector<Vertex>& verticesCache, bool reGenerate)
{
	if (mesh->GetElementTangentCount() < 1)
	{
		WarningStream << "Lack Tangent in mesh " << MeshIndex << "!" << endl;
		if (!reGenerate)
			return;
		if (!mesh->GenerateTangentsDataForAllUVSets())
		{
			WarningStream << "Regenerate tangent failed!" << endl;
			return;
		}
	}

	FbxGeometryElementTangent* leTangent = mesh->GetElementTangent(0);
	int controlPointsCount = mesh->GetControlPointsCount();
	int triangleCount = mesh->GetPolygonCount();
	XMVECTOR tempVec;
	int vertexCounter = 0;
	XMMATRIX worldXM = XMLoadFloat4x4(&World);

	switch (leTangent->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (leTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
			for (int i = 0; i < controlPointsCount; ++i)
			{
				verticesCache[i].Tangent.x = (float)leTangent->GetDirectArray()[i][0];
				verticesCache[i].Tangent.y = (float)leTangent->GetDirectArray()[i][1];
				verticesCache[i].Tangent.z = (float)leTangent->GetDirectArray()[i][2];
				tempVec = XMLoadFloat3(&verticesCache[i].Tangent);
				XMStoreFloat3(&verticesCache[i].Tangent, XMVector3TransformCoord(tempVec, worldXM));
			}
		break;
		case FbxGeometryElement::eIndexToDirect:
			for (int i = 0; i < controlPointsCount; ++i)
			{
				int id = leTangent->GetIndexArray()[i];
				verticesCache[i].Tangent.x = (float)leTangent->GetDirectArray()[id][0];
				verticesCache[i].Tangent.y = (float)leTangent->GetDirectArray()[id][1];
				verticesCache[i].Tangent.z = (float)leTangent->GetDirectArray()[id][2];
				tempVec = XMLoadFloat3(&verticesCache[i].Tangent);
				XMStoreFloat3(&verticesCache[i].Tangent, XMVector3TransformCoord(tempVec, worldXM));
			}
		break;
		default:
			throw new exception("No such tangent reference mode!");
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (leTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
			for (int i = 0; i < triangleCount; ++i)
				for (int j = 0; j < 3; j++)
				{
					int ctrlPointIndex = mesh->GetPolygonVertex(i, j);
					verticesCache[ctrlPointIndex].Tangent.x = (float)leTangent->GetDirectArray()[vertexCounter][0];
					verticesCache[ctrlPointIndex].Tangent.y = (float)leTangent->GetDirectArray()[vertexCounter][1];
					verticesCache[ctrlPointIndex].Tangent.z = (float)leTangent->GetDirectArray()[vertexCounter][2];
					tempVec = XMLoadFloat3(&verticesCache[ctrlPointIndex].Tangent);
					XMStoreFloat3(&verticesCache[ctrlPointIndex].Tangent, XMVector3TransformCoord(tempVec, worldXM));
					++vertexCounter;
				}
		break;
		case FbxGeometryElement::eIndexToDirect:
			for (int i = 0; i < triangleCount; ++i)
				for (int j = 0; j < 3; j++)
				{
					int ctrlPointIndex = mesh->GetPolygonVertex(i, j);
					int id = leTangent->GetIndexArray()[vertexCounter];
					verticesCache[ctrlPointIndex].Tangent.x = (float)leTangent->GetDirectArray()[id][0];
					verticesCache[ctrlPointIndex].Tangent.y = (float)leTangent->GetDirectArray()[id][1];
					verticesCache[ctrlPointIndex].Tangent.z = (float)leTangent->GetDirectArray()[id][2];
					tempVec = XMLoadFloat3(&verticesCache[ctrlPointIndex].Tangent);
					XMStoreFloat3(&verticesCache[ctrlPointIndex].Tangent, XMVector3TransformCoord(tempVec, worldXM));
					++vertexCounter;
				}
		break;
		default:
			throw new exception("No such tangent reference mode!");
		}
	break;

	default:
		throw new exception("No such tangent mapping mode!");
	}

}

// Only store one uv data
void ReadUV(FbxMesh* mesh, vector<Vertex>& verticesCache)
{
	if (mesh->GetElementUVCount() < 1)
	{
		WarningStream << "Lack UV in mesh " << MeshIndex << "!" << endl;
		return;
	}

	FbxGeometryElementUV* pVertexUV = mesh->GetElementUV(0);
	int controlPointsCount = mesh->GetControlPointsCount();
	int triangleCount = mesh->GetPolygonCount();
	int vertexCounter = 0;

	switch (pVertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (pVertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
			for (int i = 0; i < controlPointsCount; ++i)
			{
				verticesCache[i].TexUV.x = (float)pVertexUV->GetDirectArray()[i][0];
				verticesCache[i].TexUV.y = (float)pVertexUV->GetDirectArray()[i][1];
			}
		break;
		case FbxGeometryElement::eIndexToDirect:
			for (int i = 0; i < controlPointsCount; ++i)
			{
				int id = pVertexUV->GetIndexArray()[i];
				verticesCache[i].TexUV.x = (float)pVertexUV->GetDirectArray()[id][0];
				verticesCache[i].TexUV.y = (float)pVertexUV->GetDirectArray()[id][1];
			}
		break;
		default:
			throw new exception("No such UV reference mode!");
		}
	break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (pVertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		case FbxGeometryElement::eIndexToDirect:
			for (int i = 0; i < triangleCount; ++i)
				for (int j = 0; j < 3; j++)
				{
					int ctrlPointIndex = mesh->GetPolygonVertex(i, j);
					int textureUVIndex = mesh->GetTextureUVIndex(i, j);
					verticesCache[ctrlPointIndex].TexUV.x = (float)pVertexUV->GetDirectArray()[textureUVIndex][0];
					verticesCache[ctrlPointIndex].TexUV.y = (float)pVertexUV->GetDirectArray()[textureUVIndex][1];
				}
		break;
		default:
			throw new exception("No such UV reference mode!");
		}
	break;

	default:
		throw new exception("No such UV mapping mode!");
	}
}

// Rearrange VB and IB
void PackVI()
{
	if (VICache.size() == 0)
		return;

	// Set default material
	bool flag = true;
	int defaultMtlIndex = 0;
	for (auto& item : VICache)
		for (auto& subset : item.Subsets)
			if (subset.MtlIndex == -1)
			{
				if (flag)
				{
					flag = false;
					Materials.push_back(Material());
					defaultMtlIndex = Materials.size() - 1;
				}
				subset.MtlIndex = defaultMtlIndex;
			}


	sort(VICache.begin(), VICache.end(), [](MeshVI& a, MeshVI&b) {return a.Subsets.size() < b.Subsets.size(); });
	auto iter = VICache.begin();
	int singleCount = 0;
	for (; iter < VICache.end(); ++iter, ++singleCount)
		if (iter->Subsets.size() != 1)
			break;
	sort(VICache.begin(), iter, [](MeshVI& a, MeshVI& b) {return a.Subsets[0].MtlIndex < b.Subsets[0].MtlIndex; });

	int vertexBase = 0;
	int indexOffset = 0;
	int indexCount = 0;
	int offset = 0;
	int currentMtl = VICache[0].Subsets[0].MtlIndex;

	// Process single subset MeshVI
	for (int i = 0; i < singleCount; ++i)
	{
		// Aggregate same material
		if (VICache[i].Subsets[0].MtlIndex == currentMtl)
		{
			for (auto& item : VICache[i].Indices)
				item += offset;
			Vertices.insert(Vertices.end(), VICache[i].Vertices.begin(), VICache[i].Vertices.end());
			Indices.insert(Indices.end(), VICache[i].Indices.begin(), VICache[i].Indices.end());
			offset += VICache[i].Vertices.size();
			indexCount += VICache[i].Indices.size();
		}
		else
		{
			// Create real subset
			Subset subset;
			subset.MtlIndex = currentMtl;
			subset.VertexBase = vertexBase;
			subset.IndexStart = indexOffset;
			subset.IndexCount = indexCount;
			Subsets.push_back(subset);

			vertexBase = Vertices.size();
			indexOffset = Indices.size();
			indexCount = 0;
			offset = 0;
			currentMtl = VICache[i].Subsets[0].MtlIndex;
			--i;
		}
	}
	Subset subset;
	subset.MtlIndex = currentMtl;
	subset.VertexBase = vertexBase;
	subset.IndexStart = indexOffset;
	subset.IndexCount = indexCount;
	Subsets.push_back(subset);

	// Process multi subset MeshVI
	for (int i = singleCount; i < (int)VICache.size(); ++i)
	{
		vertexBase = Vertices.size();
		indexOffset = Indices.size();
		Vertices.insert(Vertices.end(), VICache[i].Vertices.begin(), VICache[i].Vertices.end());
		Indices.insert(Indices.end(), VICache[i].Indices.begin(), VICache[i].Indices.end());
		for (auto& item : VICache[i].Subsets)
		{
			item.IndexStart += indexOffset;
			item.VertexBase = vertexBase;
			Subsets.push_back(item);
		}
	}
}

// ASCII output
void WriteX3DText()
{
	// Write to .x3d file
	ofstream fout("resText.x3d");
	fout << "***************x3d-File-Header***************" << endl;
	fout << "#Materials: " << Materials.size() << endl;
	fout << "#Subsets: " << Subsets.size() << endl;
	fout << "#Vertices: " << Vertices.size() << endl;
	fout << "#Indices: " << Indices.size() << endl;
	fout << endl;

	fout << "***************Materials*********************" << endl;
	for (auto& item : Materials)
	{
		fout << "Ambient: " << item.Ambient.x << " " << item.Ambient.y << " " << item.Ambient.z << endl;
		fout << "Diffuse: " << item.Diffuse.x << " " << item.Diffuse.y << " " << item.Diffuse.z << endl;
		fout << "Specular: " << item.Specular.x << " " << item.Specular.y << " " << item.Specular.z << endl;
		fout << "SpecPower: " << item.SpecPower << endl;
		fout << "Reflectivity: " << item.Reflectivity.x << " " << item.Reflectivity.y << " " << item.Reflectivity.z << endl;
		fout << "Effect: " << (int)item.Effect << endl;
		fout << "DiffuseMap: " << item.DiffuseMap << endl;
		fout << "NormalMap: " << item.NormalMap << endl;
		fout << endl;
	}
	fout << endl;

	fout << "***************SubsetTable*******************" << endl;
	for (auto& item : Subsets)
	{
		fout << "MtlIndex: " << item.MtlIndex << " ";
		fout << "VertexBase: " << item.VertexBase << " ";
		fout << "IndexStart: " << item.IndexStart << " ";
		fout << "IndexCount: " << item.IndexCount << " ";
		fout << endl;
	}
	fout << endl;

	fout << "***************Vertices**********************" << endl;
	for (size_t i = 0; i < Vertices.size(); ++i)
	{
		fout << "Position: " << Vertices[i].Position.x << " " << Vertices[i].Position.y << " " << Vertices[i].Position.z << endl;
		fout << "Normal: " << Vertices[i].Normal.x << " " << Vertices[i].Normal.y << " " << Vertices[i].Normal.z << endl;
		fout << "Tangent: " << Vertices[i].Tangent.x << " " << Vertices[i].Tangent.y << " " << Vertices[i].Tangent.z << endl;
		fout << "UV: " << Vertices[i].TexUV.x << " " << Vertices[i].TexUV.y << endl;
		fout << endl;
	}
	fout << endl;

	fout << "***************Triangles*********************" << endl;
	if (Indices.size() % 3 != 0)
		throw new exception("Lack indices data!");
	for (size_t i = 0; i < Indices.size(); i += 3)
	{
		fout << Indices[i] << " " << Indices[i + 1] << " " << Indices[i + 2] << endl;
	}
	fout << endl;
	fout.flush();
	fout.close();
}

// Binary output
void WriteX3DBinary()
{
	ofstream fout("resBinary.x3d", ios::binary);
	int intTemp;

	// File header
	intTemp = Materials.size();
	fout.write((char*)&intTemp, sizeof(int));
	intTemp = Subsets.size();
	fout.write((char*)&intTemp, sizeof(int));
	intTemp = Vertices.size();
	fout.write((char*)&intTemp, sizeof(int));
	intTemp = Indices.size();
	fout.write((char*)&intTemp, sizeof(int));

	// Material
	for (auto& item : Materials)
	{
		fout.write((char*)&item.Ambient, sizeof(XMFLOAT3));
		fout.write((char*)&item.Diffuse, sizeof(XMFLOAT3));
		fout.write((char*)&item.Specular, sizeof(XMFLOAT3));
		fout.write((char*)&item.SpecPower, sizeof(float));
		fout.write((char*)&item.Reflectivity, sizeof(XMFLOAT3));
		intTemp = (int)item.Effect;
		fout.write((char*)&intTemp, sizeof(int));
		intTemp = item.DiffuseMap.length();
		fout.write((char*)&intTemp, sizeof(int));
		fout.write(item.DiffuseMap.c_str(), intTemp);
		intTemp = item.NormalMap.length();
		fout.write((char*)&intTemp, sizeof(int));
		fout.write(item.NormalMap.c_str(), intTemp);
	}

	// SubSets
	for (auto& item : Subsets)
	{
		fout.write((char*)&item.MtlIndex, sizeof(int));
		fout.write((char*)&item.VertexBase, sizeof(int));
		fout.write((char*)&item.IndexStart, sizeof(int));
		fout.write((char*)&item.IndexCount, sizeof(int));
	}

	// Vertices
	for (auto& item : Vertices)
	{
		fout.write((char*)&item.Position, sizeof(XMFLOAT3));
		fout.write((char*)&item.Normal, sizeof(XMFLOAT3));
		fout.write((char*)&item.Tangent, sizeof(XMFLOAT3));
		fout.write((char*)&item.TexUV, sizeof(XMFLOAT2));
	}

	// Indices
	if (Indices.size() % 3 != 0)
		throw new exception("Lack indices data!");
	for (auto& item : Indices)
	{
		fout.write((char*)&item, sizeof(int));
	}
	fout.flush();
	fout.close();
}
