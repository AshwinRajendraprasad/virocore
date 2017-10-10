//
// Object_JNI.cpp
// ViroRenderer
//
// Copyright © 2016 Viro Media. All rights reserved.

#include <jni.h>
#include <memory>
#include "VROOBJLoader.h"
#include "VROFBXLoader.h"
#include "VROMaterial.h"
#include "VROGeometry.h"
#include "VROPlatformUtil.h"
#include "PersistentRef.h"
#include "VRONode.h"
#include "Node_JNI.h"
#include "OBJLoaderDelegate_JNI.h"


#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_viro_renderer_jni_Object3D_##method_name

extern "C" {

JNI_METHOD(void, nativeLoadModelFromFile)(JNIEnv *env,
                                        jobject object,
                                        jstring file,
                                        jboolean isFBX) {
    const char *cStrFile = env->GetStringUTFChars(file, NULL);
    std::string strFile(cStrFile);
    env->ReleaseStringUTFChars(file, cStrFile);

    jweak weakObj = env->NewWeakGlobalRef(object);

    VROPlatformDispatchAsyncBackground([strFile, weakObj, isFBX] {
        std::string objUrlPath = VROPlatformCopyResourceToFile(strFile);
        std::string objUrlBase = objUrlPath.substr(0, objUrlPath.find_last_of('/'));

        VROPlatformDispatchAsyncRenderer([objUrlPath, objUrlBase, weakObj, isFBX] {
            JNIEnv *env = VROPlatformGetJNIEnv();

            jobject localObj = env->NewLocalRef(weakObj);
            if (localObj == NULL) {
                env->DeleteWeakGlobalRef(weakObj);
                return;
            }

            std::shared_ptr<OBJLoaderDelegate> delegateRef = std::make_shared<OBJLoaderDelegate>(localObj, env);
            std::function<void(std::shared_ptr<VRONode> node, bool success)> onFinish =
                    [delegateRef](std::shared_ptr<VRONode> node, bool success) {
                        if (!success) {
                            return;
                        }
                        delegateRef->objLoaded(node);
                    };

            if (isFBX) {
                VROFBXLoader::loadFBXFromFile(objUrlPath, objUrlBase, true, onFinish);
            }
            else {
                VROOBJLoader::loadOBJFromFile(objUrlPath, objUrlBase, true, onFinish);
            }

            env->DeleteLocalRef(localObj);
            env->DeleteWeakGlobalRef(weakObj);
        });
    });
}

JNI_METHOD(void, nativeLoadModelAndResourcesFromFile)(JNIEnv *env,
                                                     jobject object,
                                                     jstring file,
                                                     jobject resourceMap,
                                                     jboolean isFBX) {
    const char *cStrFile = env->GetStringUTFChars(file, NULL);
    std::string strFile(cStrFile);
    env->ReleaseStringUTFChars(file, cStrFile);

    jweak weakObj = env->NewWeakGlobalRef(object);
    jweak weakResourceMap = env->NewWeakGlobalRef(resourceMap);

    VROPlatformDispatchAsyncBackground([strFile, weakResourceMap, weakObj, isFBX] {
        JNIEnv *env = VROPlatformGetJNIEnv();

        jobject localResourceMap = env->NewLocalRef(weakResourceMap);
        if (localResourceMap == NULL) {
            env->DeleteWeakGlobalRef(weakResourceMap);
            env->DeleteWeakGlobalRef(weakObj);

            return;
        }

        std::string objUrlPath = VROPlatformCopyResourceToFile(strFile);
        std::map<std::string, std::string> cResourceMap = VROPlatformCopyObjResourcesToFile(localResourceMap);

        env->DeleteLocalRef(localResourceMap);
        env->DeleteWeakGlobalRef(weakResourceMap);

        VROPlatformDispatchAsyncRenderer([objUrlPath, cResourceMap, weakObj, isFBX] {
            JNIEnv *env = VROPlatformGetJNIEnv();

            jobject localObj = env->NewLocalRef(weakObj);
            if (localObj == NULL) {
                env->DeleteWeakGlobalRef(weakObj);
                return;
            }

            std::shared_ptr<OBJLoaderDelegate> delegateRef = std::make_shared<OBJLoaderDelegate>(localObj, env);
            std::function<void(std::shared_ptr<VRONode> node, bool success)> onFinish =
                    [delegateRef](std::shared_ptr<VRONode> node, bool success) {
                        if (!success) {
                            return;
                        }
                        delegateRef->objLoaded(node);
                    };

            if (isFBX) {
                VROFBXLoader::loadFBXFromFileWithResources(objUrlPath, cResourceMap, true, onFinish);
            }
            else {
                VROOBJLoader::loadOBJFromFileWithResources(objUrlPath, cResourceMap, true, onFinish);
            }

            env->DeleteLocalRef(localObj);
            env->DeleteWeakGlobalRef(weakObj);
        });
    });
}

JNI_METHOD(void, nativeLoadModelFromUrl)(JNIEnv *env,
                                        jobject object,
                                        jstring url,
                                        jboolean isFBX) {
    const char *cStrUrl = env->GetStringUTFChars(url, NULL);
    std::string objUrlPath(cStrUrl);
    std::string objUrlBase = objUrlPath.substr(0, objUrlPath.find_last_of('/'));
    env->ReleaseStringUTFChars(url, cStrUrl);

    jweak weakObj = env->NewWeakGlobalRef(object);

    VROPlatformDispatchAsyncRenderer([objUrlPath, objUrlBase, weakObj, isFBX] {
        JNIEnv *env = VROPlatformGetJNIEnv();

        jobject localObj = env->NewLocalRef(weakObj);
        if (localObj == NULL) {
            env->DeleteWeakGlobalRef(weakObj);
            return;
        }

        std::shared_ptr<OBJLoaderDelegate> delegateRef = std::make_shared<OBJLoaderDelegate>(localObj, env);
        std::function<void(std::shared_ptr<VRONode> node, bool success)> onFinish =
                [delegateRef](std::shared_ptr<VRONode> node, bool success) {
                    if (!success) {
                        delegateRef->objFailed("Failed to load OBJ");
                    }
                    else {
                        delegateRef->objLoaded(node);
                    }
                };

        if (isFBX) {
            VROFBXLoader::loadFBXFromURL(objUrlPath, objUrlBase, true, onFinish);
        }
        else {
            VROOBJLoader::loadOBJFromURL(objUrlPath, objUrlBase, true, onFinish);
        }

        env->DeleteLocalRef(localObj);
        env->DeleteWeakGlobalRef(weakObj);
    });
}
JNI_METHOD(void, nativeDestroyNode)(JNIEnv *env,
                                   jclass clazz,
                                   jlong native_node_ref) {
    delete reinterpret_cast<PersistentRef<VRONode> *>(native_node_ref);
}

JNI_METHOD(void, nativeAttachToNode)(JNIEnv *env,
                                     jobject object,
                                     jlong native_object_ref,
                                     jlong native_node_ref) {

    // Need a strong reference to this node since it is not attached
    // to the scene graph, and is immediately destroyed (by ObjectJni)
    // after this function returns
    std::shared_ptr<VRONode> nodeWithObj = Node::native(native_object_ref);
    std::weak_ptr<VRONode> node_w = Node::native(native_node_ref);
    jweak weakObj = env->NewWeakGlobalRef(object);

    VROPlatformDispatchAsyncRenderer([nodeWithObj, node_w, weakObj] {
        std::shared_ptr<VRONode> node = node_w.lock();

        if (nodeWithObj && node) {
            std::shared_ptr<VROGeometry> geometry = nodeWithObj->getGeometry();
            // The geometry is set for OBJ models
            node->setGeometry(geometry);

            // The children are set for FBX models (in FBX, the root node is a dummy node)
            for (std::shared_ptr<VRONode> child : nodeWithObj->getChildNodes()) {
                node->addChildNode(child);
            }

            JNIEnv *env = VROPlatformGetJNIEnv();
            jobject localObj = env->NewLocalRef(weakObj);
            if (localObj != NULL) {
                std::shared_ptr<OBJLoaderDelegate> delegateRef = std::make_shared<OBJLoaderDelegate>(localObj, env);
                delegateRef->objAttached();
                env->DeleteLocalRef(localObj);
            }
            env->DeleteWeakGlobalRef(weakObj);
        }
     });
}

} // extern "C"