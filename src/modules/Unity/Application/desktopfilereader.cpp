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
#include "gscopedpointer.h"
#include "logging.h"

// Qt
#include <QFile>
#include <QLocale>

// GIO
#include <gio/gdesktopappinfo.h>

namespace qtmir
{


DesktopFileReader* DesktopFileReader::Factory::createInstance(const QString &appId, const QFileInfo& fi)
{
    return new DesktopFileReader(appId, fi);
}

typedef GObjectScopedPointer<GAppInfo> GAppInfoPointer;

struct DesktopFileReaderPrivate
{
    DesktopFileReaderPrivate(DesktopFileReader *parent):
            q_ptr( parent )
    {}

    QString getKey(const char *key) const
    {
        if (!loaded()) return QString();

        return QString::fromUtf8(g_desktop_app_info_get_string((GDesktopAppInfo*)appInfo.data(), key));
    }

    bool loaded() const
    {
        return !appInfo.isNull();
    }

    DesktopFileReader * const q_ptr;
    Q_DECLARE_PUBLIC(DesktopFileReader)

    QString appId;
    QString file;
    GAppInfoPointer appInfo; // GAppInfo is actually implemented by GDesktopAppInfo
};


DesktopFileReader::DesktopFileReader(const QString &appId, const QFileInfo &desktopFile)
    : d_ptr(new DesktopFileReaderPrivate(this))
{
    Q_D(DesktopFileReader);

    d->appId = appId;
    d->file = desktopFile.absoluteFilePath();
    d->appInfo.reset((GAppInfo*) g_desktop_app_info_new_from_filename(d->file.toUtf8().constData()));

    if (!d->loaded()) {
        qWarning() << "Desktop file for appId:" << appId << "at:" << d->file << "does not exist, or is not valid";
    }
}

DesktopFileReader::~DesktopFileReader()
{
    Q_D(const DesktopFileReader);
    delete d_ptr;
}

QString DesktopFileReader::file() const
{
    Q_D(const DesktopFileReader);
    return d->file;
}

QString DesktopFileReader::appId() const
{
    Q_D(const DesktopFileReader);
    return d->appId;
}

QString DesktopFileReader::name() const
{
    Q_D(const DesktopFileReader);
    if (!d->loaded()) return QString();

    return QString::fromUtf8(g_app_info_get_name(d->appInfo.data()));
}

QString DesktopFileReader::comment() const
{
    Q_D(const DesktopFileReader);
    if (!d->loaded()) return QString();

    return QString::fromUtf8(g_app_info_get_description(d->appInfo.data()));
}

QString DesktopFileReader::icon() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("Icon");
}

QString DesktopFileReader::exec() const
{
    Q_D(const DesktopFileReader);
    if (!d->loaded()) return QString();

    return QString::fromUtf8(g_app_info_get_commandline(d->appInfo.data()));
}

QString DesktopFileReader::path() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("Path");
}

QString DesktopFileReader::stageHint() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("X-Ubuntu-StageHint");
}

QString DesktopFileReader::splashTitle() const
{
    Q_D(const DesktopFileReader);
    if (!d->loaded()) return QString();

    /* Sadly GDesktopAppInfo only considers Name, GenericName, Comments and Keywords to be keys
     * which can have locale-specific entries. So we need to work to make X-Ubuntu-Splash-Title
     * locale-aware, by generating a locale-correct key name and seeing if that exists. If yes,
     * get the value and return it. Else fallback to the non-localized value.
     */
    GDesktopAppInfo *info = (GDesktopAppInfo*)d->appInfo.data();
    QLocale defaultLocale;
    QStringList locales = defaultLocale.uiLanguages();

    QString keyTemplate("X-Ubuntu-Splash-Title[%1]");
    for (QString locale: locales) {
        // Desktop files use local specifiers with underscore separators but Qt uses hyphens
        locale = locale.replace('-', '_');
        const char* key = keyTemplate.arg(locale).toUtf8().constData();
        if (g_desktop_app_info_has_key(info, key)) {
            return d->getKey(key);
        }
    }

    // Fallback to the non-localized string, if available
    return d->getKey("X-Ubuntu-Splash-Title");
}

QString DesktopFileReader::splashImage() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("X-Ubuntu-Splash-Image");
}

QString DesktopFileReader::splashShowHeader() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("X-Ubuntu-Splash-Show-Header");
}

QString DesktopFileReader::splashColor() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("X-Ubuntu-Splash-Color");
}

QString DesktopFileReader::splashColorHeader() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("X-Ubuntu-Splash-Color-Header");
}

QString DesktopFileReader::splashColorFooter() const
{
    Q_D(const DesktopFileReader);
    return d->getKey("X-Ubuntu-Splash-Color-Footer");
}

bool DesktopFileReader::loaded() const
{
    Q_D(const DesktopFileReader);
    return d->loaded();
}

} // namespace qtmir
