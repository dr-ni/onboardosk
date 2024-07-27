
#include <algorithm>
#include <getopt.h>
#include <iostream>
#include <sstream>
#include <vector>

#include "commandlineoptions.h"


CommandLineOptions::~CommandLineOptions()
{
}

bool CommandLineOptions::parse(const std::vector<std::string>& args)
{
    bool ret = true;
    int option_index = 0;

    std::vector<const char*> v;
    std::transform(args.begin(), args.end(), v.begin(), [](const std::string& s) {
        return s.c_str();
    });

    while (true)
    {
        using std::endl;
        std::string app_name = args.empty() ? "onboard" : args[0];
        std::stringstream usage;
        usage << "Usage: " << app_name << " [options]" << endl
              << "" << endl
              << "Options:" << endl
              << " -h, --help            show this help message and exit" << endl
              << "" << endl
              << "  General Options:" << endl
              << "    -l LAYOUT, --layout=LAYOUT" << endl
              << "                        Layout file (.onboard) or name" << endl
              << "    -t THEME, --theme=THEME" << endl
              << "                        Theme file (.theme) or name" << endl
              << "    -s SIZE, --size=SIZE" << endl
              << "                        Window size, width x height" << endl
              << "    -x X                Window x position" << endl
              << "    -y Y                Window y position" << endl
              << "" << endl
              << "  Advanced Options:" << endl
              << "    -e, --xid           Start in XEmbed mode, e.g. for gnome-screensaver" << endl
              << "    -m, --allow-multiple-instances" << endl
              << "                        Allow multiple Onboard instances" << endl
              << "    --not-show-in=DESKTOPS" << endl
              << "                        Silently fail to start in the given desktop" << endl
              << "                        environments. DESKTOPS is a comma-separated list of" << endl
              << "                        XDG desktop names, e.g. GNOME for GNOME Shell." << endl
              << "    -D STARTUP_DELAY, --startup-delay=STARTUP_DELAY" << endl
              << "                        Delay showing the initial keyboard window by" << endl
              << "                        STARTUP_DELAY seconds (default 0.0)." << endl
              << "    -a, --keep-aspect   Keep aspect ratio when resizing the window" << endl
              << "    -q QUIRKS_NAME, --quirks=QUIRKS_NAME" << endl
              << "                        Override auto-detection and manually select quirks" << endl
              << "                        QUIRKS={metacity|compiz|mutter}" << endl
              << "" << endl
              << "  Debug Options:" << endl
              << "    -d ARG, --debug=ARG" << endl
              << "                        Set logging level or range of levels" << endl
              << "                        ARG=MIN_LEVEL[-MAX_LEVEL]" << endl
              << "                        LEVEL={all|event|atspi|debug|info|warning|error|" << endl
              << "                        critical}" << endl
              << "    --launched-by=ARG   Simulate being launched by certain XEmbed sockets." << endl
              << "                        Use this together with option --xid." << endl
              << "                        ARG={unity-greeter|gnome-screen-saver}" << endl
              << "    -g, --log-learning  log all learned text; off by default" << endl
              ;

        // ":" = required argument
        // "::" = optional argument
        const char* short_options = "hl:m";

        static struct option long_options[] = {
            {"help",  no_argument, 0, 'h'},
            {"layout",  required_argument, 0, 'l'},
            {"allow-multiple-instances", no_argument, 0, 'm'},
            {0, 0, 0, 0},
        };

        int opt = getopt_long (static_cast<int>(v.size()),
                               const_cast<char* const*>(&v[0]),
                               short_options, long_options, &option_index);

        if (opt == -1) // no more options?
            break;

        switch (opt)
        {
            case 'l':
                this->theme = optarg;
                break;

            case 'm':
                this->allow_multiple_instances = true;
                break;

            case 'h':
            case '?':
                std::cout << usage.str();
                ret = false;
                break;

            default:
                ret = false;
                break;
        }
    }

    std::copy(args.begin() + optind, args.end(), this->remaining_args.begin());

    return ret;
}
