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

#include "planet.h"

#include "client.h"
#include "server.h"
#include "threadworker.h"

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>

const char Planet::PLANET_VERSION[] = "077";

Planet::Planet(int threads) : settings(Settings::getInstance())
{
    // check version for sanety
    bool ok;
    version = QString(PLANET_VERSION).toInt(&ok);
    if (!ok) {
        qFatal("Planet version number defined incorrectly: %s.", qPrintable(PLANET_VERSION));
    }

    // start timer to check pings
    /*pingCheckTimer = new QTimer(this);
    pingCheckTimer->setInterval(CHECK_PING_TIMEOUT);
    connect(pingCheckTimer, SIGNAL(timeout()), this, SLOT(onPingCheck()));
    pingCheckTimer->start(CHECK_PING_TIMEOUT);
*/
    // if no threads were specified, or specified number is invalid (negative), try getting ideal thread count
    if (threads <= 0) {
        threads = QThread::idealThreadCount();
    }

    // if ideal thread count didn't work, well, do a single thread then
    if (threads < 0) {
        threads = 1;
    }

    // move each threadWorker to separate thread
    for (int i = 0; i < threads; i ++) {
        QThread *thread = new QThread(this);
        ThreadWorker *worker = new ThreadWorker(this);
        worker->moveToThread(thread);
        thread->start();
        workerList << worker;
    }

    qDebug("Useing %d thread worker(s).", threads);
}

void Planet::onPingCheck()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    Client *client;

    QMutableListIterator<Client*> it(clientList);
    while (it.hasNext()) {
        client = it.next();
        if (currentTime - client->lastPinged > CLIENT_PING_TIMEOUT) {
            qDebug("Client %s:%u ping timeout.", qPrintable(client->sock->peerAddress().toString()), client->sock->peerPort());
            client->sock->disconnectFromHost();
        }
    }
}

/** Finds a threadWorker with the least number of clients and assigns a new client there
 */
void Planet::incomingConnection(int socketDescriptor)
{

    ThreadWorker *minWorker = workerList[0];
    minWorker->clientCountMutex.lock();
    int min = minWorker->clientCount;
    minWorker->clientCountMutex.unlock();

    for (int i = 1; i < workerList.size(); i ++) {
        ThreadWorker *worker = workerList[i];
        worker->clientCountMutex.lock();
        if (worker->clientCount < min) {
            min = worker->clientCount;
            minWorker = worker;
        }
        worker->clientCountMutex.unlock();
    }

    QMetaObject::invokeMethod(minWorker, "onClientConnect", Qt::QueuedConnection, Q_ARG(int, socketDescriptor));
}

void Planet::start(QString address, quint16 port)
{
    qDebug("Trying to start listening on %s:%u.", qPrintable(address), port);
    if (!listen(QHostAddress(address), port)) {
        qFatal("Error: %s.", qPrintable(errorString()));
        close();
        return;
    }
    qDebug("Listening for incoming connections.");
}

