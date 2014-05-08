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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QSet>

class Settings
{
public:
    static Settings& getInstance(const QString &filePath = FILENAME);

    QString getAddress() {return address;}
    quint16 getPort() {return port;}

    int getMaxClients() {return maxClients;}
    int getMaxSimultaneousConnectionsFromSingleIp() {return maxSimultaneousConnectionsFromSingleIp;}

    int getMaxPenaltyPoints() {return maxPenaltyPoints;}
    int getPenaltyPeriodSeconds() {return penaltyPeriodSeconds;}

    bool getEnablePenalty() {return enablePenalty;}

    bool getDisconnectClientOnMaxPenaltyPointsReached() {return disconnectClientOnMaxPenaltyPointsReached;}
    bool getIgnoreClientCommandsOnMaxPenaltyPointsReached() {return ignoreClientCommandsOnMaxPenaltyPointsReached;}
    bool getBlacklistIpOnMaxPointsReached() {return blacklistIpOnMaxPenaltyPointsReached;}

    int getVersionRequestPenalty() {return versionRequestPenalty;}
    int getServerListRequestPenalty() {return serverListRequestPenalty;}
    int getServerRegistrationPenalty() {return serverRegistrationPenalty;}
    int getSetServerNamePenalty() {return setServerNamePenalty;}
    int getSetServerMapPenalty() {return setServerMapPenalty;}
    int getSetPlayersCountPenalty() {return setPlayersCountPenalty;}
    int getSetMaxPlayersCountPenalty() {return setMaxPlayersCountPenalty;}
    int getSetGameTypePenalty() {return setGameTypePenalty;}
    int getNumberOfClientsRequestPenalty() {return numberOfClientsRequestPenalty;}
    int getPingRequestPenalty() {return pingRequestPenalty;}
    int getInviteRequestPenalty() {return inviteRequestPenalty;}

    QSet<QString> getBlacklistedIps() {return blacklistedIpSet;}

    void blacklistIp(QString ip);

private:
    Settings();
    Settings(const QString &filePath);
    Settings(Settings &settings);
    Settings& operator=(const Settings&);

    void load(const QString &filePath);

    static const QString FILENAME;
    QString settingsPath;

    QString address;
    quint16 port;

    int maxClients;
    int maxSimultaneousConnectionsFromSingleIp;

    int maxPenaltyPoints;
    int penaltyPeriodSeconds;

    bool enablePenalty;

    bool disconnectClientOnMaxPenaltyPointsReached;
    bool ignoreClientCommandsOnMaxPenaltyPointsReached;
    bool blacklistIpOnMaxPenaltyPointsReached;

    int versionRequestPenalty;
    int serverListRequestPenalty;
    int serverRegistrationPenalty;
    int setServerNamePenalty;
    int setServerMapPenalty;
    int setPlayersCountPenalty;
    int setMaxPlayersCountPenalty;
    int setGameTypePenalty;
    int numberOfClientsRequestPenalty;
    int pingRequestPenalty;
    int inviteRequestPenalty;

    QSet<QString> blacklistedIpSet;

};

#endif // SETTINGS_H
