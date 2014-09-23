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

#ifndef QTMIR_CLIPBOARD_H
#define QTMIR_CLIPBOARD_H

#include <qpa/qplatformclipboard.h>
#include <QObject>

namespace qtmir {

class DBusClipboard : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.QtMir.Clipboard")
public:
    DBusClipboard(QObject *parent = nullptr);
    virtual ~DBusClipboard() {}

    void setContents(QByteArray contents);
    const QByteArray &contents() const { return m_contents; }

    static const int maxContentsSize = 4 * 1024 * 1024;  // 4 Mb

    // To make it testable
    static bool skipDBusRegistration;

Q_SIGNALS:
    Q_SCRIPTABLE void ContentsChanged(const QByteArray &contents);
    void contentsChangedRemotely();

public Q_SLOTS:
    Q_SCRIPTABLE QByteArray GetContents() const;
    Q_SCRIPTABLE void SetContents(QByteArray contents);

private:
    void performDBusRegistration();
    bool setContentsHelper(QByteArray newContents);

    // Contains a serialized QMimeData
    // Serialization and deserialization is done by the QPlatformClipboard
    // implementation.
    QByteArray m_contents;
};

class Clipboard : public QObject, public QPlatformClipboard
{
    Q_OBJECT
public:
    Clipboard(QObject *parent = nullptr);
    virtual ~Clipboard() {}

    QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode) override;

    void setupDBusService();

private Q_SLOTS:
    void setMimeDataWithDBusClibpboardContents();

private:

    DBusClipboard *m_dbusClipboard;
};

// NB: Copied from qtubuntu. Must be kept in sync with the original version!
// Best thing would be to share this code somehow, but not bothering with it right now
// as the clipboard will move to content-hub at some point.
QByteArray serializeMimeData(QMimeData *mimeData);
QMimeData *deserializeMimeData(const QByteArray &serializedMimeData);

} // namespace qtmir

#endif // QTMIR_CLIPBOARD_H
