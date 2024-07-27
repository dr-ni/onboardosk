
#include <algorithm>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "logger.h"

#include "tools/file_helpers.h"
#include "tools/path_helpers.h"
#include "tools/string_helpers.h"


#include "xdgdirs.h"

XDGDirs::XDGDirs(Logger* logger) :
    m_logger(logger)
{
}

XDGDirs::~XDGDirs()
{
}

std::string XDGDirs::get_config_home(const std::string& file)
{
    fs::path path = safe_assign(std::getenv("XDG_CONFIG_HOME"));
    if (!path.empty() && !path.is_absolute())
    {
        if (logger())
            LOG_WARNING << "XDG_CONFIG_HOME doesn't contain an absolute path,"
                           "ignoring.";
        path.clear();
    }
    if (path.empty())
    {
        path = path / get_user_home_dir() / ".config/";
    }

    if (!file.empty())
    {
        path /= file;
    }

    return path;
}

void XDGDirs::get_config_dirs(std::vector<std::string> paths)
{
    std::string value = safe_assign(std::getenv("XDG_CONFIG_DIRS"));
    if (value.empty())
        value = "/etc/xdg";

    auto candidates = split(value, ':');
    std::copy_if(candidates.begin(), candidates.end(), paths.begin(),
                 [](const std::string& p) {return fs::path(p).is_absolute();});
}

void XDGDirs::get_all_config_dirs(std::vector<std::string>& paths_out,
                                  const std::string& file)
{
    get_config_dirs(paths_out);
    paths_out.insert(paths_out.begin(), get_config_home());

    if (!file.empty())
    {
        std::transform(paths_out.begin(), paths_out.end(), paths_out.begin(),
                       [&](std::string& p){return fs::path(p) / file;});
    }
}

void XDGDirs::find_config_files(std::vector<std::string>& paths_out, const std::string& file)
{
    std::vector<std::string> paths;
    get_all_config_dirs(paths, file);
    std::copy_if(paths.begin(), paths.end(), paths_out.begin(),
                 [](const std::string& p){
        return fs::is_regular_file(p) &&
                is_file_readable(p);
    });
}

std::string XDGDirs::find_config_file(const std::string& file)
{
    std::vector<std::string> paths;
    find_config_files(paths, file);
    if (!paths.empty())
    {
        return paths[0];
    }
    return {};
}

std::string XDGDirs::get_data_home(const std::string& file)
{
    fs::path path = safe_assign(std::getenv("XDG_DATA_HOME"));
    if (!path.empty() && !path.is_absolute())
    {
        if (logger())
            LOG_WARNING << "XDG_DATA_HOME doesn't contain an absolute path,"
                           " ignoring.";
        path.clear();
    }
    if (path.empty())
        path = fs::path(get_user_home_dir()) / ".local/" / "share/";

    if (!file.empty())
        path /= file;

    return path;
}

void XDGDirs::get_data_dirs(std::vector<std::string> paths_out)
{
    std::string value = safe_assign(std::getenv("XDG_DATA_DIRS"));
    if (value.empty())
        value = "/usr/local/share/:/usr/share/";

    auto candidates = split(value, ':');
    std::copy_if(candidates.begin(), candidates.end(), paths_out.begin(),
                 [](const std::string& p) {return fs::path(p).is_absolute();});
}

void XDGDirs::get_all_data_dirs(std::vector<std::string>& paths_out, const std::string& file)
{
    get_data_dirs(paths_out);
    paths_out.insert(paths_out.begin(), get_data_home());

    if (!file.empty())
    {
        std::transform(paths_out.begin(), paths_out.end(), paths_out.begin(),
                       [&](std::string& p){return fs::path(p) / file;});
    }
}

void XDGDirs::find_data_files(std::vector<std::string>& paths_out, const std::string& file)
{
    std::vector<std::string> paths;
    get_all_data_dirs(paths, file);
    std::copy_if(paths.begin(), paths.end(), paths_out.begin(),
                 [](const std::string& p){
        return fs::is_regular_file(p) &&
                is_file_readable(p);
    });
}

const std::string XDGDirs::find_data_file(const std::string& file)
{
    std::vector<std::string> paths;
    find_data_files(paths, file);
    if (!paths.empty())
    {
        return paths[0];
    }
    return {};
}

bool XDGDirs::assure_user_dir_exists(const std::string& path)
{
    bool exists = fs::exists(path);
    if (!exists)
    {
        try
        {
            fs::create_directories(path);
            fs::permissions(path, fs::perms::remove_perms | fs::perms::others_all);
            exists = true;
        }
        catch (const fs::filesystem_error& ex)
        {
            if (logger())
                LOG_ERROR << "failed to create directory '" << path
                          << "': " << ex.what();
            //throw;
        }
    }
    return exists;
}


