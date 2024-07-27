#ifndef XDGDIRS_H
#define XDGDIRS_H

#include <string>
#include <vector>

class Logger;

// Build paths compliant with XDG Base Directory Specification.
// http:://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
class XDGDirs
{
    public:
        XDGDirs(Logger* logger);
        ~XDGDirs();

        // User specific config directory.
        std::string get_config_home(const std::string& file={});

        // Config directories ordered by preference.
        static void get_config_dirs(std::vector<std::string> paths);

        void get_all_config_dirs(std::vector<std::string>& paths_out,
                                 const std::string& file={});

        // Find file in all config directories, highest priority first.
        void find_config_files(std::vector<std::string>& paths_out,
                               const std::string& file);

        // Find file of highest priority
        std::string find_config_file(const std::string& file);

        // User specific data directory.
        std::string get_data_home(const std::string& file={});

        // Data directories ordered by preference.
        static void get_data_dirs(std::vector<std::string> paths_out);

        void get_all_data_dirs(std::vector<std::string>& paths_out,
                               const std::string& file={});

        // Find file in all data directories, highest priority first.
        void find_data_files(std::vector<std::string>& paths_out,
                             const std::string& file);

        // Find file of highest priority
        const std::string find_data_file(const std::string& file);

        // If necessary create user XDG directory.
        // Raises OSError.
        bool assure_user_dir_exists(const std::string& path);

    private:
        Logger* logger() {return m_logger;}

    private:
        Logger* m_logger{};
};

#endif // XDGDIRS_H
