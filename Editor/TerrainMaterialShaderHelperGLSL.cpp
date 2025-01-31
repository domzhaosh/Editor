#include "stdafx.h"
#include "TerrainMaterialGeneratorB.h"

#include "OgreTerrain.h"
#include "OgreHighLevelGpuProgramManager.h"
#include "OgreRoot.h"
#include "OgreShadowCameraSetupPSSM.h"
#include "OgreHighLevelGpuProgram.h"

namespace Ogre
{
    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    HighLevelGpuProgramPtr
    TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::createVertexProgram(
        const SM2Profile* prof, const Terrain* terrain, TechniqueType tt)
    {
        HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
        String progName = getVertexProgramName(prof, terrain, tt);

        HighLevelGpuProgramPtr ret = mgr.getByName(progName);
        if (ret.isNull())
        {
            ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
                "glsl", GPT_VERTEX_PROGRAM);
        }
        else
        {
            ret->unload();
        }

        return ret;
    }
    //---------------------------------------------------------------------
    HighLevelGpuProgramPtr
        TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::createFragmentProgram(
            const SM2Profile* prof, const Terrain* terrain, TechniqueType tt)
    {
        HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
        String progName = getFragmentProgramName(prof, terrain, tt);

        HighLevelGpuProgramPtr ret = mgr.getByName(progName);
        if (ret.isNull())
        {
            ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
                "glsl", GPT_FRAGMENT_PROGRAM);
        }
        else
        {
            ret->unload();
        }

        return ret;
    }

    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateVpHeader(const SM2Profile* prof, const Terrain* terrain,
                                                                                   TechniqueType tt, StringStream& outStream)
    {
        outStream << "#version " << Root::getSingleton().getRenderSystem()->getNativeShadingLanguageVersion() << "\n";

        bool compression = terrain->_getUseVertexCompression() && tt != RENDER_COMPOSITE_MAP;
        if (compression)
        {
            outStream <<
                "in vec2 posIndex;\n"
                "in float height;\n";
        }
        else
        {
            outStream <<
                "in vec4 position;\n"
                "in vec2 uv0;\n";
        }
        if (tt != RENDER_COMPOSITE_MAP)
            outStream << "in vec2 delta;\n"; // lodDelta, lodThreshold

        outStream <<
            "uniform mat4 worldMatrix;\n"
            "uniform mat4 viewProjMatrix;\n"
            "uniform vec2 lodMorph;\n"; // morph amount, morph LOD target

        if (compression)
        {
            outStream <<
                "uniform mat4 posIndexToObjectSpace;\n"
                "uniform float baseUVScale;\n";
        }
        // uv multipliers
        uint maxLayers = prof->getMaxLayers(terrain);
        uint numLayers = std::min(maxLayers, static_cast<uint>(terrain->getLayerCount()));
        uint numUVMultipliers = (numLayers / 4);
        if (numLayers % 4)
            ++numUVMultipliers;
        for (uint i = 0; i < numUVMultipliers; ++i)
            outStream << "uniform vec4 uvMul_" << i << ";\n";

        outStream <<
            "out vec4 oPosObj;\n";

        uint texCoordSet = 1;
        outStream <<
            "out vec4 oUVMisc; // xy = uv, z = camDepth\n";

        // layer UV's premultiplied, packed as xy/zw
        uint numUVSets = numLayers / 2;
        if (numLayers % 2)
            ++numUVSets;
        if (tt != LOW_LOD)
        {
            for (uint i = 0; i < numUVSets; ++i)
            {
                outStream <<
                    "out vec4 layerUV" << i << ";\n";
            }
        }

        if (prof->getParent()->getDebugLevel() && tt != RENDER_COMPOSITE_MAP)
        {
            outStream << "out vec2 lodInfo;\n";
        }

        bool fog = terrain->getSceneManager()->getFogMode() != FOG_NONE && tt != RENDER_COMPOSITE_MAP;
        if (fog)
        {
            outStream <<
                "uniform vec4 fogParams;\n"
                "out float fogVal;\n";
        }

        if (prof->isShadowingEnabled(tt, terrain))
        {
            texCoordSet = generateVpDynamicShadowsParams(texCoordSet, prof, terrain, tt, outStream);
        }

        // check we haven't exceeded texture coordinates
        if (texCoordSet > 8)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "Requested options require too many texture coordinate sets! Try reducing the number of layers.",
                        __FUNCTION__);
        }

        outStream << "void main(void) {\n";
        if (compression)
        {
            outStream <<
                "    vec4 position = posIndexToObjectSpace * vec4(posIndex, height, 1);\n"
                "    vec2 uv0 = vec2(posIndex.x * baseUVScale, 1.0 - (posIndex.y * baseUVScale));\n";
        }
        outStream <<
            "    vec4 worldPos = worldMatrix * position;\n"
            "    oPosObj = position;\n";

        if (tt != RENDER_COMPOSITE_MAP)
        {
            // determine whether to apply the LOD morph to this vertex
            // we store the deltas against all vertices so we only want to apply
            // the morph to the ones which would disappear. The target LOD which is
            // being morphed to is stored in lodMorph.y, and the LOD at which
            // the vertex should be morphed is stored in uv.w. If we subtract
            // the former from the latter, and arrange to only morph if the
            // result is negative (it will only be -1 in fact, since after that
            // the vertex will never be indexed), we will achieve our aim.
            // sign(vertexLOD - targetLOD) == -1 is to morph
            outStream <<
                "    float toMorph = -min(0.0, sign(delta.y - lodMorph.y));\n";
            // this will either be 1 (morph) or 0 (don't morph)
            if (prof->getParent()->getDebugLevel())
            {
                // x == LOD level (-1 since value is target level, we want to display actual)
                outStream << "    lodInfo.x = (lodMorph.y - 1) / " << terrain->getNumLodLevels() << ";\n";
                // y == LOD morph
                outStream << "    lodInfo.y = toMorph * lodMorph.x;\n";
            }

            // morph
            switch (terrain->getAlignment())
            {
                case Terrain::ALIGN_X_Y:
                    outStream << "    worldPos.z += delta.x * toMorph * lodMorph.x;\n";
                    break;
                case Terrain::ALIGN_X_Z:
                    outStream << "    worldPos.y += delta.x * toMorph * lodMorph.x;\n";
                    break;
                case Terrain::ALIGN_Y_Z:
                    outStream << "    worldPos.x += delta.x * toMorph * lodMorph.x;\n";
                    break;
            };
        }

        // generate UVs
        if (tt != LOW_LOD)
        {
            for (uint i = 0; i < numUVSets; ++i)
            {
                uint layer  =  i * 2;
                uint uvMulIdx = layer / 4;

                outStream <<
                    "    layerUV" << i << ".xy = " << " uv0.xy * uvMul_" << uvMulIdx << "." << getChannel(layer) << ";\n";
                outStream <<
                    "    layerUV" << i << ".zw = " << " uv0.xy * uvMul_" << uvMulIdx << "." << getChannel(layer+1) << ";\n";
            }
        }
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateFpHeader(const SM2Profile* prof, const Terrain* terrain,
                                                                                   TechniqueType tt, StringStream& outStream)
    {
        // Main header
        outStream <<
            // helpers
            "#version " << Root::getSingleton().getRenderSystem()->getNativeShadingLanguageVersion() << "\n"
            "vec4 expand(vec4 v)\n"
            "{\n"
            "    return v * 2.0 - 1.0;\n"
            "}\n\n"
            // From http://substance.io/zauner/porting-vvvv-hlsl-shaders-to-vvvvjs
            "vec4 lit(float NdotL, float NdotH, float m) {\n"
            "    float ambient = 1.0;\n"
            "    float diffuse = max(0.0, NdotL);\n"
            "    float specular = step(0.0, NdotL) * max(NdotH, 0.0);\n"
            "    return vec4(ambient, diffuse, specular, 1.0);\n"
            "}\n\n";

        if (prof->isShadowingEnabled(tt, terrain))
            generateFpDynamicShadowsHelpers(prof, terrain, tt, outStream);

        outStream << "in vec4 oPosObj;\n"
                     "in vec4 oUVMisc;\n"
                     "out vec4 fragColour;\n";

        uint texCoordSet = 1;

        // UV's premultiplied, packed as xy/zw
        uint maxLayers = prof->getMaxLayers(terrain);
        uint numBlendTextures = std::min(terrain->getBlendTextureCount(maxLayers), terrain->getBlendTextureCount());
        uint numLayers = std::min(maxLayers, static_cast<uint>(terrain->getLayerCount()));
        uint numUVSets = numLayers / 2;
        if (numLayers % 2)
            ++numUVSets;
        if (tt != LOW_LOD)
        {
            for (uint i = 0; i < numUVSets; ++i)
            {
                outStream <<
                    "in vec4 layerUV" << i << ";\n";
            }
        }
        if (prof->getParent()->getDebugLevel() && tt != RENDER_COMPOSITE_MAP)
        {
            outStream << "in vec2 lodInfo;\n";
        }

        bool fog = terrain->getSceneManager()->getFogMode() != FOG_NONE && tt != RENDER_COMPOSITE_MAP;
        if (fog)
        {
            outStream <<
                "uniform vec3 fogColour;\n"
                "in float fogVal;\n";
        }

        uint currentSamplerIdx = 0;

        outStream <<
            // Only 1 light supported in this version
            // deferred shading profile / generator later, ok? :)
            "uniform vec4 lightPosObjSpace;\n"
            "uniform vec3 lightDiffuseColour;\n"
            "uniform vec3 lightSpecularColour;\n"
            "uniform vec3 eyePosObjSpace;\n"
            "uniform vec4 ambient;\n"
            // pack scale, bias and specular
            "uniform vec4 scaleBiasSpecular;\n";

        if (tt == LOW_LOD)
        {
            // single composite map covers all the others below
            outStream <<
                "uniform sampler2D compositeMap;\n";
        }
        else
        {
            outStream <<
                "uniform sampler2D globalNormal;\n";

            if (terrain->getGlobalColourMapEnabled() && prof->isGlobalColourMapEnabled())
            {
                outStream << "uniform sampler2D globalColourMap;\n";
            }
            if (prof->isLightmapEnabled())
            {
                outStream << "uniform sampler2D lightMap;\n";
            }
            // Blend textures - sampler definitions
            for (uint i = 0; i < numBlendTextures; ++i)
            {
                outStream << "uniform sampler2D blendTex" << i << ";\n";
            }

            // Layer textures - sampler definitions & UV multipliers
            for (uint i = 0; i < numLayers; ++i)
            {
                outStream << "uniform sampler2D difftex" << i << ";\n";
                outStream << "uniform sampler2D normtex" << i << ";\n";
            }
        }

        if (prof->isShadowingEnabled(tt, terrain))
        {
            generateFpDynamicShadowsParams(&texCoordSet, &currentSamplerIdx, prof, terrain, tt, outStream);
        }

        // check we haven't exceeded samplers
        if (currentSamplerIdx > 16)
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS,
                        "Requested options require too many texture samplers! Try reducing the number of layers.",
                        __FUNCTION__);
        }

        outStream << "void main(void) {\n"
            "    float shadow = 1.0;\n"
            "    vec2 uv = oUVMisc.xy;\n"
            // base colour
            "    fragColour = vec4(0,0,0,1);\n";

        if (tt != LOW_LOD)
        {
            outStream <<
                // global normal
                "    vec3 normal = expand(texture(globalNormal, uv)).rgb;\n";
        }

        outStream <<
            "    vec3 lightDir = \n"
            "        lightPosObjSpace.xyz - (oPosObj.xyz * lightPosObjSpace.w);\n"
            "    vec3 eyeDir = eyePosObjSpace - oPosObj.xyz;\n"

            // set up accumulation areas
            "    vec3 diffuse = vec3(0,0,0);\n"
            "    float specular = 0.0;\n";

        if (tt == LOW_LOD)
        {
            // we just do a single calculation from composite map
            outStream <<
                "    vec4 composite = texture(compositeMap, uv);\n"
                "    diffuse = composite.rgb;\n";
            // TODO - specular; we'll need normals for this!
        }
        else
        {
            // set up the blend values
            for (uint i = 0; i < numBlendTextures; ++i)
            {
                outStream << "    vec4 blendTexVal" << i << " = texture(blendTex" << i << ", uv);\n";
            }

            if (prof->isLayerNormalMappingEnabled())
            {
                // derive the tangent space basis
                // we do this in the pixel shader because we don't have per-vertex normals
                // because of the LOD, we use a normal map
                // tangent is always +x or -z in object space depending on alignment
                switch(terrain->getAlignment())
                {
                    case Terrain::ALIGN_X_Y:
                    case Terrain::ALIGN_X_Z:
                        outStream << "    vec3 tangent = vec3(1, 0, 0);\n";
                        break;
                    case Terrain::ALIGN_Y_Z:
                        outStream << "    vec3 tangent = vec3(0, 0, -1);\n";
                        break;
                };

                outStream << "    vec3 binormal = normalize(cross(tangent, normal));\n";
                // note, now we need to re-cross to derive tangent again because it wasn't orthonormal
                outStream << "    tangent = normalize(cross(normal, binormal));\n";
                // derive final matrix
                outStream << "    mat3 TBN = mat3(tangent, binormal, normal);\n";

                // set up lighting result placeholders for interpolation
                outStream << "    vec4 litRes, litResLayer;\n";
                outStream << "    vec3 TSlightDir, TSeyeDir, TShalfAngle, TSnormal;\n";
                if (prof->isLayerParallaxMappingEnabled())
                    outStream << "    float displacement;\n";
                // move
                outStream << "    TSlightDir = normalize(TBN * lightDir);\n";
                outStream << "    TSeyeDir = normalize(TBN * eyeDir);\n";
            }
            else
            {
                // simple per-pixel lighting with no normal mapping
                outStream << "    lightDir = normalize(lightDir);\n";
                outStream << "    eyeDir = normalize(eyeDir);\n";
                outStream << "    vec3 halfAngle = normalize(lightDir + eyeDir);\n";
                outStream << "    vec4 litRes = lit(dot(normal, lightDir), dot(normal, halfAngle), scaleBiasSpecular.z);\n";
            }
        }
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateVpLayer(const SM2Profile* prof, const Terrain* terrain,
                                                                                  TechniqueType tt, uint layer, StringStream& outStream)
    {
        // nothing to do
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateFpLayer(const SM2Profile* prof, const Terrain* terrain,
                                                                                  TechniqueType tt, uint layer, StringStream& outStream)
    {
        uint uvIdx = layer / 2;
        String uvChannels = (layer % 2) ? ".zw" : ".xy";
        uint blendIdx = (layer-1) / 4;
        String blendChannel = getChannel(layer-1);
        String blendWeightStr = String("blendTexVal") + StringConverter::toString(blendIdx) + "." + blendChannel;

        // generate early-out conditional
        /* Disable - causing some issues even when trying to force the use of texldd
         if (layer && prof->_isSM3Available())
         outStream << "  if (" << blendWeightStr << " > 0.0003)\n  { \n";
         */

        // generate UV
        outStream << "    vec2 uv" << layer << " = layerUV" << uvIdx << uvChannels << ";\n";

        // calculate lighting here if normal mapping
        if (prof->isLayerNormalMappingEnabled())
        {
            if (prof->isLayerParallaxMappingEnabled() && tt != RENDER_COMPOSITE_MAP)
            {
                // modify UV - note we have to sample an extra time
                outStream << "    displacement = texture(normtex" << layer << ", uv" << layer << ").a\n"
                             "        * scaleBiasSpecular.x + scaleBiasSpecular.y;\n";
                outStream << "    uv" << layer << " += TSeyeDir.xy * displacement;\n";
            }

            // access TS normal map
            outStream << "    TSnormal = expand(texture(normtex" << layer << ", uv" << layer << ")).rgb;\n";
            outStream << "    TShalfAngle = normalize(TSlightDir + TSeyeDir);\n";
            outStream << "    litResLayer = lit(dot(TSnormal, TSlightDir), dot(TSnormal, TShalfAngle), scaleBiasSpecular.z);\n";
            if (!layer)
                outStream << "    litRes = litResLayer;\n";
            else
                outStream << "    litRes = mix(litRes, litResLayer, " << blendWeightStr << ");\n";

        }

        // sample diffuse texture
        outStream << "    vec4 diffuseSpecTex" << layer
            << " = texture(difftex" << layer << ", uv" << layer << ");\n";

        // apply to common
        if (!layer)
        {
            outStream << "    diffuse = diffuseSpecTex0.rgb;\n";
            if (prof->isLayerSpecularMappingEnabled())
                outStream << "    specular = diffuseSpecTex0.a;\n";
        }
        else
        {
            outStream << "    diffuse = mix(diffuse, diffuseSpecTex" << layer
                << ".rgb, " << blendWeightStr << ");\n";
            if (prof->isLayerSpecularMappingEnabled())
                outStream << "    specular = mix(specular, diffuseSpecTex" << layer
                    << ".a, " << blendWeightStr << ");\n";
        }

        // End early-out
        /* Disable - causing some issues even when trying to force the use of texldd
         if (layer && prof->_isSM3Available())
         outStream << "  } // early-out blend value\n";
         */
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateVpFooter(const SM2Profile* prof, const Terrain* terrain,
                                                                                   TechniqueType tt, StringStream& outStream)
    {
        outStream <<
            "    gl_Position = viewProjMatrix * worldPos;\n"
            "    oUVMisc.xy = uv0.xy;\n";

        bool fog = terrain->getSceneManager()->getFogMode() != FOG_NONE && tt != RENDER_COMPOSITE_MAP;
        if (fog)
        {
            if (terrain->getSceneManager()->getFogMode() == FOG_LINEAR)
            {
                outStream <<
                    "    fogVal = clamp((gl_Position.z - fogParams.y) * fogParams.w, 0.0, 1.0);\n";
            }
            else
            {
                outStream <<
                    "    fogVal = 1.0 - clamp(1.0 / (exp(gl_Position.z * fogParams.x)), 0.0, 1.0);\n";
            }
        }

        if (prof->isShadowingEnabled(tt, terrain))
            generateVpDynamicShadows(prof, terrain, tt, outStream);

        outStream << "}\n";
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateFpFooter(const SM2Profile* prof, const Terrain* terrain,
                                                                                   TechniqueType tt, StringStream& outStream)
    {
        if (tt == LOW_LOD)
        {
            if (prof->isShadowingEnabled(tt, terrain))
            {
                generateFpDynamicShadows(prof, terrain, tt, outStream);
                outStream <<
                    "    fragColour.rgb = diffuse * rtshadow;\n";
            }
            else
            {
                outStream <<
                    "    fragColour.rgb = diffuse;\n";
            }
        }
        else
        {
            if (terrain->getGlobalColourMapEnabled() && prof->isGlobalColourMapEnabled())
            {
                // sample colour map and apply to diffuse
                outStream << "    diffuse *= texture(globalColourMap, uv).rgb;\n";
            }
            if (prof->isLightmapEnabled())
            {
                // sample lightmap
                outStream << "    shadow = texture(lightMap, uv).r;\n";
            }

            if (prof->isShadowingEnabled(tt, terrain))
            {
                generateFpDynamicShadows(prof, terrain, tt, outStream);
            }

            // diffuse lighting
            outStream << "    fragColour.rgb += ambient.rgb * diffuse + litRes.y * lightDiffuseColour * diffuse * shadow;\n";

            // specular default
            if (!prof->isLayerSpecularMappingEnabled())
                outStream << "    specular = 1.0;\n";

            if (tt == RENDER_COMPOSITE_MAP)
            {
                // Lighting embedded in alpha
                outStream <<
                    "    fragColour.a = shadow;\n";
            }
            else
            {
                // Apply specular
                outStream << "    fragColour.rgb += litRes.z * lightSpecularColour * specular * shadow;\n";

                if (prof->getParent()->getDebugLevel())
                {
                    outStream << "    fragColour.rg += lodInfo.xy;\n";
                }
            }
        }

        bool fog = terrain->getSceneManager()->getFogMode() != FOG_NONE && tt != RENDER_COMPOSITE_MAP;
        if (fog)
        {
            outStream << "    fragColour.rgb = mix(fragColour.rgb, fogColour, fogVal);\n";
        }

        // Final return
        outStream << "}\n";
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateFpDynamicShadowsHelpers(const SM2Profile* prof, const Terrain* terrain,
                                                                                                  TechniqueType tt, StringStream& outStream)
    {
        // TODO make filtering configurable
        outStream <<
            "// Simple PCF \n"
            "// Number of samples in one dimension (square for total samples) \n"
            "#define NUM_SHADOW_SAMPLES_1D 2.0 \n"
            "#define SHADOW_FILTER_SCALE 1 \n"

            "#define SHADOW_SAMPLES NUM_SHADOW_SAMPLES_1D*NUM_SHADOW_SAMPLES_1D \n"

            "vec4 offsetSample(vec4 uv, vec2 offset, float invMapSize) \n"
            "{ \n"
            "    return vec4(uv.xy + offset * invMapSize * uv.w, uv.z, uv.w); \n"
            "} \n";

        if (prof->getReceiveDynamicShadowsDepth())
        {
            outStream <<
                "float calcDepthShadow(sampler2D shadowMap, vec4 uv, float invShadowMapSize) \n"
                "{ \n"
                "    // 4-sample PCF \n"

                "    float shadow = 0.0; \n"
                "    float offset = (NUM_SHADOW_SAMPLES_1D/2 - 0.5) * SHADOW_FILTER_SCALE; \n"
                "    for (float y = -offset; y <= offset; y += SHADOW_FILTER_SCALE) \n"
                "        for (float x = -offset; x <= offset; x += SHADOW_FILTER_SCALE) \n"
                "        { \n"
                "            vec4 newUV = offsetSample(uv, vec2(x, y), invShadowMapSize);\n"
                "            // manually project and assign derivatives \n"
                "            // to avoid gradient issues inside loops \n"
                "            float depth = textureProjGrad(shadowMap, newUV.xyw, vec2(1.0), vec2(1.0)).x; \n"
                "            if (depth >= 1 || depth >= uv.z)\n"
                "                shadow += 1.0;\n"
                "        } \n"

                "    shadow /= SHADOW_SAMPLES; \n"
                "    return shadow; \n"
                "} \n";
        }
        else
        {
            outStream <<
                "float calcSimpleShadow(sampler2D shadowMap, vec4 shadowMapPos) \n"
                "{ \n"
                "    return textureProj(shadowMap, shadowMapPos).x; \n"
                "} \n";
        }

        if (prof->getReceiveDynamicShadowsPSSM())
        {
            uint numTextures = prof->getReceiveDynamicShadowsPSSM()->getSplitCount();

            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream <<
                    "float calcPSSMDepthShadow(";
            }
            else
            {
                outStream <<
                    "float calcPSSMSimpleShadow(";
            }

            outStream << "\n    ";
            for (uint i = 0; i < numTextures; ++i)
                outStream << "sampler2D shadowMap" << i << ", ";
            outStream << "\n    ";
            for (uint i = 0; i < numTextures; ++i)
                outStream << "vec4 lsPos" << i << ", ";
            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream << "\n    ";
                for (uint i = 0; i < numTextures; ++i)
                    outStream << "float invShadowmapSize" << i << ", ";
            }
            outStream << "\n"
                "    vec4 pssmSplitPoints, float camDepth) \n"
                "    { \n"
                "    float shadow; \n"
                "    // calculate shadow \n";

            for (uint i = 0; i < numTextures; ++i)
            {
                if (!i)
                    outStream << "    if (camDepth <= pssmSplitPoints." << ShaderHelper::getChannel(i) << ") \n";
                else if (i < numTextures - 1)
                    outStream << "    else if (camDepth <= pssmSplitPoints." << ShaderHelper::getChannel(i) << ") \n";
                else
                    outStream << "    else \n";

                outStream <<
                    "    { \n";
                if (prof->getReceiveDynamicShadowsDepth())
                {
                    outStream <<
                        "        shadow = calcDepthShadow(shadowMap" << i << ", lsPos" << i << ", invShadowmapSize" << i << "); \n";
                }
                else
                {
                    outStream <<
                        "        shadow = calcSimpleShadow(shadowMap" << i << ", lsPos" << i << "); \n";
                }
                outStream << "    } \n";
            }

            outStream <<
                "    return shadow; \n"
                "} \n\n\n";
        }
    }
    //---------------------------------------------------------------------
    uint TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateVpDynamicShadowsParams(uint texCoord, const SM2Profile* prof,
                                                                                                 const Terrain* terrain, TechniqueType tt,
                                                                                                 StringStream& outStream)
    {
        // out semantics & params
        uint numTextures = 1;
        if (prof->getReceiveDynamicShadowsPSSM())
        {
            numTextures = prof->getReceiveDynamicShadowsPSSM()->getSplitCount();
        }
        for (uint i = 0; i < numTextures; ++i)
        {
            outStream <<
                "    out vec4 oLightSpacePos" << i << ";\n" <<
                "    uniform mat4 texViewProjMatrix" << i << ";\n";
            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream <<
                    "    uniform vec4 depthRange" << i << "; // x = min, y = max, z = range, w = 1/range \n";
            }
        }

        return texCoord;
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateVpDynamicShadows(const SM2Profile* prof, const Terrain* terrain,
                                                                                           TechniqueType tt, StringStream& outStream)
    {
        uint numTextures = 1;
        if (prof->getReceiveDynamicShadowsPSSM())
        {
            numTextures = prof->getReceiveDynamicShadowsPSSM()->getSplitCount();
        }

        // Calculate the position of vertex in light space
        for (uint i = 0; i < numTextures; ++i)
        {
            outStream <<
                "    oLightSpacePos" << i << " = texViewProjMatrix" << i << " * worldPos; \n";
            if (prof->getReceiveDynamicShadowsDepth())
            {
                // make linear
                outStream <<
                    "oLightSpacePos" << i << ".z = (oLightSpacePos" << i << ".z - depthRange" << i << ".x) * depthRange" << i << ".w;\n";
            }
        }

        if (prof->getReceiveDynamicShadowsPSSM())
        {
            outStream <<
                "    // pass cam depth\n"
                "    oUVMisc.z = gl_Position.z;\n";
        }
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateFpDynamicShadowsParams(uint* texCoord, uint* sampler,
                                                                                                 const SM2Profile* prof, const Terrain* terrain,
                                                                                                 TechniqueType tt, StringStream& outStream)
    {
        if (tt == HIGH_LOD)
            mShadowSamplerStartHi = *sampler;
        else if (tt == LOW_LOD)
            mShadowSamplerStartLo = *sampler;
        
        // in semantics & params
        uint numTextures = 1;
        if (prof->getReceiveDynamicShadowsPSSM())
        {
            numTextures = prof->getReceiveDynamicShadowsPSSM()->getSplitCount();
            outStream <<
                "uniform vec4 pssmSplitPoints;\n";
        }
        for (uint i = 0; i < numTextures; ++i)
        {
            outStream <<
                "in vec4 oLightSpacePos" << i << ";\n" <<
                "uniform sampler2D shadowMap" << i << ";\n";
            *sampler = *sampler + 1;
            *texCoord = *texCoord + 1;
            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream <<
                    "uniform float inverseShadowmapSize" << i << ";\n";
            }
        }
    }
    //---------------------------------------------------------------------
    void TerrainMaterialGeneratorB::SM2Profile::ShaderHelperGLSL::generateFpDynamicShadows(const SM2Profile* prof, const Terrain* terrain,
                                                                                           TechniqueType tt, StringStream& outStream)
    {
        if (prof->getReceiveDynamicShadowsPSSM())
        {
            uint numTextures = prof->getReceiveDynamicShadowsPSSM()->getSplitCount();
            outStream << 
                "    float camDepth = oUVMisc.z;\n";

            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream << 
                    "    float rtshadow = calcPSSMDepthShadow(";
            }
            else
            {
                outStream << 
                    "    float rtshadow = calcPSSMSimpleShadow(";
            }
            for (uint i = 0; i < numTextures; ++i)
                outStream << "shadowMap" << i << ", ";
            outStream << "\n        ";

            for (uint i = 0; i < numTextures; ++i)
                outStream << "oLightSpacePos" << i << ", ";
            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream << "\n        ";
                for (uint i = 0; i < numTextures; ++i)
                    outStream << "inverseShadowmapSize" << i << ", ";
            }
            outStream << "\n" <<
                "        pssmSplitPoints, camDepth);\n";
        }
        else
        {
            if (prof->getReceiveDynamicShadowsDepth())
            {
                outStream <<
                    "    float rtshadow = calcDepthShadow(shadowMap0, oLightSpacePos0, inverseShadowmapSize0);";
            }
            else
            {
                outStream <<
                    "    float rtshadow = calcSimpleShadow(shadowMap0, oLightSpacePos0);";
            }
        }

        outStream << 
            "    shadow = min(shadow, rtshadow);\n";
    }

}
