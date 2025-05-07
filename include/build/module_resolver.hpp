#include <string>

#include "build/manager.hpp"

namespace muuk {
    namespace build {
        void resolve_modules(BuildManager& build_manager, const std::string& build_dir);
    } // namespace build
} // namespace muuk