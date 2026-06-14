#pragma once
namespace CPyburnRTXEngine
{
	class ShapeHelper
	{
	public:
		struct Vertex
		{
			Vertex() {}
			Vertex(
				const DirectX::XMFLOAT3& p,
				const DirectX::XMFLOAT3& n,
				const DirectX::XMFLOAT3& t,
				const DirectX::XMFLOAT2& uv) :
				Position(p),
				Normal(n),
				TangentU(t),
				TexC(uv) {}
			Vertex(
				float px, float py, float pz,
				float nx, float ny, float nz,
				float tx, float ty, float tz,
				float u, float v) :
				Position(px, py, pz),
				Normal(nx, ny, nz),
				TangentU(tx, ty, tz),
				TexC(u, v) {}

			DirectX::XMFLOAT3 Position = DirectX::XMFLOAT3();
			DirectX::XMFLOAT3 Normal = DirectX::XMFLOAT3();
			DirectX::XMFLOAT3 TangentU = DirectX::XMFLOAT3();
			DirectX::XMFLOAT2 TexC = DirectX::XMFLOAT2();
		};

		struct MeshData
		{
			std::vector<Vertex> Vertices;
			std::vector<UINT> Indices32;

			std::vector<UINT>& GetIndices16()
			{
				if (mIndices16.empty())
				{
					mIndices16.resize(Indices32.size());
					for (size_t i = 0; i < Indices32.size(); ++i)
						mIndices16[i] = static_cast<UINT>(Indices32[i]);
				}

				return mIndices16;
			}

			~MeshData()
			{
				Vertices.clear();
				Indices32.clear();
			}

		private:
			std::vector<UINT> mIndices16;
		};

		///<summary>
		/// Creates a box centered at the origin with the given dimensions, where each
		/// face has m rows and n columns of vertices.
		///</summary>
		static MeshData CreateBox(float width, float height, float depth, UINT numSubdivisions);

		///<summary>
		/// Creates a sphere centered at the origin with the given radius.  The
		/// slices and stacks parameters control the degree of tessellation.
		///</summary>
		static MeshData CreateSphere(const XMFLOAT3& center, float radius, UINT sliceCount, UINT stackCount);

		///<summary>
		/// Creates a geosphere centered at the origin with the given radius.  The
		/// depth controls the level of tessellation.
		///</summary>
		static MeshData CreateGeosphere(float radius, UINT numSubdivisions);

		///<summary>
		/// Creates a cylinder parallel to the y-axis, and centered about the origin.  
		/// The bottom and top radius can vary to form various cone shapes rather than true
		// cylinders.  The slices and stacks parameters control the degree of tessellation.
		///</summary>
		static MeshData CreateCylinder(float bottomRadius, float topRadius, float height, UINT sliceCount, UINT stackCount);

		///<summary>
		/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
		/// at the origin with the specified width and depth.
		///</summary>
		static MeshData CreateGrid(float width, float depth, UINT m, UINT n);

		///<summary>
		/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
		///</summary>
		static MeshData CreateQuad(float x, float y, float w, float h, float depth);

	private:
		static void Subdivide(MeshData& meshData);
		static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
		static void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, UINT sliceCount, UINT stackCount, MeshData& meshData);
		static void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, UINT sliceCount, UINT stackCount, MeshData& meshData);
	};
}
