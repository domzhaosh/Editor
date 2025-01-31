/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org

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
#ifndef _RTShaderSystem_
#define _RTShaderSystem_

#include "OgreShaderProgramManager.h"
#include "OgreShaderProgramWriter.h"
#include "OgreShaderGenerator.h"
#include "OgreShaderRenderState.h"
#include "OgreShaderFFPTransform.h"
#include "OgreShaderFFPColour.h"
#include "OgreShaderFFPLighting.h"
#include "OgreShaderFFPTexturing.h"
#include "OgreShaderFFPFog.h"
#include "OgreShaderExPerPixelLighting.h"
#include "OgreShaderExNormalMapLighting.h"
#include "OgreShaderExIntegratedPSSM3.h"
#include "OgreShaderExLayeredBlending.h"
#include "OgreShaderExHardwareSkinning.h"
#include "OgreShaderMaterialSerializerListener.h"

/** \addtogroup Core
*  @{
*/
/** \addtogroup RTShader
*  @{
*/

/**
* The RT Shader System enables GPU program generation during the runtime of a process.
* The main interface to do that is the ShaderGenerator singleton.
* A typical usage of this system would be to create shader based technique from an existing technique and
* associate it with a destination scheme name. @see ShaderGenerator::createShaderBasedTechnique.
* The source technique must include only fixed function based passes, otherwise the method will fail.
* Once this task accomplished, one may switch the scheme of the current viewport(s) he uses, to 
* the scheme he associate previously, and then a technique based on the original one will be used but this
* time it will be based on GPU programs the were generated by this component.
*
* The following are the highlights applications of this system.
* - Fixed function emulation for Render Systems that supports only shader based rendering. (D3D11 for example).
*   Using this component will allow one to seamlessly use existing FFP techniques on these render systems.
* - Global custom shader based pipeline. One can extended the system to allow creation of custom shader based
*   rendering techniques such as per pixel lighting, bump map, etc and apply it to all of the materials.
*   That approach has the advantages of low maintenance of shader code since the code is the same for all materials instead 
*   of being spread around many different materials and programs. 
* 
*/

/** @} */
/** @} */

#endif
