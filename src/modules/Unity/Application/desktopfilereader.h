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

#ifndef DESKTOPFILEREADER_H
#define DESKTOPFILEREADER_H

#include <QString>
#include <QVector>
#include <QFileInfo>

namespace qtmir
{

class DesktopFileReader {
public:
    class Factory
    {
    public:
        Factory();
        Factory(const Factory&) = delete;
        virtual ~Factory();

        Factory& operator=(const Factory&) = delete;

        virtual DesktopFileReader* createInstance(const QString &appId, const QFileInfo& fi);
    };

    virtual ~DesktopFileReader();

    virtual QString file() const { return file_; }
    virtual QString appId() const { return appId_; }
    virtual QString name() const { return entries_[kNameIndex]; }
    virtual QString comment() const { return entries_[kCommentIndex]; }
    virtual QString icon() const { return entries_[kIconIndex]; }
    virtual QString exec() const { return entries_[kExecIndex]; }
    virtual QString path() const { return entries_[kPathIndex]; }
    virtual QString stageHint() const { return entries_[kStageHintIndex]; }
    virtual QString splashTitle() const { return entries_[kSplashTitleIndex]; }
    virtual QString splashImage() const { return entries_[kSplashImageIndex]; }
    virtual QString splashShowHeader() const { return entries_[kSplashShowHeaderIndex]; }
    virtual QString splashColor() const { return entries_[kSplashColorIndex]; }
    virtual QString splashColorHeader() const { return entries_[kSplashColorHeaderIndex]; }
    virtual QString splashColorFooter() const { return entries_[kSplashColorFooterIndex]; }
    virtual bool loaded() const { return loaded_; }

protected:
    friend class DesktopFileReaderFactory;

    DesktopFileReader(const QString &appId, const QFileInfo &desktopFile);

private:
    static const int kNameIndex = 0,
    kCommentIndex = 1,
    kIconIndex = 2,
    kExecIndex = 3,
    kPathIndex = 4,
    kStageHintIndex = 5,
    kSplashTitleIndex = 6,
    kSplashImageIndex = 7,
    kSplashShowHeaderIndex = 8,
    kSplashColorIndex = 9,
    kSplashColorHeaderIndex = 10,
    kSplashColorFooterIndex = 11,
    kNumberOfEntries = 12;

    virtual bool loadDesktopFile(QString desktopFile);

    QString appId_;
    QString file_;
    QVector<QString> entries_;
    bool loaded_;
};

} // namespace qtmir

#endif // DESKTOPFILEREADER_H
