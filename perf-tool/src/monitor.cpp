
#include <atomic>
#include <iostream>
#include <jvmti.h>
#include <ibmjvmti.h>
#include <map>
#include "agentOptions.hpp"
#include "infra.hpp"
#include "json.hpp"

//how many times
using json = nlohmann::json;
struct ClassCycleInfo
{
    int numFirstTier;
    // to be used for tiered spinning
    int numSecondTier;
    int numThirdTier;
};
std::atomic<bool> stackTraceEnabled{true};
std::atomic<int> monitorSampleRate{1};
std::atomic<int> monitorSampleCount{0};

void setMonitorStackTrace(bool val)
{
    // Enables or disables the stack trace option
    stackTraceEnabled = val;
    return;
}

void setMonitorSampleRate(int rate)
{
    if (rate > 0)
    {
        stackTraceEnabled = true;
        monitorSampleRate = rate;
    }
    else
    {
        stackTraceEnabled = false;
    }
}

JNIEXPORT void JNICALL MonitorContendedEntered(jvmtiEnv *jvmtiEnv, JNIEnv *env, jthread thread, jobject object){
    json j;
    jvmtiError error;
    static std::map<const char *, ClassCycleInfo> numContentions;
    jclass cls = env->GetObjectClass(object);
    // First get the class object
    jmethodID mid = env->GetMethodID(cls, "getClass", "()Ljava/lang/Class;");
    jobject clsObj = env->CallObjectMethod(object, mid);
    // Now get the class object's class descriptor
    cls = env->GetObjectClass(clsObj);
    // Find the getName() method on the class object
    mid = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
    // Call the getName() to get a jstring object back
    jstring strObj = (jstring)env->CallObjectMethod(clsObj, mid);
    // Now get the c string from the java jstring object
    const char *str = env->GetStringUTFChars(strObj, NULL);
    /*char *str;
    error = jvmtiEnv->GetClassSignature(cls , &str, NULL);
    if (str != NULL && error == JVMTI_ERROR_NONE) */
    // record calling class
    j["Class"] = str;


    numContentions[str].numFirstTier++;

    int num = numContentions[str].numFirstTier;
    j["numTypeContentions"] = num;
    // Release the memory pinned char array
    env->ReleaseStringUTFChars(strObj, str);
    env->DeleteLocalRef(cls);

    if (true)
    { // only run if the backtrace is enabled
        if (monitorSampleCount % monitorSampleRate == 0)
        {
            jvmtiFrameInfo frames[5];
            jint count;
            jvmtiError err;
            err = jvmtiEnv->GetStackTrace(thread, 0, 5,
                                          frames, &count);
            if (err == JVMTI_ERROR_NONE && count >= 1)
            {
                char *methodName;
                err = jvmtiEnv->GetMethodName(frames[0].method,
                                              &methodName, NULL, NULL);
                if (err == JVMTI_ERROR_NONE)
                {
                    j["Method"] = methodName;
                }
                err = jvmtiEnv->Deallocate((unsigned char*)methodName);
            }
        }
        monitorSampleCount++;
    } 
    sendToServer(j.dump());
}