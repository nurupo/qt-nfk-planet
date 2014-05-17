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

#include "settings.h"

#include <QHash>
#include <QMutex>
#include <QReadWriteLock>
#include <QTcpServer>

class Client;
class QThread;
class QTimer;
class Server;
class ThreadWorker;

/** Planet listens for incoming connections on a TCP socket and moves each connected client to a
 *  ThreadWorker with the least number of currently connected clients, which runs in a completely
 *  separate thread.
 *
 *  It's beneficial to process clients in separate threads because the most common operations clients
 *  do are read operations: Planet version request, get list of servers, ping and send an invite. Write
 *  operations are only used when adding/removing a client and adding a server. Moreover, ThreadWorkers
 *  process only clients assigned to them, they never process some other thread's clients (at least
 *  directly). So they just need synchronization when accessing Planet's clientList, serverList and
 *  clientIpCount, which most of the time would be locking for read, which can be done simultaneously by
 *  multiple threads with the use of QReadWriteLock.
 */
class Planet : public QTcpServer
{
    Q_OBJECT
public:
    Planet(int threads = 0);
    void start(QString address, quint16 port);


    //QTimer *pingCheckTimer;

    QList<Client*> clientList;
    QReadWriteLock clientListLock;

    QList<Server*> serverList;
    QReadWriteLock serverListLock;

    QHash<QString, int> clientIpCount;
    QMutex clientIpCountMutex;

    QList<ThreadWorker*> workerList;

    static const char PLANET_VERSION[];

    // original had 600*1000, i.e. 600 seconds or 10 minutes
    // it's a long time, considering a client pings about every 60 seconds
    // so we lower that to 3 minutes and 30 seconds, long enough to make 3 ping requests
    static const qint64 CLIENT_PING_TIMEOUT = 3*60*1000 + 5*60*100;
    // 10*1000, i.e. 10 seconds. original value from the nfkplanet
    static const int CHECK_PING_TIMEOUT = 10*1000;

    int version;

protected:
    void incomingConnection(int socketDescriptor); //http://qt-project.org/doc/qt-4.8/network-threadedfortuneserver.html

private:


    Settings &settings;

private slots:
    void onPingCheck();

};

#endif // PLANET_H
