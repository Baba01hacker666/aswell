#pragma once

// The testable core of the `parallel` builtin: how a command list is turned into
// the job list that actually runs. Kept separate from builtins_parallel.cpp (the
// scheduler, pipes and signal handling) so the string logic can be unit tested
// without a terminal, a fork or a shell.
#include "aswell/common.hpp"

namespace aswell {

// Substitutes one item into a job template.
//
//   {}   the item, shell-quoted so spaces, ; and * cannot break out of the word
//   {.}  the item's basename with the final extension removed
//
// A template without any placeholder gets the item appended as an extra
// argument, matching what people expect from `xargs`.
std::string parallel_apply_template(const std::string& tmpl, const std::string& item);

// Splits a job list (stdin or `:::` output) into trimmed, non-empty entries.
std::vector<std::string> parallel_split_lines(const std::string& data, char delimiter);

// Number of concurrent jobs to use when the user did not pass -j: the configured
// default (0 = unset), else one per CPU, capped so a 128-core box does not fork
// 128 shells per line of stdin.
size_t parallel_default_jobs(long long configured);

} // namespace aswell
