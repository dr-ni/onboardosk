#ifndef COMMANDLINEOPTIONS_H
#define COMMANDLINEOPTIONS_H

#include <string>
#include <vector>


class CommandLineOptions
{
    public:
        ~CommandLineOptions();
        bool parse(const std::vector<std::string>& args);

    public:
        std::string theme;
        bool keep_aspect_ratio{false};
        bool allow_multiple_instances{false};
        bool log_learning{false};

        std::vector<std::string> remaining_args;
};

#endif // COMMANDLINEOPTIONS_H
