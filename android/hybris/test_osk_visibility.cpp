/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include <ubuntu/ui/ubuntu_ui_session_service.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: %s {show|hide}", argv[0]);
        return 1;
    }

    if (strcmp("show", argv[1]) == 0)
        ubuntu_ui_report_osk_visible(0, 0, 0, 0);
    else if (strcmp("hide", argv[1]) == 0)
        ubuntu_ui_report_osk_invisible();
    
    return 0;
}
