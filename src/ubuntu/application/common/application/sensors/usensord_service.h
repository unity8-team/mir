/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *      Author: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <string>
#include <vector>
#include <chrono>

namespace ubuntu
{
namespace application
{
namespace sensors
{

struct USensorD
{
    static std::string& name()
    {
        static std::string s = "com.canonical.usensord";
        return s;
    }

    struct Haptic
    {
        static std::string name()
        {
            static std::string s = "com.canonical.usensord.haptic";
            return s;
        }
    
        struct Vibrate
        {
            static std::string name()
            {
                static std::string s = "Vibrate";
                return s;
            }
    
            static const std::chrono::milliseconds default_timeout() { return std::chrono::seconds{1}; }
    
            typedef Haptic Interface;
            typedef void ResultType;
            typedef std::uint32_t ArgumentType;
        };
    
        struct VibratePattern
        {
            static std::string name()
            {
                static std::string s = "VibratePattern";
                return s;
            }
     
            static const std::chrono::milliseconds default_timeout() { return std::chrono::seconds{1}; }
    
            typedef Haptic Interface;
            typedef void ResultType;
            typedef std::tuple<std::vector<std::uint32_t>, std::uint32_t> ArgumentType;
        };
    };
};

}
}
}
