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

#ifndef THREADWORKER_H
#define THREADWORKER_H

#include "settings.h"

#include <QMetaType>
#include <QMutex>
#include <QObject>

class Client;
class Planet;

/** ThreadWorker processes clients assigned to it through onClientConnect.
 *
 *  ThreadWorker doesn't modify other ThreadWorker's clients or servers
 *  created by those clients directly. Though it doesn't modify them directly,
 *  there is one case when it's done indirectly -- when a client tries to
 *  register a server with ip and port that match one of existing servers. If
 *  that happens, then the first client is disconnected (thus its server is also
 *  removed) and the server of the second client is registered. The first
 *  client that we want to disconnect might not belong to the ThreadWorker we
 *  want to disconnect it in. This means that the ThreadWorker that owns the
 *  first client might be in the middle of processing the client, so we can't
 *  just disconnect and deallocate the client, we need to make sure that the
 *  owner thread is not currently processing it. This is done by disconnecting
 *  the first client in ThreadWorker that owns it through QMetaObject::invokeMethod
 *  call. So we send a request to the owner ThreadWorker to disconnect the client
 *  sometime in the future. Though the client might be already disconnected by
 *  the time the request will be processed, so we check in onClientDisconnected if
 *  there is such client in the client list, if no -- we stop the disconnecting,
 *  since the client was already disconnected.
 */
class ThreadWorker : public QObject
{
    Q_OBJECT
public:
    explicit ThreadWorker(Planet *planet);

    int clientCount;
    QMutex clientCountMutex;

private:
    Planet *planet;
    Settings &settings;

    // original value
    static const size_t MAX_CLIENT_COMMAND_LENGTH = 256;

    // original message
    static const char OLD_VERSION_MESSAGE[];

public slots:
    void onClientConnect(int socketDescriptor);
    void onClientDisconnected(Client *client = NULL);

private slots:
    void onClientReadReady();

};

Q_DECLARE_METATYPE(ThreadWorker*)

#endif // THREADWORKER_H
