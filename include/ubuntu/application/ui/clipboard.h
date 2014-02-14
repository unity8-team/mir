/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 *              Thomas Voß <thomas.voss@canonical.com>           
 */

#ifndef UBUNTU_APPLICATION_UI_CLIPBOARD_H_
#define UBUNTU_APPLICATION_UI_CLIPBOARD_H_

#include <ubuntu/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

    UBUNTU_DLL_PUBLIC void
    ua_ui_set_clipboard_content(
        void* data,
        size_t size);

    /*
        Memory pointed by data is owned by Ubuntu Application API.
        Caller is expected to not attempt to modify or delete it.
        It's also not guaranteed to remain valid after any other call is made
        to the Ubuntu Application API.
    */
    UBUNTU_DLL_PUBLIC void
    ua_ui_get_clipboard_content(
        void** data,
        size_t* size);

#ifdef __cplusplus
}
#endif

#endif /* UBUNTU_APPLICATION_UI_CLIPBOARD_H_ */
