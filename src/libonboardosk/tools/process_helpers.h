#ifndef PROCESS_HELPERS_H
#define PROCESS_HELPERS_H

#include <string>
#include <vector>

// execute synchronously and return stdout
std::string exec_cmd(const std::string& cmd);

// run and disown
bool run_cmd(const std::vector<std::string>& argv);


class Process
{
    public:
        // Returns the command line for process id pid
        static std::vector<std::string> get_cmdline(int pid);

        static std::string get_process_name(int pid);

        // Checks if this process was launched by <process_name>
        static std::vector<std::string> get_launch_process_cmdline();

        // Checks if this process was launched by <process_name>
        static bool was_launched_by(const std::string& process_name);
};

#endif // PROCESS_HELPERS_H
