#include "pchlib.h"
#include "AssimpFactory.h"

namespace CPyburnRTXEngine
{
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

		// todo: take out the m_vertices and m_indices? why have this passed in if it is local
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
			const aiVector3D* pBiTangent = &paiMesh->mBitangents[i];

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

			// hack: for missing data
			if (paiMesh->mBitangents)
			{
				vertice.binormal = XMFLOAT3(pBiTangent->x, pBiTangent->y, pBiTangent->z);
			}
			else
				vertice.binormal = XMFLOAT3(pNormal->x, pNormal->y, pNormal->z);

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

	void AssimpFactory::CalcInterpolatedScaling(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim)
	{
		if (pNodeAnim->mNumScalingKeys == 1)
		{
			out = pNodeAnim->mScalingKeys[0].mValue;
			return;
		}

		int scalingIndex = FindScaling(animationTime, pNodeAnim);
		unsigned int nextScalingIndex = (scalingIndex + 1);
		float deltaTime = (float)(pNodeAnim->mScalingKeys[nextScalingIndex].mTime - pNodeAnim->mScalingKeys[scalingIndex].mTime);
		float factor = (animationTime - (float)pNodeAnim->mScalingKeys[scalingIndex].mTime) / deltaTime;
		const aiVector3D& start = pNodeAnim->mScalingKeys[scalingIndex].mValue;
		const aiVector3D& end = pNodeAnim->mScalingKeys[nextScalingIndex].mValue;
		aiVector3D delta = end - start;
		out = start + factor * delta;
	}

	void AssimpFactory::CalcInterpolatedRotation(aiQuaternion& out, float animationTime, const aiNodeAnim* pNodeAnim)
	{
		if (pNodeAnim->mNumRotationKeys == 1)
		{
			out = pNodeAnim->mRotationKeys[0].mValue;
			return;
		}

		int rotationIndex = FindRotation(animationTime, pNodeAnim);
		unsigned int nextRotationIndex = (rotationIndex + 1);
		float deltaTime = (float)(pNodeAnim->mRotationKeys[nextRotationIndex].mTime - pNodeAnim->mRotationKeys[rotationIndex].mTime);
		float factor = (animationTime - (float)pNodeAnim->mRotationKeys[rotationIndex].mTime) / deltaTime;
		const aiQuaternion& startRotationQ = pNodeAnim->mRotationKeys[rotationIndex].mValue;
		const aiQuaternion& endRotationQ = pNodeAnim->mRotationKeys[nextRotationIndex].mValue;
		aiQuaternion::Interpolate(out, startRotationQ, endRotationQ, factor);
		out = out.Normalize();
	}

	void AssimpFactory::CalcInterpolatedPosition(aiVector3D& out, float animationTime, const aiNodeAnim* pNodeAnim)
	{
		if (pNodeAnim->mNumPositionKeys == 1)
		{
			out = pNodeAnim->mPositionKeys[0].mValue;
			return;
		}

		int positionIndex = FindPosition(animationTime, pNodeAnim);
		unsigned int nextPositionIndex = (positionIndex + 1);
		float deltaTime = (float)(pNodeAnim->mPositionKeys[nextPositionIndex].mTime - pNodeAnim->mPositionKeys[positionIndex].mTime);
		float factor = (animationTime - (float)pNodeAnim->mPositionKeys[positionIndex].mTime) / deltaTime;
		const aiVector3D& start = pNodeAnim->mPositionKeys[positionIndex].mValue;
		const aiVector3D& end = pNodeAnim->mPositionKeys[nextPositionIndex].mValue;
		aiVector3D delta = end - start;
		out = start + factor * delta;
	}

	int AssimpFactory::FindScaling(float animationTime, const aiNodeAnim* pNodeAnim)
	{
		for (unsigned int i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
		{
			if (animationTime < (float)pNodeAnim->mScalingKeys[i + 1].mTime)
			{
				return i;
			}
		}
		return 0;
	}

	int AssimpFactory::FindRotation(float animationTime, const aiNodeAnim* pNodeAnim)
	{
		for (unsigned int i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
		{
			if (animationTime < (float)pNodeAnim->mRotationKeys[i + 1].mTime)
			{
				return i;
			}
		}
		return 0;
	}

	int AssimpFactory::FindPosition(float animationTime, const aiNodeAnim* pNodeAnim)
	{
		for (unsigned int i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
		{
			if (animationTime < (float)pNodeAnim->mPositionKeys[i + 1].mTime)
			{
				return i;
			}
		}
		return 0;
	}

	const aiNodeAnim* AssimpFactory::FindNodeAnim(const aiAnimation* pAnimation, const std::string& nodeName)
	{
		for (unsigned int i = 0; i < pAnimation->mNumChannels; i++)
		{
			const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];

			if (std::string(pNodeAnim->mNodeName.data) == nodeName)
			{
				return pNodeAnim;
			}
		}

		return NULL;
	}

	void AssimpFactory::CreateSkeletonBones(const aiNode* pNode, Bone* pBone)
	{
		pBone->nodeName = pNode->mName.data;
		pBone->pNodeAnim = FindNodeAnim(m_pScene->mAnimations[0], pBone->nodeName);
		pBone->parentNodeTransformation = XMFLOAT4X4(pNode->mTransformation.a1, pNode->mTransformation.a2, pNode->mTransformation.a3, pNode->mTransformation.a4,
			pNode->mTransformation.b1, pNode->mTransformation.b2, pNode->mTransformation.b3, pNode->mTransformation.b4,
			pNode->mTransformation.c1, pNode->mTransformation.c2, pNode->mTransformation.c3, pNode->mTransformation.c4,
			pNode->mTransformation.d1, pNode->mTransformation.d2, pNode->mTransformation.d3, pNode->mTransformation.d4);

		//pBone->parentNodeTransformation = XMFLOAT4X4(pNode->mTransformation.a1, pNode->mTransformation.b1, pNode->mTransformation.c1, pNode->mTransformation.d1,
		//											pNode->mTransformation.a2, pNode->mTransformation.b2, pNode->mTransformation.c2, pNode->mTransformation.d2,
		//											pNode->mTransformation.a3, pNode->mTransformation.b3, pNode->mTransformation.c3, pNode->mTransformation.d3,
		//											pNode->mTransformation.a4, pNode->mTransformation.b4, pNode->mTransformation.c4, pNode->mTransformation.d4);

		// this is a good place to point the global transform to the ModelNodeInfo tranform, needed for giving skinned models weapons, emitting particles, and emitting sounds in the right position
		//for (auto& emitter : m_emitters)
		//{
		//	std::string nodeNameToLower = stringToLowerReturned(pBone->nodeName);
		//	std::string emittersNodeNameToLower = stringToLowerReturned(emitter.emitterInfo->nodeName);
		//	if (nodeNameToLower.compare(emittersNodeNameToLower) == 0)
		//	{
		//		// store the address of the node to the emitter
		//		emitter.pGlobalTransform = &pBone->globalNodeTransform;
		//	}
		//}

		if (m_boneMapping.find(pBone->nodeName) != m_boneMapping.end())
		{
			pBone->hasBoneMapping = true;
			pBone->nodeId = m_boneMapping[pBone->nodeName];

			// store useful bones
			m_globalBones.push_back(pBone);
		}

		for (unsigned int i = 0; i < pNode->mNumChildren; i++)
		{
			pBone->children.push_back(std::make_unique<Bone>());
			CreateSkeletonBones(pNode->mChildren[i], pBone->children.back().get());
		}
	}

	void AssimpFactory::ReadSkeletonBonesBlended(float blendFactor, float animationTimeCurrent, float animationTimeTarget, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		XMMATRIX nodeTransformation = XMLoadFloat4x4(&pBone->parentNodeTransformation);

		if (pBone->pNodeAnim)
		{
			// scaling
			aiVector3D scalingCurrent;
			CalcInterpolatedScaling(scalingCurrent, animationTimeCurrent, pBone->pNodeAnim);
			XMVECTOR xmScalingCurrent = { scalingCurrent.x, scalingCurrent.y, scalingCurrent.z };

			aiVector3D scalingTarget;
			CalcInterpolatedScaling(scalingTarget, animationTimeTarget, pBone->pNodeAnim);
			XMVECTOR xmScalingTarget = { scalingTarget.x, scalingTarget.y, scalingTarget.z };

			XMVECTOR xmScaling = XMVectorLerp(xmScalingCurrent, xmScalingTarget, blendFactor);
			XMMATRIX scalingBlendedM = XMMatrixScalingFromVector(xmScaling);


			// rotation
			aiQuaternion rotationCurrentQ;
			CalcInterpolatedRotation(rotationCurrentQ, animationTimeCurrent, pBone->pNodeAnim);

			aiQuaternion rotationTargetQ;
			CalcInterpolatedRotation(rotationTargetQ, animationTimeTarget, pBone->pNodeAnim);

			aiQuaternion rotationBlendedQ;
			aiQuaternion::Interpolate(rotationBlendedQ, rotationCurrentQ, rotationTargetQ, blendFactor);

			XMFLOAT3X3 rotationBlended(rotationBlendedQ.GetMatrix().a1, rotationBlendedQ.GetMatrix().b1, rotationBlendedQ.GetMatrix().c1,
				rotationBlendedQ.GetMatrix().a2, rotationBlendedQ.GetMatrix().b2, rotationBlendedQ.GetMatrix().c2,
				rotationBlendedQ.GetMatrix().a3, rotationBlendedQ.GetMatrix().b3, rotationBlendedQ.GetMatrix().c3);
			XMMATRIX rotationBlendedM = XMLoadFloat3x3(&rotationBlended);


			// translation
			aiVector3D translationCurrent;
			CalcInterpolatedPosition(translationCurrent, animationTimeCurrent, pBone->pNodeAnim);
			XMVECTOR xmTranslationCurrent = { translationCurrent.x, translationCurrent.y, translationCurrent.z };

			aiVector3D translationTarget;
			CalcInterpolatedPosition(translationTarget, animationTimeTarget, pBone->pNodeAnim);
			XMVECTOR xmTranslationTarget = { translationTarget.x, translationTarget.y, translationTarget.z };

			XMVECTOR xmTranslation = XMVectorLerp(xmTranslationCurrent, xmTranslationTarget, blendFactor);
			XMMATRIX translationBlendedM = XMMatrixTranslationFromVector(xmTranslation);

			nodeTransformation = scalingBlendedM * rotationBlendedM * translationBlendedM;
		}

		XMMATRIX xmTransposedNodeTransformation = XMMatrixTranspose(nodeTransformation);
		XMMATRIX globalTransformation = parent * xmTransposedNodeTransformation;

		if (pBone->hasBoneMapping)
		{
			noGlobalBones[pBone->nodeId] = xmTransposedNodeTransformation * XMLoadFloat4x4(&m_boneInfo[pBone->nodeId]);
			global[pBone->nodeId] = parent;

			XMMATRIX finalTransformationConversion = parent * noGlobalBones[pBone->nodeId];
			bones[pBone->nodeId] = finalTransformationConversion;

			// transpose this for easy math for modelNodes and LOD's
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation)); //XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f) * XMMatrixScaling(0.036f, 0.036f, 0.036f) * XMMatrixTranslation(0.09f, 0.04f, 0.0f)  // * XMMatrixTranspose(XMMatrixRotationRollPitchYaw(0.0f, 1.5708f, 1.5708f))
		}
		else if (pBone->pNodeAnim) // give the areas that don't have bones but do have animation the global transform
		{
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation));
		}

		for (size_t i = 0; i < pBone->children.size(); i++)
		{
			ReadSkeletonBonesBlended(blendFactor, animationTimeCurrent, animationTimeTarget, pBone->children[i].get(), globalTransformation, bones, noGlobalBones, global);
		}
	}

	void AssimpFactory::ReadSkeletonBones(float animationTime, Bone* pBone, const XMMATRIX& parent, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		XMMATRIX nodeTransformation = XMLoadFloat4x4(&pBone->parentNodeTransformation);

		if (pBone->pNodeAnim)
		{
			aiVector3D scaling;
			CalcInterpolatedScaling(scaling, animationTime, pBone->pNodeAnim);
			XMMATRIX scalingM = XMMatrixScaling(scaling.x, scaling.y, scaling.z);

			aiQuaternion rotationQ;
			CalcInterpolatedRotation(rotationQ, animationTime, pBone->pNodeAnim);

			XMFLOAT3X3 rotation(rotationQ.GetMatrix().a1, rotationQ.GetMatrix().b1, rotationQ.GetMatrix().c1,
				rotationQ.GetMatrix().a2, rotationQ.GetMatrix().b2, rotationQ.GetMatrix().c2,
				rotationQ.GetMatrix().a3, rotationQ.GetMatrix().b3, rotationQ.GetMatrix().c3);
			XMMATRIX rotationM = (XMLoadFloat3x3(&rotation));

			aiVector3D translation;
			CalcInterpolatedPosition(translation, animationTime, pBone->pNodeAnim);
			XMMATRIX translationM = XMMatrixTranslation(translation.x, translation.y, translation.z);

			nodeTransformation = scalingM * rotationM * translationM;
		}

		XMMATRIX xmTransposedNodeTransformation = XMMatrixTranspose(nodeTransformation);
		XMMATRIX globalTransformation = parent * xmTransposedNodeTransformation;

		if (pBone->hasBoneMapping)
		{
			noGlobalBones[pBone->nodeId] = xmTransposedNodeTransformation * XMLoadFloat4x4(&m_boneInfo[pBone->nodeId]);
			global[pBone->nodeId] = parent;

			XMMATRIX finalTransformationConversion = parent * noGlobalBones[pBone->nodeId];
			bones[pBone->nodeId] = finalTransformationConversion;

			// transpose this for easy math for modelNodes and LOD's
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation)); //XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f) * XMMatrixScaling(0.036f, 0.036f, 0.036f) * XMMatrixTranslation(0.09f, 0.04f, 0.0f)  // * XMMatrixTranspose(XMMatrixRotationRollPitchYaw(0.0f, 1.5708f, 1.5708f))
		}
		else if (pBone->pNodeAnim) // give the areas that don't have bones but do have animation the global transform
		{
			XMStoreFloat4x4(&pBone->globalNodeTransform, XMMatrixTranspose(globalTransformation));
		}

		for (size_t i = 0; i < pBone->children.size(); i++)
		{
			ReadSkeletonBones(animationTime, pBone->children[i].get(), globalTransformation, bones, noGlobalBones, global);
		}
	}

	void AssimpFactory::LoadBones(int meshIndex, const aiMesh* pMesh, std::vector<VertexBoneData>& bones)
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
				XMFLOAT4X4 boneOffsetMatrix = XMFLOAT4X4(pMesh->mBones[i]->mOffsetMatrix.a1, pMesh->mBones[i]->mOffsetMatrix.a2, pMesh->mBones[i]->mOffsetMatrix.a3, pMesh->mBones[i]->mOffsetMatrix.a4,
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

	AssimpFactory::AssimpFactory() :
		m_boundingSphereRadiusTranslation(XMMatrixIdentity())
	{

	}

	AssimpFactory::~AssimpFactory()
	{

	}

	void AssimpFactory::Initialize(const std::string& fileName, unsigned int customFlags)
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

		bool hasBones = false;
		// Initialize the meshes in the scene one by one
		for (UINT i = 0; i < m_meshEntries.size(); i++)
		{
			if (m_meshEntries[i].hasBones)
			{
				hasBones = true;
				const aiMesh* paiMesh = m_pScene->mMeshes[i];
				m_bones.resize(m_bones.size() + m_meshEntries[i].numVerts);
				LoadBones(i, paiMesh, m_bones);
			}
		}

		if (hasBones)
		{
			CreateSkeletonBones(m_pScene->mRootNode, &m_rootBone);

			m_ticksPerSecond = (float)(m_pScene->mAnimations[0]->mTicksPerSecond != 0 ? m_pScene->mAnimations[0]->mTicksPerSecond : 25.0f);
			m_duration = (float)m_pScene->mAnimations[0]->mDuration;
		}
		else
		{
			DebugTrace("Does not have bones");
		}
	}

	void AssimpFactory::BoneTransformBlended(float blendFactor, float timeInSecondsCurrent, float timeInSecondsTarget, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		float timeInTicksCurrent = timeInSecondsCurrent * m_ticksPerSecond;
		float animationTimeCurrent = fmod(timeInTicksCurrent, m_duration);

		float timeInTicksTarget = timeInSecondsTarget * m_ticksPerSecond;
		float animationTimeTarget = fmod(timeInTicksTarget, m_duration);

		ReadSkeletonBonesBlended(blendFactor, animationTimeCurrent, animationTimeTarget, &m_rootBone, XMMatrixIdentity(), bones, noGlobalBones, global);
	}

	void AssimpFactory::BoneTransform(float timeInSeconds, XMMATRIX* bones, XMMATRIX* noGlobalBones, XMMATRIX* global)
	{
		float timeInTicks = timeInSeconds * m_ticksPerSecond;
		float animationTime = fmod(timeInTicks, m_duration);

		ReadSkeletonBones(animationTime, &m_rootBone, XMMatrixIdentity(), bones, noGlobalBones, global);
	}

	void AssimpFactory::VertexBoneData::AddBoneData(int boneID, float weight)
	{
		for (int i = 0; i < 4; i++)
		{
			if (Weights[i] == 0.0) {
				IDs[i] = boneID;
				Weights[i] = weight;
				return;
			}
		}

		//OutputDebugString("Warning: More than 4 weights found for bone\n");
		//for (int i = 0; i < 4; i++)
		//{
		//	if (Weights1[i] == 0.0) {
		//		IDs1[i] = boneID;
		//		Weights1[i] = weight;
		//		return;
		//	}
		//}

		//OutputDebugString("Fatal Warning: More than 8 weights found for bone\n");
	}
}


