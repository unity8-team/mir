/*
 * Copyright (C) 2014 Canonical, Ltd.
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
 *
 * Author: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#include "clipboard.h"
#include "logging.h"

// C++ std lib
#include <utility>

#include <QDBusConnection>
#include <QDBusError>
#include <QMimeData>

Q_LOGGING_CATEGORY(QTMIR_CLIPBOARD, "qtmir.clipboard")

// FIXME(loicm) The clipboard data format is not defined by Ubuntu Platform API
//     which makes it impossible to have non-Qt applications communicate with Qt
//     applications through the clipboard API. The solution would be to have
//     Ubuntu Platform define the data format or propose an API that supports
//     embedding different mime types in the clipboard.

// Data format:
//   number of mime types      (sizeof(int))
//   data layout               ((4 * sizeof(int)) * number of mime types)
//     mime type string offset (sizeof(int))
//     mime type string size   (sizeof(int))
//     data offset             (sizeof(int))
//     data size               (sizeof(int))
//   data                      (n bytes)

namespace {

const int maxFormatsCount = 16;

}

namespace qtmir {

QByteArray serializeMimeData(QMimeData *mimeData)
{
    const QStringList formats = mimeData->formats();
    const int formatCount = qMin(formats.size(), maxFormatsCount);
    const int headerSize = sizeof(int) + (formatCount * 4 * sizeof(int));
    int bufferSize = headerSize;

    for (int i = 0; i < formatCount; i++)
        bufferSize += formats[i].size() + mimeData->data(formats[i]).size();

    // Serialize data.
    QByteArray serializedMimeData(bufferSize, 0 /* char to fill with */);
    {
        char *buffer = serializedMimeData.data();
        int* header = reinterpret_cast<int*>(serializedMimeData.data());
        int offset = headerSize;
        header[0] = formatCount;
        for (int i = 0; i < formatCount; i++) {
            const int formatOffset = offset;
            const int formatSize = formats[i].size();
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
    }

    return serializedMimeData;
}

QMimeData *deserializeMimeData(const QByteArray &serializedMimeData)
{
    if (static_cast<std::size_t>(serializedMimeData.size()) < sizeof(int)) {
        // Data is invalid
        return nullptr;
    }

    QMimeData *mimeData = new QMimeData;

    const char* const buffer = serializedMimeData.constData();
    const int* const header = reinterpret_cast<const int*>(serializedMimeData.constData());

    const int count = qMin(header[0], maxFormatsCount);

    for (int i = 0; i < count; i++) {
        const int formatOffset = header[i*4+1];
        const int formatSize = header[i*4+2];
        const int dataOffset = header[i*4+3];
        const int dataSize = header[i*4+4];

        if (formatOffset + formatSize <= serializedMimeData.size()
                && dataOffset + dataSize <= serializedMimeData.size()) {

            QString mimeType = QString::fromLatin1(&buffer[formatOffset], formatSize);
            QByteArray mimeDataBytes(&buffer[dataOffset], dataSize);

            mimeData->setData(mimeType, mimeDataBytes);
        }
    }

    return mimeData;
}

/************************************ DBusClipboard *****************************************/

bool DBusClipboard::skipDBusRegistration = false;

DBusClipboard::DBusClipboard(QObject *parent)
    : QObject(parent)
{
    if (!skipDBusRegistration) {
        performDBusRegistration();
    }
}

void DBusClipboard::setContents(QByteArray newContents)
{
    setContentsHelper(std::move(newContents));
}

void DBusClipboard::SetContents(QByteArray newContents)
{
    qCDebug(QTMIR_CLIPBOARD, "D-Bus SetContents - %d bytes", newContents.size());

    if (setContentsHelper(std::move(newContents))) {
        Q_EMIT contentsChangedRemotely();
    }
}

bool DBusClipboard::setContentsHelper(QByteArray newContents)
{
    if (newContents.size() > maxContentsSize) {
        qCWarning(QTMIR_CLIPBOARD, "D-Bus clipboard refused the new contents (%d bytes) as they're"
                " bigger than the maximum allowed size of %d bytes.",
                newContents.size(), maxContentsSize);
        return false;
    }

    if (newContents != m_contents) {
        m_contents = std::move(newContents);
        Q_EMIT ContentsChanged(m_contents);
        return true;
    } else {
        return false;
    }
}

QByteArray DBusClipboard::GetContents() const
{
    qCDebug(QTMIR_CLIPBOARD, "D-Bus GetContents - returning %d bytes", m_contents.size());
    return m_contents;
}

void DBusClipboard::performDBusRegistration()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    const char *serviceName = "com.canonical.QtMir";
    const char *objectName = "/com/canonical/QtMir/Clipboard";

    bool serviceOk = connection.registerService(serviceName);
    if (!serviceOk) {
        QDBusError error = connection.lastError();
        QString errorMessage;
        if (error.isValid()) {
            errorMessage = error.message();
        }
        qCCritical(QTMIR_CLIPBOARD, "Failed to register service %s. %s", serviceName, qPrintable(errorMessage));
    }

    bool objectOk = connection.registerObject(objectName, this,
                              QDBusConnection::ExportScriptableSignals
                              | QDBusConnection::ExportScriptableSlots);
    if (!objectOk) {
        QDBusError error = connection.lastError();
        QString errorMessage;
        if (error.isValid()) {
            errorMessage = error.message();
        }
        qCCritical(QTMIR_CLIPBOARD, "Failed to register object %s. %s", objectName, qPrintable(errorMessage));
    }

    if (serviceOk && objectOk) {
        qCDebug(QTMIR_CLIPBOARD, "D-Bus registration successful.");
    }
}

/************************************ Clipboard *****************************************/

Clipboard::Clipboard(QObject *parent)
    : QObject(parent)
    , m_dbusClipboard(nullptr)
{
}

QMimeData *Clipboard::mimeData(QClipboard::Mode mode)
{
    if (mode == QClipboard::Clipboard) {
        return QPlatformClipboard::mimeData(mode);
    } else {
        return nullptr;
    }
}

void Clipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    if (m_dbusClipboard) {
        QByteArray serializedMimeData = serializeMimeData(data);
        m_dbusClipboard->setContents(std::move(serializedMimeData));
    }

    QPlatformClipboard::setMimeData(data, mode);
}

void Clipboard::setupDBusService()
{
    Q_ASSERT(!m_dbusClipboard);

    m_dbusClipboard = new DBusClipboard(this);

    connect(m_dbusClipboard, &DBusClipboard::contentsChangedRemotely,
            this, &Clipboard::setMimeDataWithDBusClibpboardContents);
}

void Clipboard::setMimeDataWithDBusClibpboardContents()
{
    Q_ASSERT(m_dbusClipboard);
    QMimeData *newMimeData = deserializeMimeData(m_dbusClipboard->contents());
    if (newMimeData) {
        // Don't call Clipboard::setMimeData as it will also propagate the change
        // to the D-Bus clipboard, which doesn't make sense here as we're doing
        // the other way round (propagating the D-Bus clipboard change to the local
        // clipboard).
        QPlatformClipboard::setMimeData(newMimeData, QClipboard::Clipboard);
    } else {
        qCWarning(QTMIR_CLIPBOARD, "Failed to deserialize D-Bus clipboard contents (%d bytes)",
                m_dbusClipboard->contents().size());
    }
}

} // namespace qtmir

