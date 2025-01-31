/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2013 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
#ifndef __GLHARDWAREVERTEXBUFFER_H__
#define __GLHARDWAREVERTEXBUFFER_H__

#include "OgreGLPrerequisites.h"
#include "OgreHardwareVertexBuffer.h"

namespace Ogre {

    /// Specialisation of HardwareVertexBuffer for OpenGL
    class _OgreGLExport GLHardwareVertexBuffer : public HardwareVertexBuffer 
    {
    private:
        GLuint mBufferId;
		// Scratch buffer handling
		bool mLockedToScratch;
		size_t mScratchOffset;
		size_t mScratchSize;
		void* mScratchPtr;
		bool mScratchUploadOnUnlock;

    protected:
        /** See HardwareBuffer. */
        void* lockImpl(size_t offset, size_t length, LockOptions options);
        /** See HardwareBuffer. */
        void unlockImpl(void);
    public:
        GLHardwareVertexBuffer(HardwareBufferManagerBase* mgr, size_t vertexSize, size_t numVertices, 
            HardwareBuffer::Usage usage, bool useShadowBuffer); 
        ~GLHardwareVertexBuffer();
        /** See HardwareBuffer. */
        void readData(size_t offset, size_t length, void* pDest);
        /** See HardwareBuffer. */
        void writeData(size_t offset, size_t length, 
            const void* pSource, bool discardWholeBuffer = false);
        /** See HardwareBuffer. */
        void _updateFromShadow(void);

        GLuint getGLBufferId(void) const { return mBufferId; }
    };

}
#endif // __GLHARDWAREVERTEXBUFFER_H__
