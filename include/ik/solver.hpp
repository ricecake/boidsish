#ifndef BOIDSISH_IK_SOLVER_HPP
#define BOIDSISH_IK_SOLVER_HPP

#include "body.h"

namespace Boidsish {

class IKSolver {
public:
    /**
     * @brief Solves the IK for the given body using a tree-based FABRIK algorithm.
     *
     * @param body The body structure to solve.
     * @param maxIterations Maximum number of iterations.
     * @param tolerance Convergence tolerance (distance).
     */
    static void Solve(Body& body, int maxIterations = 20, float tolerance = 0.001f);

    /**
     * @brief Resolves world positions for all bones in the body based on relative coordinates.
     *
     * @param body The body to resolve.
     */
    static void ResolveWorldPositions(Body& body);
};

} // namespace Boidsish

#endif
