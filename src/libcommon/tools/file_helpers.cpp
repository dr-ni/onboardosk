#include <cassert>
#include <fstream>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "string.h"   // strerror

#include "tools/path_helpers.h"
#include "tools/string_helpers.h"

#include "file_helpers.h"

bool is_file(const std::string& fn)
{
    return fs::is_regular_file(fn);
}

bool is_file_readable(const std::string& fn)
{
    std::ifstream file(fn);
    return file.good();
}

bool is_valid_filename(const std::string& fn)
{
  return (!fn.empty() &&
          fs::path(fn).is_absolute() &&
          is_file(fn));
}

bool read_file(std::vector<std::string>& lines,
               const std::string& fn, size_t max_characters)
{
    std::string error;
    std::string text;
    if (!read_file(text, fn, max_characters, error))
        return false;

    split(text, L'\n', std::back_inserter(lines));
    return true;
}

bool read_file(std::string& result,
               const std::string& fn,
               size_t max_characters)
{
    std::string error;
    return read_file(result, fn, max_characters, error);
}

bool read_file(std::string& result,
               const std::string& fn,
               size_t max_characters,
               std::string& error)
{
    std::ifstream ifs(fn);
    if (!ifs.good())
    {
        error = strerror(errno);
        return false;
    }

    if (max_characters == 0)
    {
        // read whole file
        std::stringstream ss;
        ss << ifs.rdbuf();
        result = ss.str();
    }
    else
    {
        // read first n characters
        result.resize(max_characters);
        ifs.read(&result[0], static_cast<std::streamsize>(max_characters));
        result.resize(static_cast<size_t>(ifs.gcount()));
    }
    return true;
}

bool write_file(std::vector<std::string>& lines, const std::string& fn)
{
    std::string error;
    std::ofstream ofs(fn);
    if (!ofs.good())
    {
        error = strerror(errno);
        return false;
    }
    for (const auto& line : lines)
    {
        ofs << line;
        if (!endswith(line, "\n"))
            ofs << std::endl;

        if (!ofs.good())
        {
            error = strerror(errno);
            return false;
        }
    }
    return true;
}

bool touch_file(const std::string& fn)
{
    std::string error;
    return touch_file(fn, error);
}

bool touch_file(const std::string& fn, std::string& error)
{
    std::ofstream ofs(fn);
    if (ofs.good())
    {
        ofs << "";
        return true;
    }
    error = strerror(errno);
    return false;
}

std::string get_current_directory()
{
    // "Returns the absolute path of the current working directory"
    return fs::current_path();
}

std::string expand_user_sys_filename(const std::string& filename,
                                     const FilenameCallback& user_filename_func,
                                     const FilenameCallback& system_filename_func)
{
    std::string result = filename;
    if (!result.empty() && !is_valid_filename(result))
    {
        if (user_filename_func)
        {
            result = user_filename_func(filename);
            if (!is_file(result))
            {
                result = "";
            }
        }

        if (result.empty() && system_filename_func)
        {
            result = system_filename_func(filename);
            if (!is_file(result))
            {
                result = "";
            }
        }
    }

    return result;
}



TempDir::TempDir(const std::string& prefix)
{
    this->dir = fs::temp_directory_path() / (prefix + "XXXXXXXXXX");
    if (mkdtemp(&this->dir[0]) == nullptr)
        throw std::runtime_error("mkdtemp failed");
}

TempDir::~TempDir()
{
    if (!this->dir.empty() && fs::is_directory(this->dir))
    {
        fs::remove_all(this->dir);
        assert(!fs::is_directory(this->dir));
    }
    assert(!fs::is_directory(this->dir));  // must be really gone by now
}


bool backup_file(const std::string& fn, const std::string& dir)
{
    try
    {
        auto name = fs::path(fn).filename();
        if (!fs::exists(dir))
            fs::create_directories(dir);

        std::string cpfn = get_counted_file_name(dir, name);
        fs::copy_file(fn, cpfn);
    }
    catch (const fs::filesystem_error& ex)
    {
        return false;
    }
    return true;
}
