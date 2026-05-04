#pragma once

#include "procedural_ir.h"

namespace Boidsish {

	class ProceduralRefiner {
	public:
		static void Refine(ProceduralIR& ir);

	private:
		static void EnsureClosedManifold(ProceduralIR& ir);
	};

} // namespace Boidsish
