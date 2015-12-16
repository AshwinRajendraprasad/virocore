//
//  VRORenderDelegate.h
//  ViroRenderer
//
//  Created by Raj Advani on 12/15/15.
//  Copyright © 2015 Viro Media. All rights reserved.
//

#ifndef VRORenderDelegate_h
#define VRORenderDelegate_h

@class VROView;
class VRORenderContext;

typedef NS_ENUM(NSInteger, VROEyeType) {
    VROEyeTypeMonocular,
    VROEyeTypeLeft,
    VROEyeTypeRight,
};

@protocol VRORenderDelegate <NSObject>

- (void)setupRendererWithView:(VROView *)view context:(VRORenderContext *)context;
- (void)shutdownRendererWithView:(VROView *)view;
- (void)renderViewDidChangeSize:(CGSize)size context:(VRORenderContext *)context;

- (void)prepareNewFrameWithHeadViewMatrix:(matrix_float4x4)headViewMatrix;
- (void)renderEye:(VROEyeType)eye context:(VRORenderContext *)renderContext;

@optional

- (void)magneticTriggerPressed;

@end

#endif /* VRORenderDelegate_h */
