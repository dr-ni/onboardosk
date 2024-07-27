#include <iomanip>
#include <string>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/string_helpers.h"

#include "path_helpers.h"

std::string get_user_home_dir()
{
    return safe_assign(std::getenv("HOME"));
}


std::string get_basename(const std::string& fn)
{
    return fs::path(fn).filename().replace_extension("");
}

std::string get_filename(const std::string& fn)
{
    return fs::path(fn).filename();
}

bool path_exists_starting_with(const std::string& partial_path)
{
    auto path = fs::path(partial_path);
    std::string dir = path.remove_filename();
    try
    {
        for (auto & p : fs::directory_iterator(dir))
            if (startswith(p.path(), partial_path))
                return true;
    } catch (const fs::filesystem_error&)
    {}
    return false;
}


std::string get_counted_file_name(const std::string& dir, const std::string& name)
{
    size_t i = 1;
    while (true)
    {
        std::stringstream ss;
        ss << name << "_"
           << std::setfill('0') << std::setw(4) << std::to_string(i);
        std::string path = fs::path(dir) / ss.str();
        if (!fs::exists(path))
            return path;
        i++;
    }
    return {};
}
