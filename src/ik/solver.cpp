#include <ik/solver.hpp>
#include <ik/body.h>
#include <glm/gtx/quaternion.hpp>
#include <vector>

namespace Boidsish {

void IKSolver::ResolveWorldPositions(Body& body) {
    for (auto& chain : body.tree.chains) {
        glm::vec3 currentBase = body.position + chain.base;

        for (auto& bone : chain.bones) {
            if (bone.isRelative) {
                bone.position = currentBase + bone.relativePosition;
            }
            // Update orientation if it's default
            if (bone.orientation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
                glm::vec3 dir = bone.position - currentBase;
                if (glm::length(dir) > 0.0001f) {
                    bone.orientation = glm::rotation(glm::vec3(0, 1, 0), glm::normalize(dir));
                }
            }
            currentBase = bone.position;
        }
    }
}

void IKSolver::Solve(Body& body, int maxIterations, float tolerance) {
    // Ensure we start with valid world positions
    ResolveWorldPositions(body);

    if (body.tree.chains.empty()) return;

    for (int iter = 0; iter < maxIterations; ++iter) {
        float maxError = 0.0f;

        // --- Backward Pass: Effectors to Body ---
        std::vector<glm::vec3> bodyDesiredPositions;

        for (auto& chain : body.tree.chains) {
            if (!chain.hasTarget || chain.bones.empty()) {
                // Even if it has no target, it still contributes to the body's position
                // by wanting to stay where it is relative to the body.
                bodyDesiredPositions.push_back(chain.bones.empty() ? body.position : chain.bones[0].position - (chain.bones[0].position - (body.position + chain.base)));
                // Actually, if no target, it just stays.
                continue;
            }

            float error = glm::distance(chain.bones.back().position, chain.target);
            if (error > maxError) maxError = error;

            // Temporary joints for this chain: [0] is Body attachment, [1..N] are bone ends
            std::vector<glm::vec3> joints;
            joints.push_back(body.position + chain.base);
            for (const auto& bone : chain.bones) joints.push_back(bone.position);

            int n = joints.size() - 1;
            joints[n] = chain.target;

            for (int i = n - 1; i >= 0; --i) {
                auto& bone = chain.bones[i];
                float L = bone.length;

                // 1. Calculate the proposed unconstrained position
                glm::vec3 dir = joints[i] - joints[i+1];
                if (glm::length(dir) < 0.0001f) dir = glm::vec3(0, 1, 0);
                else dir = glm::normalize(dir);

                glm::vec3 proposedPos = joints[i+1] + dir * L;

                // 2. Apply constraints to the proposed position relative to the anchor (joints[i+1])
                if (!bone.joints.empty()) {
                    for (const auto& constraint : bone.joints[0].constraints) {
                        // Note: Depending on hierarchy, constraints are usually relative to the PARENT bone's orientation (i-1).
                        // Using the current bone or child bone orientation can cause jittering.
                        glm::quat refOrientation = (i > 0) ? chain.bones[i-1].orientation : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

                        if (constraint.type == ConstraintType::Hinge) {
                            glm::vec3 worldAxis = refOrientation * constraint.axis;
                            glm::vec3 worldRefAxis = refOrientation * bone.bindDir; // Transform the reference axis too!

                            proposedPos = ApplyHingeConstraint(proposedPos, joints[i+1], worldAxis, worldRefAxis, constraint.minAngle, constraint.maxAngle, L);
                        } else if (constraint.type == ConstraintType::Cone) {
                            glm::vec3 worldAxis = refOrientation * constraint.axis; // Or bone.bindDir depending on your data setup

                            proposedPos = ApplyConeConstraint(proposedPos, joints[i+1], worldAxis, constraint.angle, L);
                        }
                    }
                }

                // 3. Update the temporary buffer, not the bone
                joints[i] = proposedPos;
            }

            bodyDesiredPositions.push_back(joints[0] - chain.base);

            // Store results back to bones for forward pass
            for (size_t i = 0; i < chain.bones.size(); ++i) {
                chain.bones[i].position = joints[i+1];
            }
        }

        if (maxError < tolerance && iter > 0) break;

        // --- Update Body Position ---
        if (!bodyDesiredPositions.empty()) {
            glm::vec3 weightedCentroid(0.0f);
            float totalWeight = 0.0f;

            for (size_t i = 0; i < bodyDesiredPositions.size(); ++i) {
                const Chain& chain = body.tree.chains[i];
                float chainWeight = 0.0f;
                float chainForce = 0.0f;
                for (const auto& bone : chain.bones) {
                    chainWeight += bone.weight;
                    for (const auto& joint : bone.joints) chainForce += joint.force;
                }
                if (chainForce == 0) chainForce = 1.0f;
                if (chainWeight == 0) chainWeight = 1.0f;

                float weight = chainWeight * chainForce;
                weightedCentroid += bodyDesiredPositions[i] * weight;
                totalWeight += weight;
            }

            if (totalWeight > 0) {
                weightedCentroid /= totalWeight;

                // Body mass also resists change
                float bodyResistance = body.weight;
                body.position = glm::mix(weightedCentroid, body.position, glm::clamp(bodyResistance / (bodyResistance + totalWeight), 0.0f, 1.0f));
            }

            // Optionally blend with goal if specified
            if (glm::length(body.goal) > 0.0001f) {
                body.position = glm::mix(body.position, body.goal, 0.1f);
            }
        }

        // --- Forward Pass: Body to Effectors ---
        for (auto& chain : body.tree.chains) {
            glm::vec3 prevPos = body.position + chain.base;

            for (size_t i = 0; i < chain.bones.size(); ++i) {
                Bone& bone = chain.bones[i];
                float length = bone.length;
                glm::vec3 dir = bone.position - prevPos;
                if (glm::length(dir) < 0.0001f) dir = glm::vec3(0, 1, 0);
                else dir = glm::normalize(dir);

                // Apply constraints
                if (!bone.joints.empty()) {
                    const Joint& joint = bone.joints[0];
                    for (const auto& constraint : joint.constraints) {
                        if (constraint.type == ConstraintType::Hinge) {
                            glm::vec3 referenceAxis = bone.bindDir;
                            if (i > 0) {
                                referenceAxis = glm::normalize(chain.bones[i-1].position - (i > 1 ? chain.bones[i-2].position : body.position + chain.base));
                            }
                            bone.position = ApplyHingeConstraint(bone.position, prevPos, chain.bones[i-1].orientation * constraint.axis, referenceAxis, constraint.minAngle, constraint.maxAngle, length);
                        } else if (constraint.type == ConstraintType::Cone) {
                             glm::vec3 coneAxis = bone.bindDir;
                             if (i > 0) {
                                 coneAxis = glm::normalize(chain.bones[i-1].position - (i > 1 ? chain.bones[i-2].position : body.position + chain.base));
                             }
                             bone.position = ApplyConeConstraint(bone.position, prevPos, chain.bones[i-1].orientation * coneAxis, constraint.angle, length);
                        }
                    }
                }

                bone.position = prevPos + glm::normalize(bone.position - prevPos) * length;

                // Update orientation
                glm::vec3 newDir = glm::normalize(bone.position - prevPos);
                bone.orientation = glm::rotation(bone.bindDir, newDir);

                // Replace the Twist constraint block in solver.cpp
                if (!bone.joints.empty()) {
                    const Joint& joint = bone.joints[0];
                    for (const auto& constraint : joint.constraints) {
                        if (constraint.type == ConstraintType::Twist) {

                            // Default global Z as the reference axis
                            glm::vec3 parentRef = glm::vec3(0, 0, 1);

                            if (i > 0) {
                                // Get the PREVIOUS bone's Z-axis to use as our parent reference
                                parentRef = chain.bones[i-1].orientation * glm::vec3(0, 0, 1);
                            }

                            ApplyTwistConstraint(bone.orientation, newDir, parentRef, constraint.angle);
                        }
                    }
                }
                prevPos = bone.position;
            }
        }
    }
}

} // namespace Boidsish
