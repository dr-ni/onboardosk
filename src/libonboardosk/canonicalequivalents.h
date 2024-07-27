#ifndef CANONICALEQUIVALENTS_H
#define CANONICALEQUIVALENTS_H

#include <map>
#include <vector>
#include <string>

using CanonicalEquivalentsMap = std::map<std::string, std::vector<std::string>>;
using CanonicalEquivalentsTable = std::map<std::string, CanonicalEquivalentsMap>;

extern CanonicalEquivalentsTable g_canonical_equivalents;

#endif // CANONICALEQUIVALENTS_H
