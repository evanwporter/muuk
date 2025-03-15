#include <memory>
#include <string>

#include "buildmanager.h"
namespace muuk {
    void resolve_modules(std::shared_ptr<BuildManager> build_manager, const std::string& build_dir);
} // namespace muuk