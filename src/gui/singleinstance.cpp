#include "singleinstance.h"

#include <QLocalSocket>

#include <unistd.h>

namespace sheaf {
namespace gui {

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                // Commands are short words, anything long is not ours
                const QByteArray data = socket->read(64);
                const QString command = QString::fromUtf8(data).trimmed();
                if (command == QLatin1String("show") || command == QLatin1String("quick")) {
                    emit commandReceived(command);
                }
                socket->disconnectFromServer();
            });
        }
    });
}

QString SingleInstance::serverName()
{
    return QStringLiteral("sheaf-%1").arg(static_cast<unsigned>(getuid()));
}

bool SingleInstance::sendToRunning(const QString &command)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500)) {
        return false;
    }
    socket.write(command.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    if (m_server.listen(serverName())) {
        return true;
    }
    // A crashed instance leaves its socket behind
    QLocalServer::removeServer(serverName());
    return m_server.listen(serverName());
}

} // namespace gui
} // namespace sheaf
