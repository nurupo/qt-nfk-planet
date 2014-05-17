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

#ifndef CLIENT_H
#define CLIENT_H

#include <QMetaType>
#include <QQueue>
#include <QtGlobal>

class QTcpSocket;
class Server;

/** Client represents anyone connected to the Planet.
 *
 *  Clients have optional penalty system, which is aimed to prevent flooding
 *  of the Planet by malicious clients. Every valid command client sends has
 *  a penalty value associated with it, which is based on the complexity of
 *  that command (requires locking for writing? requires iterating over a list
 *  of clients/servers? etc). So every time a valid command is received,
 *  client's penalty points increase. If client exceeds maximum penalty points,
 *  then some action is taken. Penalty points for each command, maximum penalty
 *  points and the action are specified in the settings file. The penalty system
 *  takes into account only penalty points received during last penaltyPediod
 *  seconds, which is also specified in settings. Regular clients shouldn't be
 *  able to generate enough penalty points to reach the default number of max.
 *  penalty points, so whoever reached it is considered to be a malicious client
 *  that tries to flood the Planet with commands.
 */
class Client
{
public:
    QTcpSocket *sock;
    int version;
    qint64 lastPinged;
    Server *server;

    void addPenalty(int value);
    bool isPenaltyLimitReached();

private:
    struct Penalty {
        Penalty(quint64 time, int value);

        quint64 time;
        int value;
    };

    int penaltyPoints;
    QQueue<Penalty> penaltyQueue;

};

Q_DECLARE_METATYPE(Client*)

#endif // CLIENT_H
