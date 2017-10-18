/**
 * Copyright © 2016 Viro Media. All rights reserved.
 */
package com.viro.renderer.jni;


public class Material {
    protected long mNativeRef;

    public Material() {
        mNativeRef = nativeCreateMaterial();
    }

    public long getNativeRef() { return mNativeRef; }

    public void setWritesToDepthBuffer(boolean writesToDepthBuffer) {
        nativeSetWritesToDepthBuffer(mNativeRef, writesToDepthBuffer);
    }

    public void setReadsFromDepthBuffer(boolean readsFromDepthBuffer) {
        nativeSetReadsFromDepthBuffer(mNativeRef, readsFromDepthBuffer);
    }

    public void setTexture(Texture texture, String materialPropertyName) {
        nativeSetTexture(mNativeRef, texture.mNativeRef, materialPropertyName);
    }

    public void setColor(long color, String materialPropertyName) {
        nativeSetColor(mNativeRef, color, materialPropertyName);
    }

    public void setShininess(double shininess) {
        nativeSetShininess(mNativeRef, shininess);
    }

    public void setFresnelExponent(double fresnelExponent) {
        nativeSetFresnelExponent(mNativeRef, fresnelExponent);
    }

    public void setLightingModel(String lightingModelName) {
        nativeSetLightingModel(mNativeRef, lightingModelName);
    }

    public void setTransparencyMode(String transparencyModeName) {
        nativeSetTransparencyMode(mNativeRef, transparencyModeName);
    }

    public void setDiffuseIntensity(float diffuseIntensity) { nativeSetDiffuseIntensity(mNativeRef, diffuseIntensity); }

    public void setBloomThreshold(float bloomThreshold) { nativeSetBloomThreshold(mNativeRef, bloomThreshold); }

    public void setCullMode(String cullModeName) {
        nativeSetCullMode(mNativeRef, cullModeName);
    }

    public void destroy() {
        if (mNativeRef != 0) {
            nativeDestroyMaterial(mNativeRef);
        }
        mNativeRef = 0;
    }

    private native long nativeCreateMaterial();
    private native void nativeSetWritesToDepthBuffer(long nativeRef, boolean writesToDepthBuffer);
    private native void nativeSetReadsFromDepthBuffer(long nativeRef, boolean readsFromDepthBuffer);
    private native void nativeSetTexture(long nativeRef, long textureRef, String materialPropertyName);
    private native void nativeSetColor(long nativeRef, long color, String materialPropertyName);
    private native void nativeSetShininess(long nativeRef, double shininess);
    private native void nativeSetFresnelExponent(long nativeRef, double fresnelExponent);
    private native void nativeSetLightingModel(long nativeRef, String lightingModelName);
    private native void nativeSetTransparencyMode(long nativeRef, String transparencyModeName);
    private native void nativeSetCullMode(long nativeRef, String cullModeName);
    private native void nativeSetDiffuseIntensity(long nativeRef, float diffuseIntensity);
    private native void nativeDestroyMaterial(long nativeRef);
    private native void nativeSetBloomThreshold(long nativeRef, float bloomThreshold);
}