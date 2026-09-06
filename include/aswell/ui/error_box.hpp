#pragma once

#include "aswell/common.hpp"

namespace aswell {

class ErrorBox {
public:
    static void render_not_found(const std::string& cmd,
                                 const std::vector<std::string>& suggestions);

    static void render_error(const std::string& title,
                             const std::string& message);
};

} // namespace aswell
