#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "shape.h"

namespace Boidsish {

	enum class PolyhedronType {
		Tetrahedron,
		Cube,
		Octahedron,
		Dodecahedron,
		Icosahedron,
		SmallStellatedDodecahedron,
		GreatDodecahedron,
		GreatStellatedDodecahedron,
		GreatIcosahedron
	};

	class Polyhedron: public Shape {
	public:
		Polyhedron(
			PolyhedronType type = PolyhedronType::Cube,
			int            id = 0,
			float          x = 0.0f,
			float          y = 0.0f,
			float          z = 0.0f,
			float          size = 1.0f,
			float          r = 1.0f,
			float          g = 1.0f,
			float          b = 1.0f,
			float          a = 1.0f
		);

		~Polyhedron();

		// Shape interface
		void      render() const override;
		void      render(Shader& shader, const glm::mat4& model_matrix) const override;
		glm::mat4 GetModelMatrix() const override;

		void GenerateRenderPackets(std::vector<RenderPacket>& out_packets, const RenderContext& context) const override;
		bool Intersects(const Ray& ray, float& t) const override;
		AABB GetAABB() const override;

		std::string GetInstanceKey() const override;

		void PrepareResources(Megabuffer* megabuffer = nullptr) const override;

		static void InitPolyhedronMesh(PolyhedronType type, Megabuffer* megabuffer = nullptr);
		static void DestroyPolyhedronMeshes();

	private:
		PolyhedronType type_;
		float          size_;

		// Static mesh data for each type
		struct MeshData {
			unsigned int         vao = 0;
			unsigned int         vbo = 0;
			unsigned int         ebo = 0;
			int                  index_count = 0;
			MegabufferAllocation alloc;
			AABB                 local_aabb;
		};

		static std::map<PolyhedronType, MeshData> s_meshes;
		static std::recursive_mutex               s_mesh_mutex;

		void EnsureMeshInitialized(Megabuffer* megabuffer = nullptr) const;

	protected:
		virtual bool GetDefaultCastsShadows() const override { return true; }
	};

} // namespace Boidsish
