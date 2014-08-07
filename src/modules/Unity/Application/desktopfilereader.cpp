/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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

// local
#include "desktopfilereader.h"
#include "logging.h"

// Qt
#include <QFile>

namespace qtmir
{

DesktopFileReader::Factory::Factory()
{
}

DesktopFileReader::Factory::~Factory()
{
}

DesktopFileReader* DesktopFileReader::Factory::createInstance(const QString &appId, const QFileInfo& fi)
{
    return new DesktopFileReader(appId, fi);
}

// Retrieves the size of an array at compile time.
#define ARRAY_SIZE(a) \
    ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

DesktopFileReader::DesktopFileReader(const QString &appId, const QFileInfo &desktopFile)
    : appId_(appId)
    , entries_(DesktopFileReader::kNumberOfEntries, "")
{
    qCDebug(QTMIR_APPLICATIONS) << "DesktopFileReader::DesktopFileReader - this=" << this << "appId=" << appId;

    file_ = desktopFile.absoluteFilePath();
    loaded_ = loadDesktopFile(file_);
}

DesktopFileReader::~DesktopFileReader()
{
    qCDebug(QTMIR_APPLICATIONS) << "DesktopFileReader::~DesktopFileReader";
    entries_.clear();
}

bool DesktopFileReader::loadDesktopFile(QString desktopFile)
{
    qCDebug(QTMIR_APPLICATIONS) << "DesktopFileReader::loadDesktopFile - this=" << this << "desktopFile=" << desktopFile;

    if (this->file().isNull() || this->file().isEmpty()) {
        qCritical() << "No desktop file found for appId:" << appId_;
        return false;
    }

    Q_ASSERT(desktopFile != NULL);
    const struct { const char* const name; int size; unsigned int flag; } kEntryNames[] = {
        { "Name=", sizeof("Name=") - 1, 1 << DesktopFileReader::kNameIndex },
        { "Comment=", sizeof("Comment=") - 1, 1 << DesktopFileReader::kCommentIndex },
        { "Icon=", sizeof("Icon=") - 1, 1 << DesktopFileReader::kIconIndex },
        { "Exec=", sizeof("Exec=") - 1, 1 << DesktopFileReader::kExecIndex },
        { "Path=", sizeof("Path=") - 1, 1 << DesktopFileReader::kPathIndex },
        { "X-Ubuntu-StageHint=", sizeof("X-Ubuntu-StageHint=") - 1, 1 << DesktopFileReader::kStageHintIndex },
        { "X-Ubuntu-Splash-Title=", sizeof("X-Ubuntu-Splash-Title=") - 1, 1 << DesktopFileReader::kSplashTitleIndex },
        { "X-Ubuntu-Splash-Image=", sizeof("X-Ubuntu-Splash-Image=") - 1, 1 << DesktopFileReader::kSplashImageIndex },
        { "X-Ubuntu-Splash-Show-Header=", sizeof("X-Ubuntu-Splash-Show-Header=") - 1, 1 << DesktopFileReader::kSplashShowHeaderIndex },
        { "X-Ubuntu-Splash-Color=", sizeof("X-Ubuntu-Splash-Color=") - 1, 1 << DesktopFileReader::kSplashColorIndex },
        { "X-Ubuntu-Splash-Color-Header=", sizeof("X-Ubuntu-Splash-Color-Header=") - 1, 1 << DesktopFileReader::kSplashColorHeaderIndex },
        { "X-Ubuntu-Splash-Color-Footer=", sizeof("X-Ubuntu-Splash-Color-Footer=") - 1, 1 << DesktopFileReader::kSplashColorFooterIndex }
    };
    const unsigned int kAllEntriesMask =
            (1 << DesktopFileReader::kNameIndex) | (1 << DesktopFileReader::kCommentIndex)
            | (1 << DesktopFileReader::kIconIndex) | (1 << DesktopFileReader::kExecIndex)
            | (1 << DesktopFileReader::kPathIndex) | (1 << DesktopFileReader::kStageHintIndex)
            | (1 << DesktopFileReader::kSplashTitleIndex) | (1 << DesktopFileReader::kSplashImageIndex)
            | (1 << DesktopFileReader::kSplashShowHeaderIndex) | (1 << DesktopFileReader::kSplashColorIndex)
            | (1 << DesktopFileReader::kSplashColorHeaderIndex) | (1 << DesktopFileReader::kSplashColorFooterIndex);
    const unsigned int kMandatoryEntriesMask =
            (1 << DesktopFileReader::kNameIndex) | (1 << DesktopFileReader::kIconIndex)
            | (1 << DesktopFileReader::kExecIndex);
    const int kEntriesCount = ARRAY_SIZE(kEntryNames);
    const int kBufferSize = 256;
    static char buffer[kBufferSize];
    QFile file(desktopFile);

    // Open file.
    if (!file.open(QFile::ReadOnly | QIODevice::Text)) {
        qWarning() << "Can't open file:" << file.errorString();
        return false;
    }

    // Validate "magic key" (standard group header).
    if (file.readLine(buffer, kBufferSize) != -1) {
        if (strncmp(buffer, "[Desktop Entry]", sizeof("[Desktop Entry]") - 1)) {
            qWarning() << "not a desktop file, unable to read it";
            return false;
        }
    }

    int length;
    unsigned int entryFlags = 0;
    while ((length = file.readLine(buffer, kBufferSize)) != -1) {
        // Skip empty lines.
        if (length > 1) {
            // Stop when reaching unsupported next group header.
            if (buffer[0] == '[') {
                qWarning() << "reached next group header, leaving loop";
                break;
            }
            // Lookup entries ignoring duplicates if any.
            for (int i = 0; i < kEntriesCount; i++) {
                if (!strncmp(buffer, kEntryNames[i].name, kEntryNames[i].size)) {
                    if (~entryFlags & kEntryNames[i].flag) {
                        buffer[length-1] = '\0';
                        entries_[i] = QString::fromUtf8(&buffer[kEntryNames[i].size]);
                        entryFlags |= kEntryNames[i].flag;
                        break;
                    }
                }
            }
            // Stop when matching the right number of entries.
            if (entryFlags == kAllEntriesMask) {
                break;
            }
        }
    }

    // Check that the mandatory entries are set.
    if ((entryFlags & kMandatoryEntriesMask) == kMandatoryEntriesMask) {
        qDebug("loaded desktop file with name='%s', comment='%s', icon='%s', exec='%s', path='%s', stagehint='%s'",
                qPrintable(entries_[DesktopFileReader::kNameIndex]),
                qPrintable(entries_[DesktopFileReader::kCommentIndex]),
                qPrintable(entries_[DesktopFileReader::kIconIndex]),
                qPrintable(entries_[DesktopFileReader::kExecIndex]),
                qPrintable(entries_[DesktopFileReader::kPathIndex]),
                qPrintable(entries_[DesktopFileReader::kStageHintIndex]));
        return true;
    } else {
        qWarning() << "not a valid desktop file, missing mandatory entries in the standard group header";
        return false;
    }
}

} // namespace qtmir
