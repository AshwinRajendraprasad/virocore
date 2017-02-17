//
//  VROTextureSubstrateOpenGL.cpp
//  ViroRenderer
//
//  Created by Raj Advani on 5/2/16.
//  Copyright © 2016 Viro Media. All rights reserved.
//

#include "VROTextureSubstrateOpenGL.h"
#include "VROTexture.h"
#include "VROData.h"
#include "VRODriverOpenGL.h"
#include "VROLog.h"

VROTextureSubstrateOpenGL::VROTextureSubstrateOpenGL(VROTextureType type,
                                                     VROTextureFormat format,
                                                     VROTextureInternalFormat internalFormat,
                                                     VROMipmapMode mipmapMode,
                                                     std::vector<std::shared_ptr<VROData>> &data,
                                                     int width, int height,
                                                     const std::vector<uint32_t> &mipSizes,
                                                     VRODriver &driver) :
    _owned(true) {
    
    loadTexture(type, format, internalFormat, mipmapMode, data, width, height, mipSizes);
    ALLOCATION_TRACKER_ADD(TextureSubstrates, 1);
}

VROTextureSubstrateOpenGL::~VROTextureSubstrateOpenGL() {
    ALLOCATION_TRACKER_SUB(TextureSubstrates, 1);
    if (_owned) {
        glDeleteTextures(1, &_texture);
    }
}

void VROTextureSubstrateOpenGL::loadTexture(VROTextureType type,
                                            VROTextureFormat format,
                                            VROTextureInternalFormat internalFormat,
                                            VROMipmapMode mipmapMode,
                                            std::vector<std::shared_ptr<VROData>> &data,
                                            int width, int height,
                                            const std::vector<uint32_t> &mipSizes) {
 
    _target = GL_TEXTURE_2D;
    
    glGenTextures(1, &_texture);
    glActiveTexture(GL_TEXTURE0);
    
    if (type == VROTextureType::Texture2D) {
        glBindTexture(GL_TEXTURE_2D, _texture);
        
        if (mipmapMode != VROMipmapMode::None) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }
        else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        loadFace(GL_TEXTURE_2D, format, internalFormat,
                 mipmapMode, data.front(), width, height, mipSizes);
    }
    else if (type == VROTextureType::TextureCube) {
        passert_msg (mipmapMode == VROMipmapMode::None,
                     "Cube textures should not be mipmapped!");
        passert_msg (data.size() == 6,
                     "Cube textures can only be created from exactly six images");
        
        _target = GL_TEXTURE_CUBE_MAP;
        
        glBindTexture(GL_TEXTURE_CUBE_MAP, _texture);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        
        for (int slice = 0; slice < 6; ++slice) {
            loadFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X + slice, format, internalFormat,
                     mipmapMode, data[slice], width, height, mipSizes);
        }
    }
    else {
        pabort("Invalid texture data received, could not convert to OpenGL");
    }
}

void VROTextureSubstrateOpenGL::loadFace(GLenum target,
                                         VROTextureFormat format,
                                         VROTextureInternalFormat internalFormat,
                                         VROMipmapMode mipmapMode,
                                         std::shared_ptr<VROData> &faceData,
                                         int width, int height,
                                         const std::vector<uint32_t> &mipSizes) {
    
    if (format == VROTextureFormat::ETC2_RGBA8_EAC) {
        passert (mipmapMode != VROMipmapMode::Runtime);
        
        if (mipmapMode == VROMipmapMode::Pregenerated) {
            uint32_t offset = 0;
            
            for (int level = 0; level < mipSizes.size(); level++) {
                uint32_t mipSize = mipSizes[level];
                glCompressedTexImage2D(target, level, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                       width >> level, height >> level, 0,
                                       mipSize, ((const char *)faceData->getData()) + offset);
                offset += mipSize;
            }
        }
        else { // VROMipmapMode::None
            // Note the data received may have mipmaps, we just might not be using them
            // If no mipsizes are provided, though, then we just use the full data length
            if (!mipSizes.empty()) {
                glCompressedTexImage2D(target, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, width, height, 0,
                                       mipSizes.front(), faceData->getData());
            }
            else {
                glCompressedTexImage2D(target, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, width, height, 0,
                                       faceData->getDataLength(), faceData->getData());
            }
        }
    }
    else if (format == VROTextureFormat::ASTC_4x4_LDR) {
        passert (mipmapMode == VROMipmapMode::None);
        glCompressedTexImage2D(target, 0, GL_COMPRESSED_RGBA_ASTC_4x4_KHR, width, height, 0,
                               faceData->getDataLength(), faceData->getData());
    }
    else if (format == VROTextureFormat::RGBA8) {
        passert (mipmapMode != VROMipmapMode::Pregenerated);
        
        glTexImage2D(target, 0, getInternalFormat(internalFormat), width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, faceData->getData());
        if (mipmapMode == VROMipmapMode::Runtime) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }
    else {
        pabort();
    }
}

GLuint VROTextureSubstrateOpenGL::getInternalFormat(VROTextureInternalFormat format) {
    switch (format) {
        case VROTextureInternalFormat::RGBA8:
            return GL_RGBA;
            
        case VROTextureInternalFormat::RGBA4:
            return GL_RGBA4;
            
        case VROTextureInternalFormat::RGB565:
            return GL_RGB565;
            
        default:
            return GL_RGBA;
    }
}
