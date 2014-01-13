/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/options/program_option.h"

#include <boost/program_options/parsers.hpp>

#include <fstream>
#include <iostream>

namespace mo = mir::options;
namespace po = boost::program_options;


mo::ProgramOption::ProgramOption()
{
}

void mo::ProgramOption::parse_arguments(
    po::options_description const& desc,
    int argc,
    char const* argv[])
{
    // TODO: Don't allow unregistered options, once we allow better overriding of option parsing
    po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), options);
    po::notify(options);
}

void mo::ProgramOption::parse_environment(
    po::options_description const& desc,
    char const* prefix)
{
    auto parsed_options = po::parse_environment(
        desc,
        [=](std::string const& from) -> std::string
        {
             auto const sizeof_prefix = strlen(prefix);

             if (from.length() < sizeof_prefix || 0 != from.find(prefix)) return std::string();

             std::string result(from, sizeof_prefix);

             for(auto& ch : result)
             {
                 if (ch == '_') ch = '-';
                 else ch = tolower(ch);
             }

             return result;
        });

    po::store(parsed_options, options);
}

void mo::ProgramOption::parse_file(
    po::options_description const& config_file_desc,
    std::string const& name)
{
    std::string config_roots;

    if (auto config_home = getenv("XDG_CONFIG_HOME"))
        (config_roots = config_home) += ":";
    else if (auto home = getenv("HOME"))
        (config_roots = home) += "/.config:";

    if (auto config_dirs = getenv("XDG_CONFIG_DIRS"))
        config_roots += config_dirs;
    else
        config_roots += "/etc/xdg";

    std::istringstream config_stream(config_roots);

    /* Read options from config files */
    for (std::string config_root; getline(config_stream, config_root, ':');)
    {
        auto const& filename = config_root + "/" + name;

        try
        {
            std::ifstream file(filename);
            po::store(po::parse_config_file(file, config_file_desc, true), options);
        }
        catch (const po::error& error)
        {
            std::cerr << "ERROR in " << filename << ": " << error.what() << std::endl;
        }
    }

    po::notify(options);
}

bool mo::ProgramOption::is_set(char const* name) const
{
    return options.count(name);
}


bool mo::ProgramOption::get(char const* name, bool default_) const
{
    if (options.count(name))
    {
        return options[name].as<bool>();
    }

    return default_;
}

std::string mo::ProgramOption::get(char const* name, char const* default_) const
{
    if (options.count(name))
    {
        return options[name].as<std::string>();
    }

    return default_;
}

int mo::ProgramOption::get(char const* name, int default_) const
{
    if (options.count(name))
    {
        return options[name].as<int>();
    }

    return default_;
}

