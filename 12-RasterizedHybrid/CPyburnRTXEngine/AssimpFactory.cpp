#include "pchlib.h"
#include "AssimpFactory.h"

#include "AssimpAnimations.h"

namespace CPyburnRTXEngine
{
	std::unordered_map<UINT, AssimpFactory::Model> AssimpFactory::Models;

	void AssimpFactory::DoMeshTransforms(aiNode* node, XMMATRIX parentTransform)
	{
		XMMATRIX nodeTransform = XMMatrixIdentity();
		XMMATRIX nodeT = XMMatrixSet(node->mTransformation.a1, node->mTransformation.a2, node->mTransformation.a3, node->mTransformation.a4, node->mTransformation.b1, node->mTransformation.b2, node->mTransformation.b3, node->mTransformation.b4, node->mTransformation.c1, node->mTransformation.c2, node->mTransformation.c3, node->mTransformation.c4, node->mTransformation.d1, node->mTransformation.d2, node->mTransformation.d3, node->mTransformation.d4);

		// check LCL data, translation, prerotation, rotation (importing from Unity adds some of these that need to be ignored for the import to work, still figuring them all out with each model I import)
		std::string name(node->mName.data);
		std::string nameLower = name;
		stringToLower(nameLower);

		nodeTransform = parentTransform * nodeT;

		for (size_t i = 0; i < node->mNumMeshes; i++)
		{
			UINT meshIndex = node->mMeshes[i];
			//XMStoreFloat4x4(&m_meshEntries[meshIndex].meshTransform, nodeTransform);
			m_meshEntries[meshIndex].meshTransform = nodeTransform;
		}

		for (size_t i = 0; i < node->mNumChildren; i++)
		{
			DoMeshTransforms(node->mChildren[i], nodeTransform);
		}
	}

	void AssimpFactory::CreateSingleMeshEntry(UINT i, UINT& numVertices, UINT& numIndices, MeshEntry* mesh)
	{
		const aiMesh* paiMesh = m_pScene->mMeshes[i];

		//string sname(paiMesh->mName.data);
		//mesh.name = wstring(sname.begin(), sname.end());
		mesh->name = paiMesh->mName.data;
		stringToLower(mesh->name);

		mesh->numIndices = paiMesh->mNumFaces * 3;
		mesh->numVerts = paiMesh->mNumVertices;
		mesh->materialIndex = paiMesh->mMaterialIndex;

		mesh->baseVertex = numVertices;
		mesh->baseIndex = numIndices;

		numVertices += paiMesh->mNumVertices;
		numIndices += mesh->numIndices;

		if (paiMesh->mNumBones > 0)
		{
			mesh->hasBones = true;
			m_isSkinned = true;
		}

		InitializeMesh(i, mesh);
		InitializeMaterials();
	}

	void AssimpFactory::InitializeMesh(const UINT& i, MeshEntry* meshEntry)
	{
		const aiMesh* paiMesh = m_pScene->mMeshes[i];

		meshEntry->vertices.resize(paiMesh->mNumVertices);
		meshEntry->indices.resize(paiMesh->mNumFaces * 3);

		//size_t startingV = m_positions.size();
		//m_positions.resize(startingV + paiMesh->mNumVertices);

		std::vector<XMFLOAT3> perMeshPositions(paiMesh->mNumVertices);

		// Populate the vertex attribute vectors
		for (UINT i = 0; i < paiMesh->mNumVertices; i++)
		{
			const aiVector3D* pPos = &paiMesh->mVertices[i];
			const aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &paiMesh->mTextureCoords[0][i] : &aiVector3D(0.0f, 0.0f, 0.0f);
			const aiVector3D* pNormal = &paiMesh->mNormals[i];
			const aiVector3D* pTangent = &paiMesh->mTangents[i];
			//const aiVector3D* pBiTangent = &paiMesh->mBitangents[i];

			VSVertices vertice;
			vertice.position = XMFLOAT3(pPos->x, pPos->y, pPos->z);
			//perMeshPositions[i] = vertice.position;

			XMVECTOR xmPosition = XMLoadFloat3(&vertice.position);
			//XMMATRIX xmTransform = XMLoadFloat4x4(&meshEntry->meshTransform);
			XMMATRIX xmTransform = meshEntry->meshTransform;
			XMVECTOR xmTranPos = XMVector3TransformCoord(xmPosition, XMMatrixTranspose(xmTransform));
			//XMStoreFloat3(&m_positions[startingV + i], xmTranPos);
			XMStoreFloat3(&perMeshPositions[i], xmTranPos);

			vertice.texture = XMFLOAT2(pTexCoord->x, pTexCoord->y);
			vertice.normal = XMFLOAT3(pNormal->x, pNormal->y, pNormal->z);
			// hack: for missing data
			if (paiMesh->mTangents)
			{
				vertice.tangent = XMFLOAT3(pTangent->x, pTangent->y, pTangent->z);
			}
			else
				vertice.tangent = XMFLOAT3(pNormal->x, pNormal->y, pNormal->z);

			// this is now done by the RTX shader
			// hack: for missing data
			//if (paiMesh->mBitangents)
			//{
			//	vertice.binormal = XMFLOAT3(pBiTangent->x, pBiTangent->y, pBiTangent->z);
			//}
			//else
			//	vertice.binormal = XMFLOAT3(pNormal->x, pNormal->y, pNormal->z);

			meshEntry->vertices[i] = vertice;
		}

		BoundingBox::CreateFromPoints(meshEntry->boundingBox, perMeshPositions.size(), &perMeshPositions[0], sizeof(XMFLOAT3));
		BoundingSphere::CreateFromPoints(meshEntry->boundingSphere, perMeshPositions.size(), &perMeshPositions[0], sizeof(XMFLOAT3));

		if (meshEntry->hasBones || !m_isSkinned)
		{
			// todo: make this use main mesh
			if (i == 0)
			{
				BoundingBox::CreateFromPoints(m_boundingBox, perMeshPositions.size(), &perMeshPositions[0], sizeof(XMFLOAT3));
				BoundingSphere::CreateFromPoints(m_boundingSphere, perMeshPositions.size(), &perMeshPositions[0], sizeof(XMFLOAT3));
			}
			else
			{
				// merge all the mesh bounding shapes to get a total bounding shape
				BoundingBox::CreateMerged(m_boundingBox, m_boundingBox, meshEntry->boundingBox);
				BoundingSphere::CreateMerged(m_boundingSphere, m_boundingSphere, meshEntry->boundingSphere);
			}
		}

		// store the radius translation for knowing where the center of the model is in other calculations
		XMVECTOR radius = { 0, m_boundingSphere.Radius, 0 };
		XMMATRIX xmRadiusTranslation = XMMatrixTranslationFromVector(radius);
		//XMStoreFloat4x4(&m_boundingSphereRadiusTranslation, xmRadiusTranslation);
		m_boundingSphereRadiusTranslation = xmRadiusTranslation;

		perMeshPositions.clear();

		// Populate the index buffer
		size_t positionCounter = 0;
		for (size_t i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& face = paiMesh->mFaces[i];
			size_t indice = i * 3;
			meshEntry->indices[indice] = face.mIndices[0];

			indice++;
			meshEntry->indices[indice] = face.mIndices[1];

			indice++;
			meshEntry->indices[indice] = face.mIndices[2];
		}
	}

	void AssimpFactory::InitializeMaterials()
	{
		// NOTE: materials are hard to code for unless you work with the artist. I outlined several ways to check for materials.

		// Initialize the materials
		for (UINT i = 0; i < m_pScene->mNumMaterials; i++)
		{
			const aiMaterial* pMaterial = m_pScene->mMaterials[i];

			if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString aiPath;

				if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
				{
					m_textureDiffuse = FormatTexturePath(m_pathDirectory + (std::string)aiPath.data, aiTextureType_DIFFUSE);
					m_textureSPEC = FormatTexturePath(m_pathDirectory + (std::string)aiPath.data, aiTextureType_SPECULAR);
					m_textureNRM = FormatTexturePath(m_pathDirectory + (std::string)aiPath.data, aiTextureType_NORMALS);
					m_textureDISP = FormatTexturePath(m_pathDirectory + (std::string)aiPath.data, aiTextureType_DISPLACEMENT);
				}
			}
			else
			{
				m_textureDiffuse = "Assets\\test.tga";
			}
		}

		// load all materials a different way, decide to use if artist adds them this way
		std::vector<LoadedMaterial> materials = LoadAllMaterials(m_pScene);	

		// last of all check for built in textures, decide to use if artist adds them this way
		if (m_pScene->HasTextures())
		{
			for (unsigned int i = 0; i < m_pScene->mNumTextures; ++i)
			{
				aiTexture* tex = m_pScene->mTextures[i];

				DebugTrace(L"WARNING:Scene has built in textures and code is not currently handling them!");
				if (tex->mHeight == 0)
				{
					// Compressed texture (PNG/JPG in memory)
					// tex->pcData is the file data
					// tex->mWidth is data size
				}
				else
				{
					// Raw RGBA data
				}
			}
		}
	}

	std::vector<AssimpFactory::LoadedMaterial> AssimpFactory::LoadAllMaterials(const aiScene* scene)
	{
		std::vector<LoadedMaterial> materials;

		if (!scene || !scene->HasMaterials())
			return materials;

		materials.resize(scene->mNumMaterials);

		for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
		{
			aiMaterial* mat = scene->mMaterials[i];
			LoadedMaterial& outMat = materials[i];

			// Material Name
			aiString name;
			if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
				outMat.name = name.C_Str();

			// Helper lambda to extract textures
			auto ExtractTextures = [&](aiTextureType type, std::vector<std::string>& container)
				{
					for (unsigned int t = 0; t < mat->GetTextureCount(type); ++t)
					{
						aiString path;
						if (mat->GetTexture(type, t, &path) == AI_SUCCESS)
						{
							container.push_back(path.C_Str());
						}
					}
				};

			// Standard PBR types
			ExtractTextures(aiTextureType_BASE_COLOR, outMat.albedoTextures);
			ExtractTextures(aiTextureType_DIFFUSE, outMat.albedoTextures);

			ExtractTextures(aiTextureType_NORMALS, outMat.normalTextures);
			ExtractTextures(aiTextureType_HEIGHT, outMat.normalTextures);

			ExtractTextures(aiTextureType_METALNESS, outMat.metallicTextures);
			ExtractTextures(aiTextureType_DIFFUSE_ROUGHNESS, outMat.roughnessTextures);

			ExtractTextures(aiTextureType_AMBIENT_OCCLUSION, outMat.aoTextures);
			ExtractTextures(aiTextureType_EMISSIVE, outMat.emissiveTextures);
			ExtractTextures(aiTextureType_OPACITY, outMat.opacityTextures);
		}

		return materials;
	}

	std::string AssimpFactory::FormatTexturePath(const std::string& path, const aiTextureType& type)
	{
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath_s(path.c_str(), drive, dir, fname, ext);

		// remove \r
		std::string fnameFixed = (std::string)fname;
		fnameFixed.erase(std::remove(fnameFixed.begin(), fnameFixed.end(), '\r'), fnameFixed.end());
		std::string fileName = fnameFixed + (std::string)ext;

		std::string newPath = "..\\..\\Assets\\test.tga";

		// Model Texture
		if (type == aiTextureType::aiTextureType_DIFFUSE)
		{
			newPath = m_pathDirectory + fileName;
			return newPath;
		}

		if (type == aiTextureType::aiTextureType_NORMALS)
		{
			newPath = m_pathDirectory + fnameFixed + "_NRM" + (std::string)ext;
			struct _stat buffer;
			if (_stat(newPath.c_str(), &buffer) != 0)
			{
				newPath.append(" is missing, default normal loaded instead.\n");
				DebugTrace((LPCSTR)newPath.c_str());
				newPath = "..\\..\\Assets\\defaultn.DDS";
			}
			return newPath;
		}

		if (type == aiTextureType::aiTextureType_SPECULAR)
		{
			newPath = m_pathDirectory + fnameFixed + "_SPEC" + (std::string)ext;
			struct _stat buffer;
			if (_stat(newPath.c_str(), &buffer) != 0)
			{
				newPath.append(" is missing, default specular loaded instead.\n");
				DebugTrace((LPCSTR)newPath.c_str());
				newPath = "..\\..\\Assets\\defaults.DDS";
			}
			return newPath;
		}

		if (type == aiTextureType::aiTextureType_DISPLACEMENT)
		{
			newPath = m_pathDirectory + fnameFixed + "_DISP" + (std::string)ext;
			struct _stat buffer;
			if (_stat(newPath.c_str(), &buffer) != 0)
			{
				newPath.append(" is missing, default displacement loaded instead.\n");
				DebugTrace((LPCSTR)newPath.c_str());
				newPath = "..\\..\\Assets\\defaultd.DDS";
			}
			return newPath;
		}

		return newPath;
	}

	void AssimpFactory::LoadBones(int meshIndex, const aiMesh* pMesh, std::vector<AnimationStructs::VertexBoneData>& bones)
	{
		for (unsigned int i = 0; i < pMesh->mNumBones; i++)
		{
			int boneIndex = 0;
			std::string BoneName(pMesh->mBones[i]->mName.data);

			if (m_boneMapping.find(BoneName) == m_boneMapping.end())
			{
				// Allocate an index for bone
				boneIndex = m_numBones;
				m_numBones++;
				XMMATRIX boneOffsetMatrix = XMMatrixSet(pMesh->mBones[i]->mOffsetMatrix.a1, pMesh->mBones[i]->mOffsetMatrix.a2, pMesh->mBones[i]->mOffsetMatrix.a3, pMesh->mBones[i]->mOffsetMatrix.a4,
					pMesh->mBones[i]->mOffsetMatrix.b1, pMesh->mBones[i]->mOffsetMatrix.b2, pMesh->mBones[i]->mOffsetMatrix.b3, pMesh->mBones[i]->mOffsetMatrix.b4,
					pMesh->mBones[i]->mOffsetMatrix.c1, pMesh->mBones[i]->mOffsetMatrix.c2, pMesh->mBones[i]->mOffsetMatrix.c3, pMesh->mBones[i]->mOffsetMatrix.c4,
					pMesh->mBones[i]->mOffsetMatrix.d1, pMesh->mBones[i]->mOffsetMatrix.d2, pMesh->mBones[i]->mOffsetMatrix.d3, pMesh->mBones[i]->mOffsetMatrix.d4);

				//XMFLOAT4X4 boneOffsetMatrix = XMFLOAT4X4(pMesh->mBones[i]->mOffsetMatrix.a1, pMesh->mBones[i]->mOffsetMatrix.b1, pMesh->mBones[i]->mOffsetMatrix.c1, pMesh->mBones[i]->mOffsetMatrix.d1,
				//										pMesh->mBones[i]->mOffsetMatrix.a2, pMesh->mBones[i]->mOffsetMatrix.b2, pMesh->mBones[i]->mOffsetMatrix.c2, pMesh->mBones[i]->mOffsetMatrix.d2,
				//										pMesh->mBones[i]->mOffsetMatrix.a3, pMesh->mBones[i]->mOffsetMatrix.b3, pMesh->mBones[i]->mOffsetMatrix.c3, pMesh->mBones[i]->mOffsetMatrix.d3,
				//										pMesh->mBones[i]->mOffsetMatrix.a4, pMesh->mBones[i]->mOffsetMatrix.b4, pMesh->mBones[i]->mOffsetMatrix.c4, pMesh->mBones[i]->mOffsetMatrix.d4);

				m_boneInfo.push_back(boneOffsetMatrix);
				m_boneMapping[BoneName] = boneIndex;
			}
			else
			{
				boneIndex = m_boneMapping[BoneName];
			}

			for (unsigned int j = 0; j < pMesh->mBones[i]->mNumWeights; j++)
			{
				int vertexID = m_meshEntries[meshIndex].baseVertex + pMesh->mBones[i]->mWeights[j].mVertexId;
				float weight = pMesh->mBones[i]->mWeights[j].mWeight;
				bones[vertexID].AddBoneData(boneIndex, weight);
			}
		}
	}

	AssimpFactory::AssimpFactory(const UINT modelId, const std::string& fileName, unsigned int customFlags) :
		m_boundingSphereRadiusTranslation(XMMatrixIdentity())
	{
		m_modelId = modelId;
		m_pathFileName = fileName;

		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath_s(fileName.c_str(), drive, dir, fname, ext);
		// get the dir for loading textures from the correct path
		m_pathDirectory = dir;
		m_fileName = std::string(fname);

		m_pScene = m_importer.ReadFile(m_pathFileName, customFlags);

		if (!m_pScene || m_pScene->mNumMeshes == 0)
		{
			throw;
		}

		UINT numVertices = 0;
		UINT numIndices = 0;

		m_meshEntries.resize(m_pScene->mNumMeshes);
		DoMeshTransforms(m_pScene->mRootNode, XMMatrixIdentity());

		for (UINT i = 0; i < m_pScene->mNumMeshes; i++)
		{
			MeshEntry* mesh = &m_meshEntries[i];
			CreateSingleMeshEntry(i, numVertices, numIndices, mesh);
		}

		// Initialize the meshes in the scene one by one
		for (UINT i = 0; i < m_meshEntries.size(); i++)
		{
			if (m_meshEntries[i].hasBones)
			{
				const aiMesh* paiMesh = m_pScene->mMeshes[i];
				m_bones.resize(m_bones.size() + m_meshEntries[i].numVerts);
				LoadBones(i, paiMesh, m_bones);
			}
		}
	}

	AssimpFactory::~AssimpFactory()
	{

	}

	void AssimpFactory::CreateDeviceDependentResources(DX::DeviceResources* deviceResources)
	{
		ID3D12Device5* d3dDevice = deviceResources->GetD3DDevice();
		m_vertexBuffer.CreateDeviceDependentResources(d3dDevice);
		m_indexBuffer.CreateDeviceDependentResources(d3dDevice);
	}

	void AssimpFactory::CreateBuffers(ID3D12GraphicsCommandList4* commandList)
	{
		m_vertexBuffer.CpuData = m_meshEntries[0].vertices; // todo: index 0 is the first mesh, but the Models map will have the correct mesh to use as the main mesh
		m_vertexBuffer.CreateOnDefaultHeap(commandList, L"Model Buffer");

		m_indexBuffer.CpuData = m_meshEntries[0].indices;
		m_indexBuffer.CreateOnDefaultHeap(commandList, L"Index Buffer");
	}

	void AssimpFactory::CreateShaderResources()
	{
		m_vertexBuffer.CreateShaderResourceView();
		m_indexBuffer.CreateShaderResourceView();
	}

	void AssimpFactory::LoadJson()
	{
		// see if animations have already been loaded
		if (AssimpFactory::Models.size() > 0)
		{
			return;
		}

		{
			std::string filePath = "../../Assets/Json/Models.json";
			rapidjson::Document doc = LoadJsonDocument(filePath);

			const auto& arr = doc["models"];

			Model model;
			for (auto& v : arr.GetArray()) {
				model.modelId = v["id"].GetInt();
				model.name = v["name"].GetString();
				model.meshEntryLocation = v["meshEntryLocation"].GetInt();
				model.contentLocation = v["contentLocation"].GetString();

				const auto& textures = v["textureBaseColorList"];
				for (auto& tex : textures.GetArray()) {
					model.textures.push_back(tex.GetString());
				}

				//for (char& c : name)
				//{
				//	c = static_cast<char>(
				//		std::tolower(static_cast<unsigned char>(c))
				//		);
				//}

				AssimpFactory::Models[model.modelId] = model;
			}
		}
	}

	void AssimpFactory::ReleaseUploadResources()
	{
		m_vertexBuffer.ReleaseUploadResource();
		m_indexBuffer.ReleaseUploadResource();
	}

	void AssimpFactory::Release()
	{
		m_vertexBuffer.Release();
		m_indexBuffer.Release();
	}
}


