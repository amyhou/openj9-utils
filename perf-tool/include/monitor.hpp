#ifndef MONITOR_H_
#define MONITOR_H_

JNIEXPORT void JNICALL MonitorContendedEntered(jvmtiEnv *jvmtiEnv,
            JNIEnv* env,
            jthread thread, 
            jobject object);

void setMonitorStackTrace(bool val);



#endif /* MONITOR_H_ */ 