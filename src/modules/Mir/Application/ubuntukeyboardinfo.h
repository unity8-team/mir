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

#ifndef UBUNTU_KEYBOARD_INFO_H
#define UBUNTU_KEYBOARD_INFO_H

#include <QLocalSocket>
#include <QTimer>

// Temporary solution to get information about the onscreen keyboard
// This shouldn't be needed once the OSK is a properly sized surface
// instead of a fullscreen one.
class UbuntuKeyboardInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal x READ x NOTIFY xChanged)
    Q_PROPERTY(qreal y READ y NOTIFY yChanged)
    Q_PROPERTY(qreal width READ width NOTIFY widthChanged)
    Q_PROPERTY(qreal height READ height NOTIFY heightChanged)
public:
    UbuntuKeyboardInfo(QObject *parent = 0);
    virtual ~UbuntuKeyboardInfo();
    qreal x() const { return m_lastX; }
    qreal y() const { return m_lastY; }
    qreal width() const { return m_lastWidth; }
    qreal height() const { return m_lastHeight; }

Q_SIGNALS:
    void xChanged(qreal x);
    void yChanged(qreal y);
    void widthChanged(qreal width);
    void heightChanged(qreal height);

private Q_SLOTS:
    void tryConnectingToServer();
    void onSocketStateChanged(QLocalSocket::LocalSocketState socketState);
    void onSocketError(QLocalSocket::LocalSocketError socketError);
    void readAllBytesFromSocket();

private:
    // NB! Must match the definition in ubuntu-keyboard. Not worth creating a shared header
    // just for that.
    struct SharedInfo {
        qint32 keyboardX;
        qint32 keyboardY;
        qint32 keyboardWidth;
        qint32 keyboardHeight;
    };
    void readInfoFromSocket();
    void retryConnection();
    void buildSocketFilePath();

    int m_consecutiveAttempts;

    QLocalSocket m_socket;
    qint32 m_lastX;
    qint32 m_lastY;
    qint32 m_lastWidth;
    qint32 m_lastHeight;
    QTimer m_connectionRetryTimer;

    // Path to the socket file created by ubuntu-keyboard
    QString m_socketFilePath;
};

#endif // UBUNTU_KEYBOARD_INFO_H
