#ifndef FILE_HELPERS_H
#define FILE_HELPERS_H

#include <string>
#include <functional>

bool is_file(const std::string& fn);
bool is_file_readable(const std::string& fn);
bool is_valid_filename(const std::string& fn);

bool read_file(std::vector<std::string>& lines,
               const std::string& fn,
               size_t max_characters=0);
bool read_file(std::string& result,
               const std::string& fn,
               size_t max_characters=0);
bool read_file(std::string& result,
               const std::string& fn,
                size_t max_characters,
               std::string& error);

bool write_file(std::vector<std::string>& lines,
                const std::string& fn);

bool touch_file(const std::string& fn);
bool touch_file(const std::string& fn, std::string& error);

std::string get_current_directory();

using FilenameCallback = std::function<std::string(std::string)>;
std::string expand_user_sys_filename(const std::string& filename,
                              const FilenameCallback& user_filename_func={},
                              const FilenameCallback& system_filename_func={});

// Copy fn to dir with a counted filename. Used for debugging
// lm file corruptions.
bool backup_file(const std::string& fn, const std::string& dir);

class TempDir
{
    public:
        TempDir(const std::string& prefix="tmpdir_");
        ~TempDir();

    public:
        std::string dir;
};

#endif // FILE_HELPERS_H
