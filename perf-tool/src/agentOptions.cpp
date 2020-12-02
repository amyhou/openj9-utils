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

#include <jvmti.h>
#include <ibmjvmti.h>
#include <string>
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <unistd.h>

#include "agentOptions.hpp"
#include "infra.hpp"
#include "monitor.hpp"
#include "objectalloc.hpp"
#include "exception.hpp"
#include "methodEntry.hpp"

#include "json.hpp"

using json = nlohmann::json;

void invalidCommand(std::string function, std::string command)
{
    printf("Invalid command with parameters: {functionality: %s, command: %s}\n", function.c_str(), command.c_str());
}

void invalidFunction(std::string function, std::string command)
{
    printf("Invalid function with parameters: {functionality: %s, command: %s}\n", function.c_str(), command.c_str());
}

void invalidRate(std::string function, std::string command, int sampleRate)
{
    printf("Invalid rate with parameters: {functionality: %s, command: %s, sampleRate: %i}\n", function.c_str(), command.c_str(), sampleRate);
}

void modifyMonitorEvents(std::string function, std::string command, int rate)
{
    jvmtiCapabilities capa;
    jvmtiError error;
    setMonitorSampleRate(rate);
    (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
    error = jvmti->GetCapabilities(&capa);
    check_jvmti_error(jvmti, error, "Unable to get current capabilties");
    if (capa.can_generate_monitor_events)
    {
        if (!command.compare("stop"))
        {
            (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
            capa.can_generate_monitor_events = 1;

            error = jvmti->RelinquishCapabilities(&capa);
            check_jvmti_error(jvmti, error, "Unable to relinquish \n");
            error = jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to disable MonitorContendedEntered event.");
        }
        else
        { // c == start
            printf("Monitor Events Capability already enabled\n");
            error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to enable MonitorContendedEntered event notifications.");
        }
    }
    else
    { // cannot generate monitor events
        if (!command.compare("start"))
        {
            (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
            capa.can_generate_monitor_events = 1;

            error = jvmti->AddCapabilities(&capa);
            check_jvmti_error(jvmti, error, "Unable to init monitor events capability");

            error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to enable MonitorContendedEntered event notifications.");
        }
        else
        { // c == stop
            printf("Monitor Events Capability already disabled\n");
            error = jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to disable MonitorContendedEntered event.");
        }
    }
    return;
}

void modifyObjectAllocEvents(std::string function, std::string command, int sampleRate)
{
    jvmtiCapabilities capa;
    jvmtiError error;

    (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
    error = jvmti->GetCapabilities(&capa);
    check_jvmti_error(jvmti, error, "Unable to get current capabilties");
    setObjAllocSampleRate(sampleRate);
    if (capa.can_generate_vm_object_alloc_events)
    {
        if (!command.compare("stop"))
        {
            (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
            capa.can_generate_vm_object_alloc_events = 1;

            error = jvmti->RelinquishCapabilities(&capa);
            check_jvmti_error(jvmti, error, "Unable to relinquish object alloc capability\n");

            error = jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to disable ObjectAlloc event.");
        }
        else if (!command.compare("start"))
        { // c == start
            printf("Object Alloc Capability already enabled");
            error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to enable VM ObjectAlloc event notifications.");
        }
        else
        {
            invalidCommand(function, command);
        }
    }
    else
    { // cannot generate monitor events
        if (!command.compare("start"))
        {
            (void)memset(&capa, 0, sizeof(jvmtiCapabilities));
            capa.can_generate_vm_object_alloc_events = 1;

            error = jvmti->AddCapabilities(&capa);
            check_jvmti_error(jvmti, error, "Unable to init object alloc events capability");

            error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to enable VM ObjectAlloc event notifications.");
        }
        else if (!command.compare("stop"))
        { // c == stop
            printf("Obect Alloc Capability already disabled");
            error = jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread)NULL);
            check_jvmti_error(jvmti, error, "Unable to disable ObjectAlloc event.");
        }
        else
        {
            invalidCommand(function, command);
        }
    }
}

void modifyMonitorStackTrace(std::string function, std::string command)
{
    // enable stack trace
    if (!command.compare("start"))
    {
        setMonitorStackTrace(true);
    }
    else if (!command.compare("stop"))
    {
        setMonitorStackTrace(false);
    }
    else
    {
        invalidCommand(function, command);
    }
}

//object alloc capability already enabled
void modifyMethodEntryEvents(std::string function, std::string command, int sampleRate)
{
    jvmtiError error;
    setMethodEntrySampleRate(sampleRate);
    if (!command.compare("stop"))
    {
        error = jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, (jthread)NULL);
        check_jvmti_error(jvmti, error, "Unable to disable MethodEntry event.\n");
    }
    else if (!command.compare("start"))
    { // c == start
        error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, (jthread)NULL);
        check_jvmti_error(jvmti, error, "Unable to enable MethodEntry event notifications.\n");
    }
    else
    {
        invalidCommand(function, command);
    }
}

void modifyExceptionBackTrace(std::string function, std::string command)
{
    // enable stack trace
    if (!command.compare("start"))
    {
        setExceptionBackTrace(true);
    }
    else if (!command.compare("stop"))
    {
        setExceptionBackTrace(false);
    }
    else
    {
        invalidCommand(function, command);
    }
}

void modifyExceptionEvents(std::string function, std::string command, int sampleRate)
{
    jvmtiError error;
    setExceptionSampleRate(sampleRate);

    if (!command.compare("start"))
    {
        error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_EXCEPTION, (jthread)NULL);
        check_jvmti_error(jvmti, error, "Unable to enable Exception event notifications.\n");
    }
    else if (!command.compare("stop"))
    {
        error = jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_EXCEPTION, (jthread)NULL);
        check_jvmti_error(jvmti, error, "Unable to disable Exception event.\n");
    }
    else
    {
        invalidCommand(function, command);
    }
}


void agentCommand(json jCommand)
{
    jvmtiCapabilities capa;
    jvmtiError error;
    jvmtiPhase phase;

    std::string function;
    function = jCommand["functionality"].get<std::string>();
    std::string command;
    command = jCommand["command"].get<std::string>();
    int sampleRate = 1; // sampleRate is automatically set to 1. To turn off, set to 0
    if (jCommand.contains("sampleRate"))
    {
        sampleRate = jCommand["sampleRate"].get<int>();
        if (sampleRate < 0)
        {
            invalidRate(function, command, sampleRate);
        }
    }

    jvmti->GetPhase(&phase);
    if (!(phase == JVMTI_PHASE_ONLOAD || phase == JVMTI_PHASE_LIVE))
    {
        check_jvmti_error(jvmti, JVMTI_ERROR_WRONG_PHASE, "AGENT CANNOT RECEIVE COMMANDS DURING THIS PHASE\n");
    }
    else
    {
        error = jvmti->GetCapabilities(&capa);
        check_jvmti_error(jvmti, error, "Unable to get current capabilties");

        if (!function.compare("monitorEvents"))
        {
            modifyMonitorEvents(function, command, sampleRate);
        }
        else if (!function.compare("objectAllocEvents"))
        {
            modifyObjectAllocEvents(function, command, sampleRate);
        }
        else if (!function.compare("monitorStackTrace"))
        {
            modifyMonitorStackTrace(function, command);
        }
        else if (!function.compare("methodEntryEvents"))
        {
            modifyMethodEntryEvents(function, command, sampleRate);
        }
        else if (!function.compare("exceptionEvents"))
        {
            modifyExceptionEvents(function, command, sampleRate);
        }
        else
        {
            invalidFunction(function, command);
        }
    }
    return;
}
