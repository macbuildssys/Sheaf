#pragma once

#include <QLocalServer>
#include <QObject>

namespace sheaf {
namespace gui {

// Makes sure only one window runs per user, later starts hand over and exit
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);

    // Tries to tell a running instance what to do. True when one answered
    bool sendToRunning(const QString &command);

    // Becomes the running instance. False when listening failed
    bool listen();

    static QString serverName();

signals:
    void commandReceived(const QString &command);

private:
    QLocalServer m_server;
};

} // namespace gui
} // namespace sheaf
