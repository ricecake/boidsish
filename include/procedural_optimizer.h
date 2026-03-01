#pragma once

#include "procedural_ir.h"

namespace Boidsish {

	class ProceduralOptimizer {
	public:
		static void Optimize(ProceduralIR& ir);

	private:
		static void ConsolidateTubes(ProceduralIR& ir);
		static void HandleJunctions(ProceduralIR& ir);
	};

} // namespace Boidsish
