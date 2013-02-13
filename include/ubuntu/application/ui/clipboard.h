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
#ifndef UBUNTU_APPLICATION_UI_CLIPBOARD_H_
#define UBUNTU_APPLICATION_UI_CLIPBOARD_H_

#include "ubuntu/platform/shared_ptr.h"

namespace ubuntu
{
namespace application
{
namespace ui
{
/** Models a system-wide clipboard. 
 * \deprecated This is a temporary solution and is likely to be removed or subject to significant changes in upcoming revisions.
 */
class Clipboard : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Clipboard> Ptr;

    struct Content
    {
        static const int MAX_MIME_TYPE_SIZE=32;

        Content() : data(NULL),
                    data_size(0)
        {
        }
        
        Content(const char* mime,
                void* _data,
                size_t size) :   data(malloc(size)),
                                 data_size(size)
        {
            memcpy(mime_type, mime, MAX_MIME_TYPE_SIZE);
            memcpy(data, _data, size);
        }

        Content(const Content& rhs) : data(malloc(rhs.data_size)),
                                      data_size(rhs.data_size)
        {
            memcpy(mime_type, rhs.mime_type, MAX_MIME_TYPE_SIZE);
            memcpy(data, rhs.data, data_size);
        }

        Content& operator=(const Content& rhs)
        {
            memcpy(mime_type, rhs.mime_type, MAX_MIME_TYPE_SIZE);
            data = realloc(data, rhs.data_size);
            memcpy(data, rhs.data, data_size);
            data_size = rhs.data_size;

            return *this;
        }

        char mime_type[MAX_MIME_TYPE_SIZE];
        void* data;
        size_t data_size;
    };

    virtual void set_content(const Content& content) = 0;
    virtual Content get_content() = 0;

protected:
    Clipboard() {}
    virtual ~Clipboard() {}

    Clipboard(const Clipboard&) = delete;
    Clipboard& operator=(const Clipboard&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_CLIPBOARD_H_
