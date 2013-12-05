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

#include "ubuntukeyboardinfo.h"

#include <QDir>

namespace {
    const int gConnectionAttemptIntervalMs = 5000;
    const int gMaxConsecutiveAttempts = 10;
    const char gServerName[] = "ubuntu-keyboard-info";
}

UbuntuKeyboardInfo::UbuntuKeyboardInfo(QObject *parent)
    : QObject(parent),
    m_consecutiveAttempts(0),
    m_lastWidth(0),
    m_lastHeight(0)
{
    connect(&m_socket, &QLocalSocket::stateChanged, this, &UbuntuKeyboardInfo::onSocketStateChanged);
    connect(&m_socket, &QIODevice::readyRead,
            this, &UbuntuKeyboardInfo::readAllBytesFromSocket);

    buildSocketFilePath();

    typedef void (QLocalSocket::*MemberFunctionType)(QLocalSocket::LocalSocketError);
    MemberFunctionType funcPointer = &QLocalSocket::error;
    connect(&m_socket, funcPointer,
            this, &UbuntuKeyboardInfo::onSocketError);

    m_connectionRetryTimer.setInterval(gConnectionAttemptIntervalMs);
    m_connectionRetryTimer.setSingleShot(true);
    connect(&m_connectionRetryTimer, &QTimer::timeout,
            this, &UbuntuKeyboardInfo::tryConnectingToServer);
    tryConnectingToServer();
}

UbuntuKeyboardInfo::~UbuntuKeyboardInfo()
{
    // Make sure we don't get onSocketStateChanged() called during
    // destruction.
    m_socket.disconnect(this);
}

void UbuntuKeyboardInfo::tryConnectingToServer()
{
    ++m_consecutiveAttempts;
    Q_ASSERT(!m_socketFilePath.isEmpty());
    m_socket.connectToServer(m_socketFilePath, QIODevice::ReadOnly);
}

void UbuntuKeyboardInfo::onSocketStateChanged(QLocalSocket::LocalSocketState socketState)
{
    switch (socketState) {
    case QLocalSocket::UnconnectedState:
        retryConnection();
        break;
    case QLocalSocket::ConnectedState:
        m_consecutiveAttempts = 0;
        break;
    default:
        break;
    }
}

void UbuntuKeyboardInfo::onSocketError(QLocalSocket::LocalSocketError socketError)
{
    Q_UNUSED(socketError);
    qWarning() << "UbuntuKeyboardInfo - socket error:" << m_socket.errorString();
}

void UbuntuKeyboardInfo::retryConnection()
{
    // Polling every gConnectionAttemptIntervalMs. Not the best approach but could be worse.
    if (m_consecutiveAttempts < gMaxConsecutiveAttempts) {
        if (!m_connectionRetryTimer.isActive()) {
            m_connectionRetryTimer.start();
        }
    } else {
        qCritical() << "Failed to connect to" << m_socketFilePath << "after"
                    << m_consecutiveAttempts << "failed attempts";

        // it shouldn't be running, but just to be sure.
        m_connectionRetryTimer.stop();
    }
}

void UbuntuKeyboardInfo::readAllBytesFromSocket()
{
    while (m_socket.bytesAvailable() > 0) {
        readInfoFromSocket();
    }
}

void UbuntuKeyboardInfo::readInfoFromSocket()
{
    const size_t sharedInfoSize = sizeof(struct SharedInfo);
    QByteArray bytes = m_socket.read(sharedInfoSize);
    if (bytes.size() != sharedInfoSize) {
        qWarning() << "UbuntuKeyboardInfo: expected to receive" << sharedInfoSize
                   << "but got" << bytes.size();
        return;
    }

    {
        struct SharedInfo *sharedInfo = reinterpret_cast<struct SharedInfo*>(bytes.data());

        if (m_lastX != sharedInfo->keyboardX) {
            m_lastX = sharedInfo->keyboardX;
            Q_EMIT xChanged(m_lastX);
        }

        if (m_lastY != sharedInfo->keyboardY) {
            m_lastY = sharedInfo->keyboardY;
            Q_EMIT yChanged(m_lastY);
        }

        if (m_lastWidth != sharedInfo->keyboardWidth) {
            m_lastWidth = sharedInfo->keyboardWidth;
            Q_EMIT widthChanged(m_lastWidth);
        }

        if (m_lastHeight != sharedInfo->keyboardHeight) {
            m_lastHeight = sharedInfo->keyboardHeight;
            Q_EMIT heightChanged(m_lastHeight);
        }
    }
}

void UbuntuKeyboardInfo::buildSocketFilePath()
{
    char *xdgRuntimeDir = getenv("XDG_RUNTIME_DIR");

    if (xdgRuntimeDir) {
        m_socketFilePath = QDir(xdgRuntimeDir).filePath(gServerName);
    } else {
        m_socketFilePath = QDir("/tmp").filePath(gServerName);
    }

}
