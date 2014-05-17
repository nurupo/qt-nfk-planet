/**
 * Copyright (c) 2014 Maxim Biro <nurupo.contributions@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN pNO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "threadworker.h"

#include "client.h"
#include "planet.h"
#include "server.h"
#include "settings.h"

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>

const char ThreadWorker::OLD_VERSION_MESSAGE[] = "L127.0.0.1\rYour version of NF\rK is too old\r1\r1\r1\r\n\0"
        "L127.0.0.1\rPlease download\rthe latest version\r1\r1\r1\r\n\0"
        "L127.0.0.1\rfrom\r^2needforkill.ru     \r1\r1\r1\r\n\0"
        "L127.0.0.1\r\r\r1\r1\r1\r\n\0"
        "L127.0.0.1\rCKA4AUTE HOBY|-0\rNFK C CAUTA\r1\r1\r1\r\n\0"
        "L127.0.0.1\r^2needforkill.ru    \r\r1\r1\r1\r\n\0E\n\0";


ThreadWorker::ThreadWorker(Planet *planet) :
    QObject(0), clientCount(0), planet(planet), settings(Settings::getInstance())
{
}

void ThreadWorker::onClientConnect(int socketDescriptor)
{
    qDebug() << "Connecting thread" << QThread::currentThread();
    Client *client = new Client();

    client->version = 0;
    client->lastPinged = QDateTime::currentMSecsSinceEpoch();
    client->server = NULL;
    client->sock = new QTcpSocket(this);
    client->sock->setSocketDescriptor(socketDescriptor);
    client->sock->setProperty("client", QVariant::fromValue(client));
    client->sock->setProperty("threadWorker", QVariant::fromValue(this));

    connect(client->sock, SIGNAL(readyRead()), this, SLOT(onClientReadReady()));
    connect(client->sock, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));

    planet->clientListLock.lockForWrite();
    planet->clientList << client;
    planet->clientListLock.unlock();

    clientCountMutex.lock();
    clientCount += 1;
    clientCountMutex.unlock();

    planet->clientIpCountMutex.lock();
    int ipCount = planet->clientIpCount[client->sock->peerAddress().toString()];
    planet->clientIpCount[client->sock->peerAddress().toString()] = ipCount + 1;
    planet->clientIpCountMutex.unlock();

    qDebug("Client connected: %s:%u. There are currently %d connection(s) from client's IP, including this one.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), ipCount + 1);

    planet->clientListLock.lockForRead();
    if (planet->clientList.size() >= settings.getMaxClients()) {
        planet->clientListLock.unlock();
        qDebug("Maximum number of clients (%d) reached. Disconnecting client %s:%u.", settings.getMaxClients(), qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
        client->sock->disconnectFromHost();
        return;
    }
    planet->clientListLock.unlock();

    if (settings.getBlacklistedIps().contains(client->sock->peerAddress().toString())) {
        qDebug("Client %s:%u is blacklisted. Disconnecting.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
        client->sock->disconnectFromHost();
        return;
    }

    int maxConnectionsFromTheSameIp = settings.getMaxSimultaneousConnectionsFromSingleIp();
    if (maxConnectionsFromTheSameIp >= 0 && ipCount == maxConnectionsFromTheSameIp) {
        qDebug("Client %s:%u exceeded the number of maximum simultanious connections from single IP address (%d). Disconnecting.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), maxConnectionsFromTheSameIp);
        client->sock->disconnectFromHost();
        return;
    }
}

void ThreadWorker::onClientDisconnected(Client *client)
{
    qDebug() << "Disconnect thread" << QThread::currentThread();
    // if client is null, that means that this function was called from this thread by QTcpSocket
    if (client == NULL) {
        client = sender()->property("client").value<Client*>();
    }

    planet->clientListLock.lockForWrite();
    bool removed = planet->clientList.removeOne(client);
    planet->clientListLock.unlock();

    // if was already removed do nothing (other thread placed Client on removal in event queue right after this thread removed the Client)
    if (!removed) {
        return;
    }

    // if it prints :0 that mens that the socket is not in QAbstractSocket::ConnectedState
    qDebug("Client disconnected: %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());

    clientCountMutex.lock();
    clientCount -= 1;
    clientCountMutex.unlock();

    planet->clientIpCountMutex.lock();
    planet->clientIpCount[client->sock->peerAddress().toString()] = planet->clientIpCount[client->sock->peerAddress().toString()] - 1;
    if (planet->clientIpCount[client->sock->peerAddress().toString()] <= 0) {
        planet->clientIpCount.remove(client->sock->peerAddress().toString());
    }
    planet->clientIpCountMutex.unlock();

    if (client->server != NULL) {
        planet->serverListLock.lockForWrite();
        // could have been already removed
        planet->serverList.removeOne(client->server);
        planet->serverListLock.unlock();
        client->server->client = NULL;
        delete client->server;
        client->server = NULL;
    }
    // this slot might called by socket's signal, so we can't delete the socket directly
    client->sock->deleteLater();
    delete client;
}

void ThreadWorker::onClientReadReady()
{
    qDebug() << "Commanding thread" << QThread::currentThread();
    Client *client = sender()->property("client").value<Client*>();

    char command[MAX_CLIENT_COMMAND_LENGTH];

    qint64 length;

    // read all new-line'd messages
    while ((length = client->sock->readLine(command, MAX_CLIENT_COMMAND_LENGTH)) > 0) {

        // discard \r\n
        length -= 2;

        command[length] = '\0';

        qDebug("Command from a client %s:%u received: %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), command);

        if (settings.getEnablePenalty() && client->isPenaltyLimitReached()) {
            qDebug("Client %s:%u reached penalty limit.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
            if (settings.getBlacklistIpOnMaxPointsReached()) {
                settings.blacklistIp(client->sock->peerAddress().toString());
                qDebug("Blacklisted IP of client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
            }
            if (settings.getDisconnectClientOnMaxPenaltyPointsReached()) {
                client->sock->disconnectFromHost();
                return;
            } else if (settings.getIgnoreClientCommandsOnMaxPenaltyPointsReached()) {
                return;
            }
        }

        if (length < 2) {
            qWarning("Client %s:%u sent too short command. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
            client->sock->disconnectFromHost();
            return;
        }

        if (command[0] != '?') {
            qWarning("Client %s:%u sent invalid command first byte. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
            client->sock->disconnectFromHost();
            return;
        }

        /* client must ask for Planet version first (since 077 client also reports its version) */
        if (client->version == 0 && command[1] != 'V') {
            qWarning("Client %s:%u did not provide its version first. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
            client->sock->disconnectFromHost();
            return;
        }

        switch (command[1]) {
            case 'V': {   /* version request */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getVersionRequestPenalty());
                }

                if (length == 2) {
                    /* report V075 to old clients */
                    client->version = 75;
                    if (client->sock->write("V075\n") <= 0) {
                        qCritical("Failed to send version number to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                    } else {
                        qDebug("Successfully sent version number to client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    }
                } else {
                    /* extract and save client NFK version */
                    bool ok;
                    client->version = QString(command + 2).toInt(&ok);
                    if (!ok) {
                        qWarning("Client %s:%u sent an invalid version number (%s). Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), command + 2);
                        client->sock->disconnectFromHost();
                        return;
                    }
                    /* report current Planet version */
                    if (client->sock->write(QString("V%1\n").arg(planet->PLANET_VERSION).toAscii().data()) <= 0) {
                        qCritical("Failed to send version number to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                    } else {
                        qDebug("Successfully sent version number to client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    }
                }
                break;
            }
            case 'G': {  /* servers list request */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getServerListRequestPenalty());
                }

                if (client->version < planet->version) {
                    /* send the message to old clients */
                    if (client->sock->write(OLD_VERSION_MESSAGE, sizeof(OLD_VERSION_MESSAGE) - 1) != sizeof(OLD_VERSION_MESSAGE) - 1) {
                        qCritical("Failed to send the old version message to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                    } else {
                        qDebug("Successfully sent the old version message to client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    }
                } else {
                    QByteArray servers;
                    qint64 serversSize = 0;

                    planet->serverListLock.lockForRead();

                    // value from the original nfkplanet
                    servers.reserve(90 * planet->serverList.size() + 3);

                    for (int i = 0; i < planet->serverList.size(); i ++) {
                        Server *server = planet->serverList[i];

                        QString serverEntry = QString("L%1\r%2\r%3\r%4\r%5\r%6\r")
                                .arg(server->client->sock->peerAddress().toString())
                                .arg(server->hostname)
                                .arg(server->mapname)
                                .arg(server->gametype)
                                .arg(server->currentUsers)
                                .arg(server->maxUsers);

                        if (client->version > 76) {
                            serverEntry.append(QString("%1\r")
                                        .arg(server->port));
                        }

                        serverEntry.append("\n");

                        servers.append(serverEntry);
                        servers.append('\0');
                        serversSize += serverEntry.size() + 1;
                    }

                    planet->serverListLock.unlock();

                    servers.append("E\n\0");
                    serversSize += 3;

                    if (client->sock->write(servers.data(), serversSize) != serversSize) {
                        qCritical("Failed to send server list to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                    } else {
                        qDebug("Successfully sent server list to client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    }
                }
                break;
            }
            case 'R': {   /* register new server */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getServerRegistrationPenalty());
                }

                if (client->server != NULL) {
                    qWarning("Client %s:%u tried to register server twice. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    client->sock->disconnectFromHost();
                    return;
                }

                /* don't let old clients create servers, drop them instead */
                if (client->version < 76) {
                    qWarning("Client %s:%u with an old version (%d) tried to register a server. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), client->version);
                    client->sock->disconnectFromHost();
                    return;
                }

                Server *newServer = new Server();

                bool ok;
                newServer->port = QString(command + 2).toShort(&ok);
                if (!ok) {
                    qWarning("Client %s:%u has sent invalid port (%s). Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), command + 2);
                    client->sock->disconnectFromHost();
                    return;
                }

                Server *server = NULL;
                bool serverAlreadyExists = false;

                planet->serverListLock.lockForRead();
                for (int i = 0; i < planet->serverList.size(); i ++) {
                    server = planet->serverList[i];
                    if (server->client->sock->peerAddress().toString().compare(client->sock->peerAddress().toString(), Qt::CaseInsensitive) == 0 && server->port == newServer->port) {
                        serverAlreadyExists = true;
                        break;
                    }
                }
                planet->serverListLock.unlock();

                // taken out from the loop to prevent the deadlock, since disconnectFromHost will call ThreadWorker::onClientDisconnected, which locks the lock
                if (serverAlreadyExists) {
                    qDebug("Client %s:%u tried to create server twice. Removed the first server and disconnecting its client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    // this server might belong to another thread, which might currently do calls on the server, so we do a queued method invocation in owner thread's even loop
                    ThreadWorker *threadWorker = sender()->property("threadWorker").value<ThreadWorker*>();
                    if (threadWorker == this) {
                        server->client->sock->disconnectFromHost();
                    } else {
                        QMetaObject::invokeMethod(threadWorker, "onClientDisconnect", Qt::QueuedConnection, Q_ARG(Client*, server->client));

                        // removing it here since if we don't, clients will see two servers with the same ip, which might break stuff, like asking for invitation
                        planet->serverListLock.lockForWrite();
                        planet->serverList.removeOne(server);
                        planet->serverListLock.unlock();
                    }
                }

                client->server = newServer;
                newServer->client = client;
                newServer->hostname = "null";
                newServer->mapname = "null";
                newServer->currentUsers = '0';
                newServer->maxUsers = '8';
                newServer->gametype = '0';

                planet->serverListLock.lockForWrite();
                planet->serverList << newServer;
                planet->serverListLock.unlock();

                qDebug("Client %s:%u created a server %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->peerAddress().toString()), client->server->port);

                if (client->sock->write("r\n") <= 0) {
                    qCritical("Failed to send server registration confirmation to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                } else {
                    qDebug("Successfully sent server registration confirmation to client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                }

                break;
            }
            case 'N': {   /* set server name */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getSetServerNamePenalty());
                }

                if (client->server == NULL) {
                    qWarning("Client %s:%u has tried to set server name without having a server created. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    client->sock->disconnectFromHost();
                    return;
                }

                client->server->hostname = QString(command + 2);

                qDebug("Client %s:%u set server name of server %s:%u to \"%s\".", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->peerAddress().toString()), client->server->port, qPrintable(client->server->hostname));

                break;
            }
            case 'm': {  /* set server map */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getSetServerMapPenalty());
                }

                if (client->server == NULL) {
                    qWarning("Client %s:%u has tried to set server map name without having a server created. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    client->sock->disconnectFromHost();
                    return;
                }

                client->server->mapname = QString(command + 2);

                qDebug("Client %s:%u set server map name of server %s:%u to \"%s\".", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->peerAddress().toString()), client->server->port, qPrintable(client->server->mapname));

                break;
            }
            case 'C': {  /* set players count */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getSetPlayersCountPenalty());
                }

                if (client->server == NULL) {
                    qWarning("Client %s:%u has tried to set current player count without having a server created. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    client->sock->disconnectFromHost();
                    return;
                }

                client->server->currentUsers = command[2];

                qDebug("Client %s:%u set server current player count of server %s:%u to %c.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->peerAddress().toString()), client->server->port, client->server->currentUsers);

                break;
            }
            case 'M': {  /* set max players count */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getSetMaxPlayersCountPenalty());
                }

                if (client->server == NULL) {
                    qWarning("Client %s:%u has tried to set server maximum player count without having a server created. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    client->sock->disconnectFromHost();
                    return;
                }

                client->server->maxUsers= command[2];

                qDebug("Client %s:%u set server maximum player count of server %s:%u to %c.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->peerAddress().toString()), client->server->port, client->server->maxUsers);

                break;
            }
            case 'P': {  /* set server game type */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getSetGameTypePenalty());
                }

                if (client->server == NULL) {
                    qWarning("Client %s:%u has tried to set server game type without having a server created. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                    client->sock->disconnectFromHost();
                    return;
                }

                client->server->gametype = command[2];

                qDebug("Client %s:%u set server gametype of server %s:%u to %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->peerAddress().toString()), client->server->port, qPrintable(client->server->getGametypeString()));

                break;
            }
            case 'S': {  /* get number of clients */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getNumberOfClientsRequestPenalty());
                }

                planet->clientListLock.lockForRead();
                if (client->sock->write(QString("S%1\n").arg(planet->clientList.size()).toAscii().data()) <= 0) {
                    qCritical("Failed to send planet's' number of connected clients to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                } else {
                    qDebug("Successfully sent planet's' number of connected clients (%d) to client %s:%u.", planet->clientList.size(), qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                }
                planet->clientListLock.unlock();
                break;
            }
            case 'K': {  /* ping */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getPingRequestPenalty());
                }

                client->lastPinged = QDateTime::currentMSecsSinceEpoch();

                if (client->sock->write(QString("K\n").toAscii().data()) <= 0) {
                    qCritical("Failed to send a ping reply to client %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(client->sock->errorString()));
                } else {
                    qDebug("Successfully sent a ping reply to client %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                }

                break;
            }
            case 'X': {  /* ask for invite */
                if (settings.getEnablePenalty()) {
                    client->addPenalty(settings.getInviteRequestPenalty());
                }

                QStringList serverIpPort = QString(command + 2).split(':');

                if (serverIpPort.size() != 2) {
                    qWarning("Client %s:%u has sent invalid invite ip:port (%s). Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), command + 2);
                    client->sock->disconnectFromHost();
                    return;
                }

                QString serverIp = serverIpPort[0];

                bool ok;
                quint16 serverPort = serverIpPort[1].toShort(&ok);
                if (!ok) {
                    qWarning("Client %s:%u has sent invalid port (%s). Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(serverIpPort[0]));
                    client->sock->disconnectFromHost();
                    return;
                }

                Server *server = NULL;
                bool serverFound = false;
                planet->serverListLock.lockForRead();
                for (int i = 0; i < planet->serverList.size(); i ++) {
                    Server *server = planet->serverList[i];
                    if (server->client->sock->peerAddress().toString().compare(serverIp, Qt::CaseInsensitive) == 0 && server->port == serverPort) {
                        serverFound = true;
                        break;
                    }
                }
                planet->serverListLock.unlock();

                if (serverFound) {
                    if (server->client->sock->write((QString("x%1\n").arg(client->sock->peerAddress().toString())).toAscii().data()) <= 0) {
                        qCritical("Failed to rely an invitation request from client %s:%u to server %s:%u. %s.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(server->client->sock->peerAddress().toString()), server->port, qPrintable(client->sock->errorString()));
                    } else {
                        qDebug("Successfully relied an invitation request from client %s:%u to server %s:%u.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort(), qPrintable(server->client->sock->peerAddress().toString()), server->port);
                    }
                }
                break;
            }
            default: {
                qWarning("Client %s:%u has sent an unknown command. Command dropped. Disconnecting the client.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
                client->sock->disconnectFromHost();
                break;
            }
        }

    }
}
