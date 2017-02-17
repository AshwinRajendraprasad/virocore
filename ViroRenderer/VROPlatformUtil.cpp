//
//  VROPlatformUtil.cpp
//  ViroRenderer
//
//  Created by Raj Advani on 11/7/16.
//  Copyright © 2016 Viro Media. All rights reserved.
//

#include "VROPlatformUtil.h"
#include "VROLog.h"
#include <stdio.h>
#include <stdlib.h>

#if VRO_PLATFORM_IOS

#import <UIKit/UIKit.h>
#import "VROImageiOS.h"

std::string VROPlatformGetPathForResource(std::string resource, std::string type) {
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.viro.ViroKit"];
    NSString *path = [bundle pathForResource:[NSString stringWithUTF8String:resource.c_str()]
                                      ofType:[NSString stringWithUTF8String:type.c_str()]];
    
    return std::string([path UTF8String]);
}

std::string VROPlatformLoadFileAsString(std::string path) {
    return std::string([[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path.c_str()]
                                                  encoding:NSUTF8StringEncoding
                                                     error:nil] UTF8String]);
}

std::string VROPlatformLoadResourceAsString(std::string resource, std::string type) {
    return VROPlatformLoadFileAsString(VROPlatformGetPathForResource(resource, type));
}

NSURLSessionDataTask *downloadDataWithURLSynchronous(NSURL *url,
                                                     void (^completionBlock)(NSData *data, NSError *error)) {
    
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    
    NSURLSessionConfiguration *sessionConfig = [NSURLSessionConfiguration defaultSessionConfiguration];
    sessionConfig.timeoutIntervalForRequest = 30;
    
    NSURLSession *downloadSession = [NSURLSession sessionWithConfiguration: sessionConfig];
    NSURLSessionDataTask * downloadTask = [downloadSession dataTaskWithURL:url
                                                         completionHandler:^(NSData *data, NSURLResponse *response, NSError *error){
                                                             completionBlock(data, error);
                                                             dispatch_semaphore_signal(semaphore);
                                                         }];
    [downloadTask resume];
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    
    return downloadTask;
}

NSURLSessionDataTask *downloadDataWithURL(NSURL *url, void (^completionBlock)(NSData *data, NSError *error)) {
    NSURLSessionConfiguration *sessionConfig = [NSURLSessionConfiguration defaultSessionConfiguration];
    sessionConfig.timeoutIntervalForRequest = 30;
    
    NSURLSession *downloadSession = [NSURLSession sessionWithConfiguration: sessionConfig];
    NSURLSessionDataTask * downloadTask = [downloadSession dataTaskWithURL:url
                                                         completionHandler:^(NSData *data, NSURLResponse *response, NSError *error){
                                                             completionBlock(data, error);
                                                         }];
    [downloadTask resume];
    return downloadTask;
}

std::string VROPlatformDownloadURLToFile(std::string url, bool *temp) {
    NSURL *URL = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
    
    __block NSString *tempFilePath;
    
    downloadDataWithURLSynchronous(URL, ^(NSData *data, NSError *error) {
        if (error.code == NSURLErrorCancelled){
            return;
        }
        
        if (!error) {
            NSString *fileName = [NSString stringWithFormat:@"%@.tmp", [[NSProcessInfo processInfo] globallyUniqueString]];
            NSURL *fileURL = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:fileName]];
            
            [data writeToURL:fileURL atomically:NO];
            
            *temp = true;
            tempFilePath = [fileURL path];
        }
    });
    
    return std::string([tempFilePath UTF8String]);
}

void VROPlatformDeleteFile(std::string filename) {
    NSError *deleteError = nil;
    [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithUTF8String:filename.c_str()]
                                               error:&deleteError];
}

std::shared_ptr<VROImage> VROPlatformLoadImageFromFile(std::string filename) {
    UIImage *image = [UIImage imageNamed:[NSString stringWithUTF8String:filename.c_str()]];
    return std::make_shared<VROImageiOS>(image);
}

void VROPlatformDispatchAsyncRenderer(std::function<void()> fcn) {
    dispatch_async(dispatch_get_main_queue(), ^{
        fcn();
    });
}

void VROPlatformDispatchAsyncBackground(std::function<void()> fcn) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        fcn();
    });
}

#elif VRO_PLATFORM_ANDROID

#include "VROImageAndroid.h"
#include <mutex>
#include <thread>
#include <map>

// We can hold a static reference to the JVM and to global references, but not to individual
// JNIEnv objects, as those are thread-local. Access the JNIEnv object via getJNIEnv().
// There is one JavaVM per application on Android (shared across activities).
static JavaVM *sVM = nullptr;
static jobject sJavaAssetMgr = nullptr;
static jobject sPlatformUtil = nullptr;
static AAssetManager *sAssetMgr = nullptr;

// Map (and mutex) holding native tasks waiting to be dispatched. On Android the threading
// functionality is handled on the Java layer, so we need some mechanism for mapping IDs
// to corresponding tasks. Both the ID generator and the map itself are protected by the
// mutex.
static std::mutex sTaskMapMutex;
static int sTaskIdGenerator;
static std::map<int, std::function<void()>> sTaskMap;

// Get the JNI Environment for the current thread. If the JavaVM is not yet attached to the
// current thread, attach it
void getJNIEnv(JNIEnv **jenv) {
    if (sVM->GetEnv((void **) jenv, JNI_VERSION_1_6) == JNI_EDETACHED) {
        sVM->AttachCurrentThread(jenv, nullptr);
    }
}

void VROPlatformSetEnv(JNIEnv *env, jobject assetManager, jobject platformUtil) {
    env->GetJavaVM(&sVM);
    sJavaAssetMgr = env->NewGlobalRef(assetManager);
    sPlatformUtil = env->NewGlobalRef(platformUtil);
    sAssetMgr = AAssetManager_fromJava(env, assetManager);
}

void VROPlatformSetEnv(JNIEnv *env) {
    // If the VM was already set, then don't reset it.
    if (!sVM) {
        env->GetJavaVM(&sVM);
    }
}

JNIEnv *VROPlatformGetJNIEnv() {
    JNIEnv *env;
    getJNIEnv(&env);

    return env;
}

jobject VROPlatformGetJavaAssetManager() {
    return sJavaAssetMgr;
}

AAssetManager *VROPlatformGetAssetManager() {
    return sAssetMgr;
}

void VROPlatformReleaseEnv() {
    JNIEnv *env;
    getJNIEnv(&env);

    env->DeleteGlobalRef(sJavaAssetMgr);
    env->DeleteGlobalRef(sPlatformUtil);

    sJavaAssetMgr = nullptr;
    sAssetMgr = nullptr;
}

std::string VROPlatformLoadFileAsString(std::string path) {
    pabort();
    return "";
}

std::string VROPlatformLoadResourceAsString(std::string resource, std::string type) {
    std::string assetName = resource + "." + type;

    AAsset *asset = AAssetManager_open(sAssetMgr, assetName.c_str(), AASSET_MODE_BUFFER);
    size_t length = AAsset_getLength(asset);
    
    char *buffer = (char *)malloc(length + 1);
    AAsset_read(asset, buffer, length);
    buffer[length] = 0;
    
    std::string str(buffer);
    AAsset_close(asset);
    free(buffer);

    return str;
}

std::string VROPlatformDownloadURLToFile(std::string url, bool *temp) {
    JNIEnv *env = VROPlatformGetJNIEnv();
    jstring jurl = env->NewStringUTF(url.c_str());

    jclass cls = env->FindClass("com/viro/renderer/jni/PlatformUtil");
    jmethodID jmethod = env->GetStaticMethodID(cls, "downloadURLToTempFile", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring jpath = (jstring) env->CallStaticObjectMethod(cls, jmethod, jurl);

    const char *path = env->GetStringUTFChars(jpath, 0);
    std::string spath(path);

    pinfo("Path to file is %s", path);

    env->ReleaseStringUTFChars(jpath, path);
    env->DeleteLocalRef(jpath);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(cls);

    *temp = true;
    return spath;
}

void VROPlatformDeleteFile(std::string filename) {
    JNIEnv *env = VROPlatformGetJNIEnv();
    jstring jfilename = env->NewStringUTF(filename.c_str());

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "deleteFile", "(Ljava/lang/String;)V");
    env->CallVoidMethod(sPlatformUtil, jmethod, jfilename);

    env->DeleteLocalRef(jfilename);
    env->DeleteLocalRef(cls);
}

void *VROPlatformLoadFile(std::string filename, int *outLength) {
    FILE *fl = fopen(filename.c_str(), "r");
    fseek(fl, 0, SEEK_END);
    *outLength = ftell(fl);

    char *ret = (char *)malloc(*outLength);
    fseek(fl, 0, SEEK_SET);
    fread(ret, 1, *outLength, fl);
    fclose(fl);

    return ret;
}

std::string VROPlatformCopyAssetToFile(std::string asset) {
    JNIEnv *env = VROPlatformGetJNIEnv();
    jstring jasset = env->NewStringUTF(asset.c_str());

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "copyAssetToFile", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring jpath = (jstring) env->CallObjectMethod(sPlatformUtil, jmethod, jasset);

    const char *path = env->GetStringUTFChars(jpath, 0);
    std::string spath(path);

    pinfo("Path to file is %s", path);

    env->ReleaseStringUTFChars(jpath, path);
    env->DeleteLocalRef(jpath);
    env->DeleteLocalRef(cls);

    return spath;
}

std::shared_ptr<VROImage> VROPlatformLoadImageFromFile(std::string filename) {
    jobject bitmap = VROPlatformLoadBitmapFromFile(filename);
    return std::make_shared<VROImageAndroid>(bitmap);
}

std::shared_ptr<VROImage> VROPlatformLoadImageFromAsset(std::string asset) {
    jobject bitmap = VROPlatformLoadBitmapFromAsset(asset);
    return std::make_shared<VROImageAndroid>(bitmap);
}

jobject VROPlatformLoadBitmapFromAsset(std::string asset) {
    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "loadBitmapFromAsset", "(Ljava/lang/String;)Landroid/graphics/Bitmap;");

    jstring string = env->NewStringUTF(asset.c_str());
    jobject jbitmap = env->CallObjectMethod(sPlatformUtil, jmethod, string);

    env->DeleteLocalRef(string);
    env->DeleteLocalRef(cls);
    return jbitmap;
}

jobject VROPlatformLoadBitmapFromFile(std::string path) {
    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "loadBitmapFromFile", "(Ljava/lang/String;)Landroid/graphics/Bitmap;");

    jstring string = env->NewStringUTF(path.c_str());
    jobject jbitmap = env->CallObjectMethod(sPlatformUtil, jmethod, string);

    env->DeleteLocalRef(string);
    env->DeleteLocalRef(cls);
    return jbitmap;
}

void *VROPlatformConvertBitmap(jobject jbitmap, int *bitmapLength, int *width, int *height) {
    JNIEnv *env;
    getJNIEnv(&env);

    AndroidBitmapInfo bitmapInfo;
    AndroidBitmap_getInfo(env, jbitmap, &bitmapInfo);

    passert (bitmapInfo.format == ANDROID_BITMAP_FORMAT_RGBA_8888);

    *width = bitmapInfo.width;
    *height = bitmapInfo.height;
    *bitmapLength = bitmapInfo.height * bitmapInfo.stride;

    void *bitmapData;
    AndroidBitmap_lockPixels(env, jbitmap, &bitmapData);

    void *safeData = malloc(*bitmapLength);
    memcpy(safeData, bitmapData, *bitmapLength);

    AndroidBitmap_unlockPixels(env, jbitmap);

    env->DeleteLocalRef(jbitmap);
    return safeData;
}

jobject VROPlatformCreateVideoSink(int textureId) {
    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "createVideoSink", "(I)Landroid/view/Surface;");
    jobject jsurface = env->CallObjectMethod(sPlatformUtil, jmethod, textureId);

    env->DeleteLocalRef(cls);
    return jsurface;
}

void VROPlatformDestroyVideoSink(int textureId) {
    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "destroyVideoSink", "(I)V");
    env->CallVoidMethod(sPlatformUtil, jmethod, textureId);

    env->DeleteLocalRef(cls);
}

int VROPlatformGetAudioSampleRate() {
    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "getAudioSampleRate", "()I");
    jint sampleRate = env->CallIntMethod(sPlatformUtil, jmethod);

    env->DeleteLocalRef(cls);
    return sampleRate;
}

int VROPlatformGetAudioBufferSize() {
    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "getAudioBufferSize", "()I");
    jint bufferSize = env->CallIntMethod(sPlatformUtil, jmethod);

    env->DeleteLocalRef(cls);
    return bufferSize;
}

int VROPlatformGenerateTask(std::function<void()> fcn) {
    std::lock_guard<std::mutex> lock(sTaskMapMutex);

    int taskId = ++sTaskIdGenerator;
    sTaskMap[taskId] = fcn;

    return taskId;
}

void VROPlatformRunTask(int taskId) {
    std::function<void()> fcn;
    {
        std::lock_guard<std::mutex> lock(sTaskMapMutex);
        auto it = sTaskMap.find(taskId);
        if (it != sTaskMap.end()) {
            fcn = it->second;
            sTaskMap.erase(it);
        }
    }

    if (fcn) {
        fcn();
        pinfo("Executed task %d [task queue size %d]", taskId, sTaskMap.size());
    }
}

void VROPlatformDispatchAsyncRenderer(std::function<void()> fcn) {
    int task = VROPlatformGenerateTask(fcn);
    pinfo("Generated async task %d for renderer [task queue size %d]", task, sTaskMap.size());

    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->GetObjectClass(sPlatformUtil);
    jmethodID jmethod = env->GetMethodID(cls, "dispatchAsyncRenderer", "(I)V");
    env->CallVoidMethod(sPlatformUtil, jmethod, task);

    env->DeleteLocalRef(cls);
}

void VROPlatformDispatchAsyncBackground(std::function<void()> fcn) {
    int task = VROPlatformGenerateTask(fcn);
    pinfo("Generated async task %d for background [task queue size %d]", task, sTaskMap.size());

    JNIEnv *env;
    getJNIEnv(&env);

    jclass cls = env->FindClass("com/viro/renderer/jni/PlatformUtil");
    jmethodID jmethod = env->GetStaticMethodID(cls, "dispatchAsyncBackground", "(I)V");
    env->CallStaticVoidMethod(cls, jmethod, task);

    env->DeleteLocalRef(cls);
}

jobject VROPlatformGetClassLoader(JNIEnv *jni, jobject jcontext) {
    jclass contextClass = jni->GetObjectClass(jcontext);
    jclass contextClassClass = jni->GetObjectClass(contextClass);

    jmethodID method = jni->GetMethodID(contextClassClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = jni->CallObjectMethod(contextClass, method);

    jni->DeleteLocalRef(contextClass);
    jni->DeleteLocalRef(contextClassClass);

    if (classLoader == 0) {
        perr("Failed to get class loader from activity context");
    }
    return classLoader;
}

jclass VROPlatformFindClass(JNIEnv *jni, jobject javaObject, const char *className) {
    jobject classLoader = VROPlatformGetClassLoader(jni, javaObject);
    jclass classLoaderClass = jni->FindClass("java/lang/ClassLoader");
    jmethodID loadClassMethodId = jni->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;" );

    jstring jclassName = jni->NewStringUTF(className);
    jclass cls = static_cast<jclass>(jni->CallObjectMethod(classLoader, loadClassMethodId, jclassName));
    if (cls == 0) {
        perr("Failed to locate class %s using activity class loader", className);
    }

    jni->DeleteLocalRef(classLoaderClass);
    jni->DeleteLocalRef(classLoader);
    jni->DeleteLocalRef(jclassName);

    return cls;
}

void VROPlatformCallJavaFunction(jobject javaObject,
                                 std::string functionName,
                                 std::string methodID, ...){
    JNIEnv *env = VROPlatformGetJNIEnv();
    env->ExceptionClear();
    
    jclass viroClass = env->GetObjectClass(javaObject);
    if (viroClass == nullptr) {
        perr("Unable to find class for making java calls [function %s, method %s]",
             functionName.c_str(), methodID.c_str());
        return;
    }
    
    jmethodID method = env->GetMethodID(viroClass, functionName.c_str(), methodID.c_str());
    if (method == nullptr) {
        perr("Unable to find method %s callback.", functionName.c_str());
        return;
    }
    
    va_list args;
    va_start(args, methodID);
    env->CallVoidMethodV(javaObject, method, args);
    if (env->ExceptionOccurred()) {
        perr("Exception occured when calling %s.", functionName.c_str());
        env->ExceptionClear();
    }
    va_end(args);
    
    env->DeleteLocalRef(viroClass);
}

 jlong VROPlatformCallJavaLongFunction(jobject javaObject,
                                       std::string functionName,
                                       std::string methodID, ...){
    JNIEnv *env = VROPlatformGetJNIEnv();
    env->ExceptionClear();

    jclass viroClass = env->GetObjectClass(javaObject);
    if (viroClass == nullptr) {
        perr("Unable to find class for making java calls [function %s, method %s]",
             functionName.c_str(), methodID.c_str());
        return 0;
    }

    jmethodID method = env->GetMethodID(viroClass, functionName.c_str(), methodID.c_str());
    if (method == nullptr) {
        perr("Unable to find method %s callback.", functionName.c_str());
        return 0;
    }

    va_list args;
    va_start(args, methodID);
    jlong result = env->CallLongMethodV(javaObject, method, args);
    if (env->ExceptionOccurred()) {
        perr("Exception occured when calling %s.", functionName.c_str());
        env->ExceptionClear();
    }
    va_end(args);

    env->DeleteLocalRef(viroClass);
    return result;
}

void Java_com_viro_renderer_jni_PlatformUtil_runTask(JNIEnv *env, jclass clazz, jint taskId) {
    VROPlatformRunTask(taskId);
}

#endif

#if VRO_PLATFORM_IOS || VRO_PLATFORM_ANDROID

#include "VROStringUtil.h"

gvr_audio_material_type VROPlatformParseGVRAudioMaterial(std::string property) {
    if (VROStringUtil::strcmpinsensitive(property, "transparent")) {
        return GVR_AUDIO_MATERIAL_TRANSPARENT;
    } else if (VROStringUtil::strcmpinsensitive(property, "acoustic_ceiling_tiles")) {
        return GVR_AUDIO_MATERIAL_ACOUSTIC_CEILING_TILES;
    } else if (VROStringUtil::strcmpinsensitive(property, "brick_bare")) {
        return GVR_AUDIO_MATERIAL_BRICK_BARE;
    } else if (VROStringUtil::strcmpinsensitive(property, "brick_painted")) {
        return GVR_AUDIO_MATERIAL_BRICK_PAINTED;
    } else if (VROStringUtil::strcmpinsensitive(property, "concrete_block_coarse")) {
        return GVR_AUDIO_MATERIAL_CONCRETE_BLOCK_COARSE;
    } else if (VROStringUtil::strcmpinsensitive(property, "concrete_block_painted")) {
        return GVR_AUDIO_MATERIAL_CONCRETE_BLOCK_PAINTED;
    } else if (VROStringUtil::strcmpinsensitive(property, "curtain_heavy")) {
        return GVR_AUDIO_MATERIAL_CURTAIN_HEAVY;
    } else if (VROStringUtil::strcmpinsensitive(property, "fiber_glass_insulation")) {
        return GVR_AUDIO_MATERIAL_FIBER_GLASS_INSULATION;
    } else if (VROStringUtil::strcmpinsensitive(property, "glass_thin")) {
        return GVR_AUDIO_MATERIAL_GLASS_THIN;
    } else if (VROStringUtil::strcmpinsensitive(property, "glass_thick")) {
        return GVR_AUDIO_MATERIAL_GLASS_THICK;
    } else if (VROStringUtil::strcmpinsensitive(property, "grass")) {
        return GVR_AUDIO_MATERIAL_GRASS;
    } else if (VROStringUtil::strcmpinsensitive(property, "linoleum_on_concrete")) {
        return GVR_AUDIO_MATERIAL_LINOLEUM_ON_CONCRETE;
    } else if (VROStringUtil::strcmpinsensitive(property, "marble")) {
        return GVR_AUDIO_MATERIAL_MARBLE;
    } else if (VROStringUtil::strcmpinsensitive(property, "metal")) {
        return GVR_AUDIO_MATERIAL_METAL;
    } else if (VROStringUtil::strcmpinsensitive(property, "parquet_on_concrete")) {
        return GVR_AUDIO_MATERIAL_PARQUET_ON_CONCRETE;
    } else if (VROStringUtil::strcmpinsensitive(property, "plaster_rough")) {
        return GVR_AUDIO_MATERIAL_PLASTER_ROUGH;
    } else if (VROStringUtil::strcmpinsensitive(property, "plaster_smooth")) {
        return GVR_AUDIO_MATERIAL_PLASTER_SMOOTH;
    } else if (VROStringUtil::strcmpinsensitive(property, "plywood_panel")) {
        return GVR_AUDIO_MATERIAL_PLYWOOD_PANEL;
    } else if (VROStringUtil::strcmpinsensitive(property, "polished_concrete_or_tile")) {
        return GVR_AUDIO_MATERIAL_POLISHED_CONCRETE_OR_TILE;
    } else if (VROStringUtil::strcmpinsensitive(property, "sheet_rock")) {
        return GVR_AUDIO_MATERIAL_SHEET_ROCK;
    } else if (VROStringUtil::strcmpinsensitive(property, "water_or_ice_surface")) {
        return GVR_AUDIO_MATERIAL_WATER_OR_ICE_SURFACE;
    } else if (VROStringUtil::strcmpinsensitive(property, "wood_ceiling")) {
        return GVR_AUDIO_MATERIAL_WOOD_CEILING;
    } else if (VROStringUtil::strcmpinsensitive(property, "wood_panel")) {
        return GVR_AUDIO_MATERIAL_WOOD_PANEL;
    }
    return GVR_AUDIO_MATERIAL_TRANSPARENT;
}

#endif