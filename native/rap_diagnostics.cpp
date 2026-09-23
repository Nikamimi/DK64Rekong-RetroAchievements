#include "rap_diagnostics.h"

#include "diagnostic_log.h"

// This is intentionally tied to the pinned rcheevos revision and is excluded
// from ordinary builds. The public client API does not expose condition flags.
extern "C" {
#include "rc_client_internal.h"
}

#include <cstdio>
#include <cstdint>
#include <vector>

namespace dk64_ra {
namespace {
struct ConditionStats {
    rc_condition_t* condition;
    unsigned group;
    unsigned index;
    unsigned long long true_frames = 0;
    std::uint32_t max_hits = 0;
};

rc_client_game_info_t* observed_game = nullptr;
rc_trigger_t* rap_trigger = nullptr;
std::vector<ConditionStats> conditions;

void describe_operand(const rc_operand_t& operand, char* out, std::size_t size) {
    if (rc_operand_is_memref(&operand) && operand.value.memref) {
        std::snprintf(out, size, "mem %06X size %u kind %u",
                      operand.value.memref->address, operand.size, operand.type);
    } else if (operand.type == RC_OPERAND_CONST) {
        std::snprintf(out, size, "const %u", operand.value.num);
    } else {
        std::snprintf(out, size, "kind %u", operand.type);
    }
}

void add_group(rc_condset_t* set, unsigned group) {
    if (!set) {
        return;
    }
    unsigned index = 0;
    for (auto* condition = set->conditions; condition && index < 64;
         condition = condition->next, ++index) {
        conditions.push_back({condition, group, index});
        char left[64]{};
        char right[64]{};
        describe_operand(condition->operand1, left, sizeof(left));
        describe_operand(condition->operand2, right, sizeof(right));
        diagnostic_log("[DK64 RA diag] Rap G%u.%u: type %u op %u, %s / %s, target hits %u",
                       group, index, condition->type, condition->oper, left, right,
                       condition->required_hits);
    }
}
} // namespace

void rap_diagnostics_loaded(rc_client_t* client) {
    observed_game = nullptr;
    rap_trigger = nullptr;
    conditions.clear();
    if (!client || !client->game || client->game->public_.id != 10075) {
        return;
    }
    for (auto* subset = client->game->subsets; subset; subset = subset->next) {
        for (std::uint32_t i = 0; i < subset->public_.num_achievements; ++i) {
            const auto& achievement = subset->achievements[i];
            if (achievement.public_.id != 60497 || !achievement.trigger) {
                continue;
            }
            observed_game = client->game;
            rap_trigger = achievement.trigger;
            add_group(rap_trigger->requirement, 0);
            unsigned group = 1;
            for (auto* alt = rap_trigger->alternative; alt && group < 32;
                 alt = alt->next, ++group) {
                add_group(alt, group);
            }
            diagnostic_log("[DK64 RA diag] Rap definition: %u conditions, trigger state %u",
                           static_cast<unsigned>(conditions.size()), rap_trigger->state);
            return;
        }
    }
    diagnostic_log("[DK64 RA diag] Rap runtime trigger not found");
}

void rap_diagnostics_frame(rc_client_t* client, unsigned long long frames_seen) {
    if (!client || client->game != observed_game || !rap_trigger) {
        return;
    }
    for (auto& stat : conditions) {
        stat.true_frames += (stat.condition->is_true & 1U) != 0;
        if (stat.condition->current_hits > stat.max_hits) {
            stat.max_hits = stat.condition->current_hits;
        }
    }
    if (frames_seen % 900 == 0) {
        diagnostic_log("[DK64 RA diag] Rap runtime at frame %llu: state %u",
                       frames_seen, rap_trigger->state);
        for (const auto& stat : conditions) {
            diagnostic_log("[DK64 RA diag] Rap G%u.%u true frames %llu, peak hits %u",
                           stat.group, stat.index, stat.true_frames, stat.max_hits);
        }
    }
}

} // namespace dk64_ra
