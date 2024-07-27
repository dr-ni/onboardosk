#ifndef PATH_HELPERS_H
#define PATH_HELPERS_H

#include <string>

std::string get_user_home_dir();
std::string get_basename(const std::string& fn);
std::string get_filename(const std::string& fn);
bool path_exists_starting_with(const std::string& partial_path);

std::string get_counted_file_name(const std::string& dir, const std::string& name);
#endif // PATH_HELPERS_H
