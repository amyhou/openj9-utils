/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "serverClients.hpp"

#include <chrono>
#include <ctime>

using namespace std;

string NetworkClient::handlePoll(char buffer[])
{
    int n = read(socketFd, buffer, 255);
    string s = string(buffer);
    s.pop_back();

    if (n < 0)
    {
        perror("ERROR reading from socket");
    }
    else if (n > 0)
    {
        return s;
    }

    return "";
}

NetworkClient::NetworkClient(int fd)
{
    socketFd = fd;
}

int NetworkClient::getSocketFd(void)
{
    return socketFd;
}

void NetworkClient::closeFd(void)
{
    close(socketFd);
}

LoggingClient::LoggingClient(const string filename)
{
    logFile.open(filename);
    if (!logFile.is_open())
    {
        perror("ERROR opening logs file");
    }
    else
    {
        logFile << "[" << endl;
    }
}

void LoggingClient::closeFile(void)
{
    if (logFile.is_open())
    {
        // Delete last comma to make a proper json array
        long currentPos = logFile.tellp();
        logFile.seekp(currentPos - 2);
        logFile << '\n' << "]" << endl;

        logFile.close();
    }
}

void LoggingClient::logData(const string message, const std::string recievedFrom)
{
    if (logFile.is_open())
    {
        json log;
        auto currentClockTime = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(currentClockTime);

        // If message was a proper json, log as such
        // Otherwise log the string
        try
        {
            log["body"] = log.parse(message);
        }
        catch(const std::exception& e)
        {
            log["body"] = message;
        }

        log["from"] = recievedFrom;
        log["timestamp"] = currentTime;
        logFile << log.dump(2, ' ', true) << ',' << endl;
    }
}

CommandClient::CommandClient(const string filename)
{
    commandsFile.open(filename);
    if (!commandsFile.is_open())
    {
        printf("filename: %s\n", filename.c_str());
        perror("ERROR opening commands file");
    }

    commands = json::parse(commandsFile);
}

void CommandClient::closeFile(void)
{
    if (commandsFile.is_open())
    {
        commandsFile.close();
    }
}

json CommandClient::handlePoll()
{
    static int commandNumber = 0;
    static const int numCommands = commands.size();
    json j;

    if (currentInterval <= 0)
    {
        if (commandNumber < numCommands)
        {
            return commands[commandNumber++];
        }
        currentInterval = ServerConstants::COMMAND_INTERVALS;
    }
    else
    {
        currentInterval = currentInterval - ServerConstants::POLL_INTERVALS;
    }

    return j;
}
