#include "pchlib.h"
#include "TestAssimp.h"

namespace CPyburnRTXEngine
{
	void TestAssimp::DoMeshTransforms(aiNode* node, XMMATRIX parentTransform)
	{
		XMMATRIX nodeTransform = XMMatrixIdentity();
		XMMATRIX nodeT = XMMatrixSet(node->mTransformation.a1, node->mTransformation.a2, node->mTransformation.a3, node->mTransformation.a4, node->mTransformation.b1, node->mTransformation.b2, node->mTransformation.b3, node->mTransformation.b4, node->mTransformation.c1, node->mTransformation.c2, node->mTransformation.c3, node->mTransformation.c4, node->mTransformation.d1, node->mTransformation.d2, node->mTransformation.d3, node->mTransformation.d4);

		// check LCL data, translation, prerotation, rotation (importing from Unity adds some of these that need to be ignored for the import to work, still figuring them all out with each model I import)
		std::string name(node->mName.data);
		std::string nameLower = name;
		stringToLower(nameLower);

		for (size_t i = 0; i < node->mNumMeshes; i++)
		{
			UINT meshIndex = node->mMeshes[i];
			XMStoreFloat4x4(&m_meshEntries[meshIndex].meshTransform, nodeTransform);
		}

		for (size_t i = 0; i < node->mNumChildren; i++)
		{
			DoMeshTransforms(node->mChildren[i], nodeTransform);
		}
	}

	void TestAssimp::CreateSingleMeshEntry(UINT i, UINT& numVertices, UINT& numIndices, MeshEntry* mesh)
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

		// todo: take out the m_vertices and m_indices? why have this passed in if it is local
		InitializeMesh(i, mesh, m_vertices, m_indices);
		InitializeMaterials();
	}

	void TestAssimp::InitializeMesh(const UINT& i, MeshEntry* meshEntry, std::vector<VSVertices>& vertices, std::vector<unsigned int>& indices)
	{
		const aiMesh* paiMesh = m_pScene->mMeshes[i];

		int startingV = vertices.size();
		vertices.resize(startingV + paiMesh->mNumVertices);

		int startingI = indices.size();
		indices.resize(startingI + paiMesh->mNumFaces * 3);

		m_positions.resize(vertices.size());

		std::vector<XMFLOAT3> perMeshPositions(paiMesh->mNumVertices);

		// Populate the vertex attribute vectors
		for (UINT i = 0; i < paiMesh->mNumVertices; i++)
		{
			const aiVector3D* pPos = &paiMesh->mVertices[i];
			const aiVector3D* pTexCoord = paiMesh->HasTextureCoords(0) ? &paiMesh->mTextureCoords[0][i] : &aiVector3D(0.0f, 0.0f, 0.0f);
			const aiVector3D* pNormal = &paiMesh->mNormals[i];
			const aiVector3D* pTangent = &paiMesh->mTangents[i];
			const aiVector3D* pBiTangent = &paiMesh->mBitangents[i];

			VSVertices vertice;
			vertice.position = XMFLOAT3(pPos->x, pPos->y, pPos->z);
			//perMeshPositions[i] = vertice.position;

			XMVECTOR xmPosition = XMLoadFloat3(&vertice.position);
			XMMATRIX xmTransform = XMLoadFloat4x4(&meshEntry->meshTransform);
			XMVECTOR xmTranPos = XMVector3TransformCoord(xmPosition, XMMatrixTranspose(xmTransform));
			XMStoreFloat3(&m_positions[startingV + i], xmTranPos);
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

			// hack: for missing data
			if (paiMesh->mBitangents)
			{
				vertice.binormal = XMFLOAT3(pBiTangent->x, pBiTangent->y, pBiTangent->z);
			}
			else
				vertice.binormal = XMFLOAT3(pNormal->x, pNormal->y, pNormal->z);

			vertices[startingV + i] = vertice;
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
		XMStoreFloat4x4(&m_boundingSphereRadiusTranslation, xmRadiusTranslation);

		perMeshPositions.clear();

		// Populate the index buffer
		int positionCounter = 0;
		for (unsigned int i = 0; i < paiMesh->mNumFaces; i++)
		{
			const aiFace& face = paiMesh->mFaces[i];
			int indice = startingI + i * 3;
			indices[indice] = face.mIndices[0];

			indice++;
			indices[indice] = face.mIndices[1];

			indice++;
			indices[indice] = face.mIndices[2];
		}
	}

	void TestAssimp::InitializeMaterials()
	{
		// Initialize the materials
		for (UINT i = 0; i < m_pScene->mNumMaterials; i++)
		{
			const aiMaterial* pMaterial = m_pScene->mMaterials[i];

			if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString aiPath;

				if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
				{
					m_textureDiffuse.push_back(m_pathDirectory + (std::string)aiPath.data);
				}

				if (pMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0)
				{
					aiString aiPath;

					if (pMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiPath, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
					{
						m_textureSPEC.push_back(m_pathDirectory + (std::string)aiPath.data);
					}
				}

				if (pMaterial->GetTextureCount(aiTextureType_NORMALS) > 0)
				{
					aiString aiPath;

					if (pMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiPath, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
					{
						m_textureNRM.push_back(m_pathDirectory + (std::string)aiPath.data);
					}
				}

				if (pMaterial->GetTextureCount(aiTextureType_DISPLACEMENT) > 0)
				{
					aiString aiPath;

					if (pMaterial->GetTexture(aiTextureType_DISPLACEMENT, 0, &aiPath, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
					{
						m_textureDISP.push_back(m_pathDirectory + (std::string)aiPath.data);
					}
				}
			}
			else
			{
				m_textureDiffuse.push_back("Assets\\test.tga");
			}
		}
	}

	void TestAssimp::FormatTexturePath(const std::string& path, const std::string& appendType)
	{

	}

	TestAssimp::TestAssimp()
	{

	}

	TestAssimp::~TestAssimp()
	{

	}

	void TestAssimp::Initialize(const std::string& fileName, unsigned int customFlags)
	{
		m_pathFileName = fileName;

		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath_s(fileName.c_str(), drive, dir, fname, ext);
		// get the dir for loading textures from the correct path
		m_pathDirectory = dir;
		m_fileName = std::string(fname);

		m_pScene = m_importer.ReadFile("Assets\\" + m_pathFileName, customFlags);

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
	}
}


