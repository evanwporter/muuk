#include <unordered_map>
#include <string>

// { Dependency { Versioning { T }}}
template<typename T>
using DependencyVersionMap = std::unordered_map<std::string, std::unordered_map<std::string, T>>;
