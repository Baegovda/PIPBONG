#pragma once

#include "app/SessionRunPolicy.h"

#include <cstdint>
#include <string>
#include <vector>

struct SessionRunPolicyViolation {
    std::string invariantId;
    std::string messageKo;
    std::string detailEn;
};

struct SessionRunPolicySweepResult {
    int statesChecked = 0;
    std::vector<SessionRunPolicyViolation> violations;
};

/// Returns empty vector when all invariants hold for this input.
std::vector<SessionRunPolicyViolation> checkSessionRunPolicyInvariants(
    const SessionRunPolicyInput& session);

/// Enumerates a bounded grid of mode/phase/flag combinations.
SessionRunPolicySweepResult runSessionRunPolicyExhaustiveSweep();

/// Pseudo-random inputs; reproducible via seed.
SessionRunPolicySweepResult runSessionRunPolicyFuzzSweep(std::uint32_t seed, int sampleCount);
