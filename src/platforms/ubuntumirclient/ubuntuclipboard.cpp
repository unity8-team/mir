/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ubuntuclipboard.h"
#include <logging.h>

#include <QtCore/QMimeData>
#include <QtCore/QStringList>

// Platform API
#include <ubuntu/application/ui/clipboard.h>

// FIXME(loicm) The clipboard data format is not defined by Ubuntu Platform API
//     which makes it impossible to have non-Qt applications communicate with Qt
//     applications through the clipboard API. The solution would be to have
//     Ubuntu Platform define the data format or propose an API that supports
//     embedding different mime types in the clipboard.

// Data format:
//   number of mime types      (4 bytes)
//   data layout               (16 bytes * number of mime types)
//     mime type string offset (4 bytes)
//     mime type string size   (4 bytes)
//     data offset             (4 bytes)
//     data size               (4 bytes)
//   data                      (n bytes)

const int maxFormatsCount = 16;
const int maxBufferSize = 4 * 1024 * 1024;  // 4 Mb

UbuntuClipboard::UbuntuClipboard()
    : mMimeData(new QMimeData)
{
}

UbuntuClipboard::~UbuntuClipboard()
{
    delete mMimeData;
}

QMimeData* UbuntuClipboard::mimeData(QClipboard::Mode mode)
{
    Q_UNUSED(mode);
    // Get clipboard data.
    void* data = NULL;
    size_t size = 0;
    ua_ui_get_clipboard_content(&data, &size);

    // Deserialize, update and return mime data taking care of incorrectly
    // formatted input.
    mMimeData->clear();
    if (static_cast<size_t>(size) > sizeof(int)  // Should be at least that big to read the count.
            && data != NULL) {
        const char* const buffer = reinterpret_cast<char*>(data);
        const int* const header = reinterpret_cast<int*>(data);
        const int count = qMin(header[0], maxFormatsCount);
        for (int i = 0; i < count; i++) {
            const unsigned int formatOffset = header[i*4+1];
            const unsigned int formatSize = header[i*4+2];
            const unsigned int dataOffset = header[i*4+3];
            const unsigned int dataSize = header[i*4+4];
            if (formatOffset + formatSize <= size && dataOffset + dataSize <= size) {
                mMimeData->setData(QString(&buffer[formatOffset]),
                        QByteArray(&buffer[dataOffset], dataSize));
            }
        }
    }
    return mMimeData;
}

void UbuntuClipboard::setMimeData(QMimeData* mimeData, QClipboard::Mode mode)
{
    Q_UNUSED(mode);
    if (mimeData == NULL) {
        ua_ui_set_clipboard_content(NULL, 0);
        return;
    }

    const QStringList formats = mimeData->formats();
    const int count = qMin(formats.size(), maxFormatsCount);
    const int headerSize = sizeof(int) + count * 4 * sizeof(int);
    int bufferSize = headerSize;
    char* buffer;

    // Get the buffer size considering the header size, the NULL-terminated
    // formats and the non NULL-terminated data.
    for (int i = 0; i < count; i++)
        bufferSize += formats[i].size() + 1 + mimeData->data(formats[i]).size();
    // FIXME(loicm) Implement max buffer size limitation.
    // FIXME(loicm) Remove ASSERT before release.
    ASSERT(bufferSize <= maxBufferSize);

    // Serialize data.
    buffer = new char[bufferSize];
    int* header = reinterpret_cast<int*>(buffer);
    int offset = headerSize;
    header[0] = count;
    for (int i = 0; i < count; i++) {
        const int formatOffset = offset;
        const int formatSize = formats[i].size() + 1;
        const int dataOffset = offset + formatSize;
        const int dataSize = mimeData->data(formats[i]).size();
        memcpy(&buffer[formatOffset], formats[i].toLatin1().data(), formatSize);
        memcpy(&buffer[dataOffset], mimeData->data(formats[i]).data(), dataSize);
        header[i*4+1] = formatOffset;
        header[i*4+2] = formatSize;
        header[i*4+3] = dataOffset;
        header[i*4+4] = dataSize;
        offset += formatSize + dataSize;
    }

    // Set clipboard content.
    ua_ui_set_clipboard_content(reinterpret_cast<void*>(buffer), bufferSize);
    delete [] buffer;
}

bool UbuntuClipboard::supportsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard;
}

bool UbuntuClipboard::ownsMode(QClipboard::Mode mode) const
{
    Q_UNUSED(mode);
    return false;
}
