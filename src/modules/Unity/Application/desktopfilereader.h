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
#include <QFileInfo>

namespace qtmir
{

class DesktopFileReaderPrivate;
class DesktopFileReader {
public:
    class Factory
    {
    public:
        Factory() = default;
        Factory(const Factory&) = delete;

        Factory& operator=(const Factory&) = delete;

        virtual DesktopFileReader* createInstance(const QString &appId, const QFileInfo& fi);
    };

    virtual ~DesktopFileReader();

    virtual QString file() const;
    virtual QString appId() const;
    virtual QString name() const;
    virtual QString comment() const;
    virtual QString icon() const;
    virtual QString exec() const;
    virtual QString path() const;
    virtual QString stageHint() const;
    virtual QString splashTitle() const;
    virtual QString splashImage() const;
    virtual QString splashShowHeader() const;
    virtual QString splashColor() const;
    virtual QString splashColorHeader() const;
    virtual QString splashColorFooter() const;
    virtual bool loaded() const;

protected:
    DesktopFileReader(const QString &appId, const QFileInfo &desktopFile);

    DesktopFileReaderPrivate * const d_ptr;

    friend class DesktopFileReaderFactory;

private:
    Q_DECLARE_PRIVATE(DesktopFileReader)
};

} // namespace qtmir

#endif // DESKTOPFILEREADER_H
