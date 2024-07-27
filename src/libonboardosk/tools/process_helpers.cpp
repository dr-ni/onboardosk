
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <unistd.h>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "tools/file_helpers.h"
#include "tools/path_helpers.h"
#include "tools/string_helpers.h"

#include "../exception.h"
#include "process_helpers.h"


std::string exec_cmd(const std::string& cmd)
{
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe)
        throw ProcessException(sstr()
            << "failed to execute cmd '" << cmd << "'");

    while (!feof(pipe.get()))
    {
        std::array<char, 256> buf;
        if (fgets(buf.data(), buf.size(), pipe.get()) != NULL)
            result += buf.data();
    }

    return result;
}

bool run_cmd(const std::vector<std::string>& argv)
{
    std::vector<const char*> a;
    for (auto& v : argv)
        a.emplace_back(v.c_str());
    a.emplace_back(nullptr);

    // double fork to prevent zombies
    int pid = fork();
    if (pid < 0)  // fork failed?
        return false;
    if (pid > 0)
        // grand parent
        return true;

    // first child
    int pid_child = fork();
    if (pid_child < 0)  // fork failed?
        exit(0);
    if (pid_child > 0)
        // parent
        exit(0);

    // second child
    if (execvp(a[0], const_cast<char* const*>(&a[0])) < 0)
        exit(1);

    return false;
}


std::vector<std::string> Process::get_cmdline(int pid)
{
    std::string cmdline;
    auto path = fs::path("/proc") / std::to_string(pid) / "cmdline";
    read_file(cmdline, path);
    return split(cmdline, '\0');
}

std::string Process::get_process_name(int pid)
{
    auto cmdline = Process::get_cmdline(pid);
    if (!cmdline.empty())
        return get_basename(cmdline[0]);
    return {};
}

std::vector<std::string> Process::get_launch_process_cmdline()
{
    // "The getppid() function shall always be successful
    // and no return value is reserved to indicate an error."
    int ppid = getppid();
    return  Process::get_cmdline(ppid);
}

bool Process::was_launched_by(const std::string& process_name)
{
    std::string cmdline = join(Process::get_launch_process_cmdline(), " ");
    return contains(cmdline, process_name);  // crude, improve as necessary
}
