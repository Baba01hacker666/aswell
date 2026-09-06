#pragma once

#include "aswell/common.hpp"
#include "aswell/config/config.hpp"
#include "aswell/config/theme.hpp"
#include "aswell/ui/prompt.hpp"

namespace aswell {

class ConfigEditor {
public:
    static int run_interactive(Environment& env);
};

} // namespace aswell
