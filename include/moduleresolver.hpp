#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "buildmanager.h"

namespace muuk {
    void generate_compilation_database(
        BuildManager& build_manager, 
        const std::string& dependency_db, 
        const std::string& build_dir);
    nlohmann::json parse_dependency_db(const std::string& dependency_db);
    void resolve_provided_modules(
        const nlohmann::json& dependencies,
        std::unordered_map<std::string, CompilationTarget*>& target_map);
    void resolve_required_modules(
        const nlohmann::json& dependencies,
        BuildManager& build_manager,
        std::unordered_map<std::string, CompilationTarget*>& target_map);
    void resolve_modules(BuildManager& build_manager, const std::string& build_dir);
} // namespace muuk
