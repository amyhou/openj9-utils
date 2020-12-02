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
#include <string.h>
#include "agentOptions.hpp"
#include "methodEntry.hpp"
#include "json.hpp"
#include "server.hpp"
#include "infra.hpp"

#include <iostream>
#include <atomic>

using json = nlohmann::json;

std::atomic<int> mEntrySampleCount {0};
std::atomic<int> mEntrySampleRate {1};

/* set sample rate according to command instructions
 * requirement: rate > 0 */
void setMethodEntrySampleRate(int rate) {
    mEntrySampleRate = rate;
}

/*** retrieves method name and line number, and declaring class name and signature
 *      for every nth method entry                                                 ***/
JNIEXPORT void JNICALL MethodEntry(jvmtiEnv *jvmtiEnv,
            JNIEnv* env,
            jthread thread,
            jmethodID method) {

    int numMethods;
    /* Get number of methods and increment */
    numMethods = atomic_fetch_add(&mEntrySampleCount, 1);

    if (numMethods % mEntrySampleRate == 0) {           
        json j;
        jvmtiError err;
        char *name_ptr;
        char *signature_ptr;
        char *declaringClassName;
        jclass declaring_class;
        jint entry_count_ptr;
        jvmtiLineNumberEntry* table_ptr;

        j["methodNum"] = numMethods;
        
        err = jvmtiEnv->GetMethodName(method, &name_ptr, &signature_ptr, NULL);
        if (check_jvmti_error(jvmtiEnv, err, "Unable to retrieve Method Name.\n")) {
            j["methodName"] = name_ptr;
            j["methodSig"] = signature_ptr;
        }
        err = jvmtiEnv->GetMethodDeclaringClass(method, &declaring_class);
        if (check_jvmti_error(jvmtiEnv, err, "Unable to retrieve Method Declaring Class.\n")) {
            err = jvmtiEnv->GetClassSignature(declaring_class, &declaringClassName, NULL);
            if (check_jvmti_error(jvmtiEnv, err, "Unable to retrieve Method Declaring Class Signature.\n")) {
                j["methodClass"] = declaringClassName;
            }
        }
        err = jvmtiEnv->GetLineNumberTable(method, &entry_count_ptr, &table_ptr);
        if (check_jvmti_error(jvmtiEnv, err, "Unable to retrieve Method Line Number Table.\n")) {
            j["methodLineNum"] = table_ptr->line_number;
        }

        err = jvmtiEnv->Deallocate((unsigned char*)name_ptr);
        check_jvmti_error(jvmtiEnv, err, "Unable to deallocate name_ptr.\n");
        err = jvmtiEnv->Deallocate((unsigned char*)signature_ptr);
        check_jvmti_error(jvmtiEnv, err, "Unable to deallocate signature_ptr.\n");
        err = jvmtiEnv->Deallocate((unsigned char*)declaringClassName);
        check_jvmti_error(jvmtiEnv, err, "Unable to deallocate declaringClassName.\n");
        err = jvmtiEnv->Deallocate((unsigned char*)table_ptr);
        check_jvmti_error(jvmtiEnv, err, "Unable to deallocate table_ptr.\n");
    
        std::string s = j.dump();
        sendToServer(s);
    }
    mEntrySampleCount = atomic_fetch_add(&mEntrySampleCount, 1);
}
