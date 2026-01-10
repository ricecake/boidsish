#include "MakeBranchAttractor.h"

namespace Boidsish {

	MakeBranchAttractor::MakeBranchAttractor(): eng(rd()), x(-1, 1), y(0, 1), z(-1, 1) {}

	Vector3 MakeBranchAttractor::operator()(float r) {
		return r * Vector3(x(eng), y(eng), z(eng)).Normalized();
	}

} // namespace Boidsish
