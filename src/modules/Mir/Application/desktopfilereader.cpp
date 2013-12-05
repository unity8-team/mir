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

// local
#include "desktopfilereader.h"

// unity-mir
#include "logging.h"

// Qt
#include <QFile>
#include <QDir>
#include <QStandardPaths>

// Retrieves the size of an array at compile time.
#define ARRAY_SIZE(a) \
    ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

DesktopFileReader::DesktopFileReader(const QString &appId)
    : appId_(appId)
    , entries_(DesktopFileReader::kNumberOfEntries, "")
{
    DLOG("DesktopFileReader::DesktopFileReader (this=%p, appId='%s')", this, appId.toLatin1().data());
    DASSERT(appId != NULL);

    file_ = findDesktopFile(appId);
    loaded_ = loadDesktopFile(file_);
}

DesktopFileReader::DesktopFileReader(const QFileInfo &desktopFile)
    : entries_(DesktopFileReader::kNumberOfEntries, "")
{
    DLOG("DesktopFileReader::DesktopFileReader (this=%p, desktopFile='%s')", this, desktopFile.absoluteFilePath().toLatin1().data());
    DASSERT(desktopFile.exists());

    appId_ = desktopFile.completeBaseName();
    file_ = desktopFile.absoluteFilePath();
    loaded_ = loadDesktopFile(file_);
}

DesktopFileReader::~DesktopFileReader()
{
    DLOG("DesktopFileReader::~DesktopFileReader");
    entries_.clear();
}


QString DesktopFileReader::findDesktopFile(const QString &appId) const
{
    DLOG("DesktopFileReader::findDesktopFile (appId='%s')", appId.toLatin1().data());

    int dashPos = -1;
    QString helper = appId;
    QString desktopFile;

    do {
        if (dashPos != -1) {
            helper = helper.replace(dashPos, 1, '/');
        }

        desktopFile = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QString("%1.desktop").arg(helper));
        if (!desktopFile.isEmpty()) return desktopFile;

        dashPos = helper.indexOf("-");
    } while (dashPos != -1);

    return QString();
}


bool DesktopFileReader::loadDesktopFile(QString desktopFile)
{
    DLOG("DesktopFileReader::loadDesktopFile (this=%p, desktopFile='%s')",
         this, desktopFile.toLatin1().data());

    if (this->file().isNull() || this->file().isEmpty()) {
        DLOG("No desktop file found for appId: %s", qPrintable(appId_));
        return false;
    }

    DASSERT(desktopFile != NULL);
    const struct { const char* const name; int size; unsigned int flag; } kEntryNames[] = {
        { "Name=", sizeof("Name=") - 1, 1 << DesktopFileReader::kNameIndex },
        { "Comment=", sizeof("Comment=") - 1, 1 << DesktopFileReader::kCommentIndex },
        { "Icon=", sizeof("Icon=") - 1, 1 << DesktopFileReader::kIconIndex },
        { "Exec=", sizeof("Exec=") - 1, 1 << DesktopFileReader::kExecIndex },
        { "Path=", sizeof("Path=") - 1, 1 << DesktopFileReader::kPathIndex },
        { "X-Ubuntu-StageHint=", sizeof("X-Ubuntu-StageHint=") - 1, 1 << DesktopFileReader::kStageHintIndex }
    };
    const unsigned int kAllEntriesMask =
            (1 << DesktopFileReader::kNameIndex) | (1 << DesktopFileReader::kCommentIndex)
            | (1 << DesktopFileReader::kIconIndex) | (1 << DesktopFileReader::kExecIndex)
            | (1 << DesktopFileReader::kPathIndex) | (1 << DesktopFileReader::kStageHintIndex);
    const unsigned int kMandatoryEntriesMask =
            (1 << DesktopFileReader::kNameIndex) | (1 << DesktopFileReader::kIconIndex)
            | (1 << DesktopFileReader::kExecIndex);
    const int kEntriesCount = ARRAY_SIZE(kEntryNames);
    const int kBufferSize = 256;
    static char buffer[kBufferSize];
    QFile file(desktopFile);

    // Open file.
    if (!file.open(QFile::ReadOnly | QIODevice::Text)) {
        DLOG("can't open file: %s", file.errorString().toLatin1().data());
        return false;
    }

    // Validate "magic key" (standard group header).
    if (file.readLine(buffer, kBufferSize) != -1) {
        if (strncmp(buffer, "[Desktop Entry]", sizeof("[Desktop Entry]") - 1)) {
            DLOG("not a desktop file");
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
                DLOG("reached next group header, leaving loop");
                break;
            }
            // Lookup entries ignoring duplicates if any.
            for (int i = 0; i < kEntriesCount; i++) {
                if (!strncmp(buffer, kEntryNames[i].name, kEntryNames[i].size)) {
                    if (~entryFlags & kEntryNames[i].flag) {
                        buffer[length-1] = '\0';
                        entries_[i] = QString::fromLatin1(&buffer[kEntryNames[i].size]);
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
        DLOG("loaded desktop file with name='%s', comment='%s', icon='%s', exec='%s', path='%s', stagehint='%s'",
             entries_[DesktopFileReader::kNameIndex].toLatin1().data(),
                entries_[DesktopFileReader::kCommentIndex].toLatin1().data(),
                entries_[DesktopFileReader::kIconIndex].toLatin1().data(),
                entries_[DesktopFileReader::kExecIndex].toLatin1().data(),
                entries_[DesktopFileReader::kPathIndex].toLatin1().data(),
                entries_[DesktopFileReader::kStageHintIndex].toLatin1().data());
        return true;
    } else {
        DLOG("not a valid desktop file, missing mandatory entries in the standard group header");
        return false;
    }
}
