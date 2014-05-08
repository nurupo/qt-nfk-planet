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

#include "settings.h"

#include <QDebug>
#include <QFile>
#include <QSettings>

const QString Settings::FILENAME = "settings.ini";

#define GET_INT_GENERIC(type, format, var, key, defaultValue, ok) \
    var = s.value(key, defaultValue).to##type(&ok); \
    if (!ok) { \
        qWarning("Invalid key \"%s\" specified in settings. Using the default value of " #format, key, defaultValue); \
        var = defaultValue; \
    }

#define GET_INT(var, key, defautValue, ok) \
    GET_INT_GENERIC(Int, %d, var, key, defautValue, ok)

#define GET_UINT(var, key, defautValue, ok) \
    GET_INT_GENERIC(UInt, %u, var, key, defautValue, ok)

Settings::Settings(const QString &filePath)
{
    load(filePath);
}

Settings& Settings::getInstance(const QString &filePath )
{
    static Settings settings(filePath);
    return settings;
}

void Settings::load(const QString &filePath)
{
    settingsPath = filePath;

    //if no settings file exist -- use the default one and save it on disc
    QFile file(settingsPath);
    if (!file.exists()) {
        if (!QFile::copy(":/texts/" + FILENAME, settingsPath)) {
            qWarning("Failed to save default settings in %s, please check user permissions.", qPrintable(settingsPath));
            settingsPath = ":/texts/" + FILENAME;
        }
    }

    QSettings s(settingsPath, QSettings::IniFormat);
    bool ok;
    s.beginGroup("Network");
        address = s.value("address", "127.0.0.1").toString();

        GET_UINT(port, "port", 10003, ok)
        GET_INT(maxClients, "maxClients", 1024, ok);
        GET_INT(maxSimultaneousConnectionsFromSingleIp, "maxSimultaneousConnectionsFromSingleIp", 10, ok);
    s.endGroup();

    s.beginGroup("Penalty");
        enablePenalty = s.value("enable", true).toBool();

        GET_INT(maxPenaltyPoints, "maxPenaltyPoints", 85, ok)
        GET_UINT(penaltyPeriodSeconds, "penaltyPeriodSeconds", 10, ok)

        disconnectClientOnMaxPenaltyPointsReached = s.value("disconnectClientOnMaxPenaltyPointsReached", true).toBool();
        ignoreClientCommandsOnMaxPenaltyPointsReached = s.value("ignoreClientCommandsOnMaxPenaltyPointsReached", true).toBool();
        blacklistIpOnMaxPenaltyPointsReached = s.value("blacklistIpOnMaxPenaltyPointsReached", true).toBool();

        GET_INT(versionRequestPenalty, "versionRequestPenalty", 1, ok)
        GET_INT(serverListRequestPenalty, "serverListRequestPenalty", 3, ok)
        GET_INT(serverRegistrationPenalty, "serverRegistrationPenalty", 5, ok)
        GET_INT(setServerNamePenalty, "setServerNamePenalty", 3, ok)
        GET_INT(setServerMapPenalty, "setServerMapPenalty", 3, ok)
        GET_INT(setPlayersCountPenalty, "setPlayersCountPenalty", 3, ok)
        GET_INT(setMaxPlayersCountPenalty, "setMaxPlayersCountPenalty", 3, ok)
        GET_INT(setGameTypePenalty, "setGameTypePenalty", 3, ok)
        GET_INT(numberOfClientsRequestPenalty, "numberOfClientsRequestPenalty", 2, ok)
        GET_INT(pingRequestPenalty, "pingRequestPenalty", 1, ok)
        GET_INT(inviteRequestPenalty, "inviteRequestPenalty", 3, ok)
    s.endGroup();

    int blacklistSize = s.beginReadArray("Blacklist");
        while (blacklistSize) {
            s.setArrayIndex(--blacklistSize);
            blacklistedIpSet.insert(s.value("IP", "none").toString());
        }
}

void Settings::blacklistIp(QString ip)
{
    if (blacklistedIpSet.contains(ip)) {
        qDebug("Trying to blacklist IP %s which is already blacklisted.", qPrintable(ip));
        return;
    }
    QSettings s(settingsPath, QSettings::IniFormat);
    int blacklistSize = blacklistedIpSet.size();
    s.beginWriteArray("Blacklist");
        s.setArrayIndex(blacklistSize);
        s.setValue("IP", ip);
    s.endArray();
    blacklistedIpSet.insert(ip);
}
