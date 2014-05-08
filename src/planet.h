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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef PLANET_H
#define PLANET_H

#include <QMutex>
#include <QObject>
#include <QHash>
#include "settings.h"

class QTimer;
class Client;
class Server;
class QTcpServer;

class Planet : public QObject
{
    Q_OBJECT
public:
    Planet();

    void start(QString address, quint16 port);

private:
    QTimer *pingCheckTimer;
    QTcpServer *server;

    QList<Client*> clientList;
    QList<Server*> serverList;
    QHash<QString, int> clientIpCount;

    static const char PLANET_VERSION[];

    // original had 600*1000, i.e. 600 seconds or 10 minutes
    // it's a long time, considering a client pings about every 60 seconds
    // so we lower that to 3 minutes and 30 seconds, long enough to make 3 ping requests
    static const qint64 CLIENT_PING_TIMEOUT = 3*60*1000 + 5*60*100;
    // 10*1000, i.e. 10 seconds. original value from the nfkplanet
    static const int CHECK_PING_TIMEOUT = 10*1000;
    // original value
    static const size_t MAX_CLIENT_COMMAND_LENGTH = 256;
    // original message
    static const char OLD_VERSION_MESSAGE[];

    int version;

    Settings &settings;

private slots:
    void onPingCheck();
    void onClientConnect();
    void onClientDisconnected();
    void onClientReadReady();

};

#endif // PLANET_H
