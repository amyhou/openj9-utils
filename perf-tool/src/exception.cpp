#include <atomic>
#include <jvmti.h>
#include <string>

#include "agentOptions.hpp"
#include "infra.hpp"
#include "json.hpp"
#include "exception.hpp"

using namespace std;
using json = nlohmann::json;

std::atomic<bool> backTraceEnabled {true};
std::atomic<int> exceptionSampleCount {0};
std::atomic<int> exceptionSampleRate {1};


void setExceptionSampleRate(int rate) {
    /* Set sampling rate and enable/disable backtrace */
    if (rate > 0) {
        setExceptionBackTrace(true);
        exceptionSampleRate = rate;
    } else {
        setExceptionBackTrace(false);
    }
}


void setExceptionBackTrace(bool val){
    /* Enables or disables the stack trace option */
    backTraceEnabled = val;
    return;
}


JNIEXPORT void JNICALL Exception(jvmtiEnv *jvmtiEnv,
            JNIEnv* jniEnv,
            jthread thread,
            jmethodID method,
            jlocation location,
            jobject exception,
            jmethodID catch_method,
            jlocation catch_location) {

    json jdata;
    jvmtiError err;
    jclass klass;
    char *fileName;
    int lineCount, lineNumber;
    jvmtiLineNumberEntry *lineTable;
    char buffer[20];
    char *methodName;

    static int numExceptions = 0; // number of total exceptions recorded
    numExceptions++;

    // First get exception address
    char ptrStr[] = "%p";
    sprintf(buffer, ptrStr, exception);
    string exceptionStr(buffer);
    jdata["exceptionAddress"] = (char*)exceptionStr.c_str();

    /* Get method name */
    err = jvmtiEnv->GetMethodName(method, &methodName, NULL, NULL);
    if (err == JVMTI_ERROR_NONE) { 
        jdata["callingMethod"] = (char *)methodName;  // record calling method
    } else {
        printf("GetMethodName) Error received: %d\n", err);
    }

    /* Get line number */
    err = jvmtiEnv->GetLineNumberTable(method, &lineCount, &lineTable);
    if (err == JVMTI_ERROR_NONE) { // Find line
        lineNumber = lineTable[0].line_number;
        for (int i = 1; i < lineCount; i++) {
            if (location < lineTable[i].start_location) {
                break;
            } else {
                lineNumber = lineTable[i].line_number;
            }
        }
        jdata["callingMethodLineNumber"] = lineNumber;  // record line number of calling method
    } else {
        printf("GetLineNumberTable) Error received: %d\n", err);
    }

    /* Get jclass object of calling method*/
    err = jvmtiEnv->GetMethodDeclaringClass(method, &klass);
    check_jvmti_error(jvmtiEnv, err, "Unable to get method declaring class.\n");

    /* Get source file name */
    err = jvmtiEnv->GetSourceFileName(klass, &fileName);
    if (err == JVMTI_ERROR_NONE) {
        jdata["callingMethodSourceFile"] = fileName;
    }

    // Get information from stack
    if (backTraceEnabled) { // only run when backtrace is enabled
        if (exceptionSampleCount % exceptionSampleRate == 0) {
            int numFrames = 5;
            jvmtiFrameInfo frames[numFrames];
            jint count;
            auto jMethods = json::array();

            err = jvmtiEnv->GetStackTrace(thread, 0, numFrames,
                                        frames, &count);
            if (err == JVMTI_ERROR_NONE && count >= 1) {
                json jMethod;
                for (int i = 0; i < count; i++) {
                    /* Get method name */
                    err = jvmtiEnv->GetMethodName(frames[i].method,
                                        &methodName, NULL, NULL);
                    if (err == JVMTI_ERROR_NONE) {
                        jMethod["methodName"] = methodName;
                    }

                    err = jvmtiEnv->GetLineNumberTable(frames[i].method, &lineCount, &lineTable);
                    if (err == JVMTI_ERROR_NONE) { // Find line
                        lineNumber = lineTable[0].line_number;
                        for (int j = 1; j < lineCount; j++) {
                            if (frames[i].location < lineTable[i].start_location) {
                                break;
                            } else {
                                lineNumber = lineTable[i].line_number;
                            }
                        }

                        jMethod["lineNumber"] = lineNumber;
                    }

                    /* Get jclass object of calling method*/
                    err = jvmtiEnv->GetMethodDeclaringClass(frames[i].method, &klass);
                    check_jvmti_error(jvmtiEnv, err, "Unable to get method declaring class.\n");

                    /* Get file name */
                    err = jvmtiEnv->GetSourceFileName(klass, &fileName);
                    if (err == JVMTI_ERROR_NONE) {
                        jMethod["fileName"] = fileName;
                    }

                    jMethods.push_back(jMethod);
                }
            }

            jdata["backtrace"] = jMethods;
        }

        exceptionSampleCount++;
    }

    jdata["numExceptions"] = numExceptions;

    err = jvmtiEnv->Deallocate((unsigned char*)fileName);
    err = jvmtiEnv->Deallocate((unsigned char*)methodName);
    err = jvmtiEnv->Deallocate((unsigned char*)lineTable);

    sendToServer(jdata.dump());
}
