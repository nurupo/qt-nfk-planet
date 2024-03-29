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

#include "client.h"
#include "settings.h"

#include <QDateTime>

Client::Penalty::Penalty(quint64 time, int value) : time(time), value(value)
{
    // intentially left blank
}

void Client::addPenalty(int value)
{
    penaltyQueue.enqueue(Penalty(QDateTime::currentMSecsSinceEpoch(), value));
    penaltyPoints += value;
    qDebug("Adding penalty of %d.", value);
    qDebug("%d Total penalty points %d.", QTime::currentTime().second(), penaltyPoints);
}

bool Client::isPenaltyLimitReached()
{
    // get onle points that are within last penaltyPeriod seconds
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    Settings &settings = Settings::getInstance();

    quint64 penaltyPeriodMilliseconds = settings.getPenaltyPeriodSeconds() * 1000;

    while (!penaltyQueue.isEmpty() && (currentTime - penaltyQueue.head().time > penaltyPeriodMilliseconds)) {
        penaltyPoints -= penaltyQueue.dequeue().value;
    }

    return penaltyPoints >= settings.getMaxPenaltyPoints();
}
