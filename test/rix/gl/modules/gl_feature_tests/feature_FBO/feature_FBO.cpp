// Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include <test/testfw/manager/Manager.h>
#include "feature_FBO.h"

#include <dp/util/Image.h>

#include <test/rix/core/framework/RiXBackend.h>
#include <test/rix/core/helpers/GeometryHelper.h>
#include <test/rix/core/helpers/TextureHelper.h>

#include <boost/program_options.hpp>

#include <limits>

//Automatically add the test to the module's global test list
REGISTER_TEST("feature_FBO", "Render to texture with native gl calls", create_feature_FBO);

using namespace dp;
using namespace math;
using namespace rix::core;
using namespace rix::util;

namespace options = boost::program_options;

Feature_FBO::Feature_FBO()
: m_renderData(nullptr)
, m_rix(nullptr)

, m_vertViewProjContainer(nullptr)
, m_fragContainerScreenPass(nullptr)

, m_renderGroupScene(nullptr)
, m_renderGroupSecondPass(nullptr)

, m_containerEntryFBOTexture(0)
, m_containerEntryWorld2view(0)
, m_containerEntryView2world(0)
, m_containerEntryView2clip(0)
, m_containerEntryWorld2clip(0)

, m_fbo(nullptr)

, m_screenshotFBO(false)
, m_screenshotFBOName("")
{
}

Feature_FBO::~Feature_FBO()
{
}

bool Feature_FBO::onInit()
{
  DP_ASSERT( dynamic_cast<test::framework::RiXBackend*>(&(*m_backend)) );
  m_rix = static_cast<test::framework::RiXBackend*>(&(*m_backend))->getRenderer();
  std::static_pointer_cast<dp::gl::RenderTarget>(m_displayTarget)->setClearColor(0.46f, 0.72f, 0.0f, 1.0f);

  m_renderData = new test::framework::RenderDataRiX;

  createCamera();

  createScene();

  createSecondPass();

  DP_ASSERT(m_renderGroupScene);
  DP_ASSERT(m_renderGroupSecondPass);

  return true;
}

bool Feature_FBO::onRunInit( unsigned int i )
{
  dp::gl::RenderTargetSharedPtr rt = std::static_pointer_cast<dp::gl::RenderTarget>(m_fbo);
  rt->setClearMask( gl::TBM_COLOR_BUFFER | gl::TBM_DEPTH_BUFFER );
  rt->setClearColor( 0.46f, 0.72f, 0.0f, 1.0f );
  rt->setClearDepth( 1.0f );

  std::vector<dp::gl::RenderTargetFBO::AttachmentTarget> drawBuffers;
  drawBuffers.push_back(dp::gl::RenderTargetFBO::AttachmentTarget::COLOR0);
  std::static_pointer_cast<dp::gl::RenderTargetFBO>(m_fbo)->setDrawBuffers(drawBuffers);
  m_fbo->setSize( 2*m_width, 2*m_height );

  return true;
}

bool Feature_FBO::onRun( unsigned int idx )
{
  setupCamera( Vec3f(0.0f, 6.0f, 10.0f), Vec3f(0.0f, 0.0f, 0.0f), Vec3f(0.0f, 1.0f, 0.0f) );

  // First Pass

  glEnable(GL_DEPTH_TEST);

  m_renderData->setRenderGroup(m_renderGroupScene);
  render(m_renderData, m_fbo);


  // Second Pass

  glDisable(GL_DEPTH_TEST);

  m_renderData->setRenderGroup(m_renderGroupSecondPass);
  render(m_renderData, m_displayTarget);

  return true;
}

bool Feature_FBO::onClear()
{
  if(m_screenshotFBO)
  {
    dp::util::ImageSharedPtr fboColorShot = m_fbo->getImage(dp::PixelFormat::RGBA, dp::DataType::UNSIGNED_INT_8);
    dp::util::imageToFile(fboColorShot, std::string(CURRENT_MODULE_DIR) + "/feature_FBO/color" + m_screenshotFBOName + ".png" );

    dp::util::ImageSharedPtr fboDepth = dp::rix::util::getEyeZFromDepthBuffer( m_fbo->getImage(dp::PixelFormat::DEPTH_COMPONENT, dp::DataType::FLOAT_32), m_nearPlane, m_farPlane );
    dp::util::imageToFile(fboDepth, std::string(CURRENT_MODULE_DIR) + "/feature_FBO/depth_" + m_screenshotFBOName + ".png" );
  }

  delete m_renderData;

  return true;
}

bool Feature_FBO::onRunCheck( unsigned int i )
{
  return i < 2;
}

bool Feature_FBO::option( const std::vector<std::string>& optionString )
{
  TestRender::option(optionString);


  options::options_description od("Usage: Feature_FBO_MRT");
  od.add_options() ( "screenshotFBO", options::value<std::string>(), "tests to run" );

  options::basic_parsed_options<char> parsedOpts = options::basic_command_line_parser<char>(optionString).options( od ).allow_unregistered().run();

  options::variables_map optsMap;
  options::store( parsedOpts, optsMap );


  if( !optsMap["screenshotFBO"].empty() )
  {
    m_screenshotFBO = true;
    m_screenshotFBOName = optsMap["screenshotFBO"].as<std::string>();
  }
  else
  {
    m_screenshotFBO = false;
  }

  return true;
}

void Feature_FBO::createCamera( void )
{
  // Some Camera data

  m_aspectRatio = float(m_width)/m_height;
  m_nearPlane = 1.0f;
  m_farPlane = 18.0f;
  m_fovy = PI_QUARTER;

  // Program Parameters

  ProgramParameter vertexConstProgramParameters[] = {
    ProgramParameter("g_world2view", ContainerParameterType::MAT4X4),
    ProgramParameter("g_view2world", ContainerParameterType::MAT4X4),
    ProgramParameter("g_view2clip",  ContainerParameterType::MAT4X4),
    ProgramParameter("g_world2clip", ContainerParameterType::MAT4X4)
  };


  // Container Descriptors

  m_vertContainerDescriptorCamera =
    m_rix->containerDescriptorCreate( ProgramParameterDescriptorCommon( vertexConstProgramParameters,
    sizeof testfw::core::array(vertexConstProgramParameters) ) );


  // Container Entries

  m_containerEntryView2clip   = m_rix->containerDescriptorGetEntry( m_vertContainerDescriptorCamera, "g_view2clip" );
  m_containerEntryWorld2view  = m_rix->containerDescriptorGetEntry( m_vertContainerDescriptorCamera, "g_world2view" );
  m_containerEntryView2world  = m_rix->containerDescriptorGetEntry( m_vertContainerDescriptorCamera, "g_view2world" );
  m_containerEntryWorld2clip  = m_rix->containerDescriptorGetEntry( m_vertContainerDescriptorCamera, "g_world2clip" );


  // Container

  m_vertViewProjContainer = m_rix->containerCreate( m_vertContainerDescriptorCamera );

}

/*
Ideal Order of Scene Creation RiX Interaction:

- Geometry

- Shader Code
- Program Parameters

- Container Descriptors
- Container Entries

- Program Descriptors
- Programs
- Program Pipeline

- Containers
- (Local Data)
- Set Container Data

- Geometry Instances
- Render Group

*/

void Feature_FBO::createScene()
{
  // Geometry

  GeometryDataSharedPtr cylinderDataNormal  = createCylinder( AttributeID::POSITION | AttributeID::NORMAL | AttributeID::TEXCOORD0, 64 );
  GeometryDataSharedPtr cylinderDataCut     = createCylinder( AttributeID::POSITION | AttributeID::NORMAL | AttributeID::TEXCOORD0, 64, 8, 3.0f*PI_HALF );
  GeometryDataSharedPtr cylinderDataCutTube = createCylinder( AttributeID::POSITION | AttributeID::NORMAL | AttributeID::TEXCOORD0, 64, 8, 3.0f*PI_HALF, 0.5f );

  GeometrySharedHandle cylinder = rix::util::generateGeometry(cylinderDataNormal, m_rix);
  GeometrySharedHandle cylinderCut = rix::util::generateGeometry(cylinderDataCut, m_rix);
  GeometrySharedHandle cylinderCutTube = rix::util::generateGeometry(cylinderDataCutTube, m_rix);


  // Shader Code

  const char * vertexShader = ""
    "#version 400\n"
    "layout(location=0) in vec3 Position;\n"
    "layout(location=2) in vec3 Normal;\n"
    "layout(location=8) in vec2 TexCoord;\n\n"

    "uniform mat4 model2world;\n"
    "uniform mat4 model2worldIT;\n\n"

    "uniform mat4 g_world2view;\n"
    "uniform mat4 g_view2world;\n"
    "uniform mat4 g_view2clip;\n"
    "uniform mat4 g_world2clip;\n\n"

    "out vec3 vPosition;\n"
    "out vec2 vTexCoord;\n"
    "out vec3 vEyePos;\n"
    "out vec3 vNormal;\n"
    "out vec3 vTangent;\n"
    "out vec3 vBinormal;\n\n"

    "void main(void)\n"
    "{\n"
    "  vec4 worldPos = model2world * vec4( Position, 1.0 );\n\n"

    "  vPosition     = worldPos.xyz;\n"
    "  vTexCoord     = TexCoord;\n"
    "  vNormal       = ( model2worldIT * vec4( Normal, 0.0 ) ).xyz;\n\n"

    "  //Get the translation part of the inverse view matrix\n"
    "  vEyePos       = vec3( g_view2world[3][0], g_view2world[3][1], g_view2world[3][2] );\n"
    "  gl_Position   = g_world2clip * worldPos;\n"
    "}\n";

  ProgramShaderCode vertShader( vertexShader, ShaderType::VERTEX_SHADER );


  const char * fragmentShader = ""
    "#version 400\n"
    "uniform sampler2D diffuseTex;\n"
    "uniform vec3 lightDir;\n\n"

    "in vec3 vPosition;\n"
    "in vec2 vTexCoord;\n"
    "in vec3 vNormal;\n"
    "in vec3 vEyePos;\n"
    "layout(location = 0, index = 0) out vec4 Color;\n\n"

    "// Phong lighting\n"
    "vec3 eval(in vec3 wo, in vec3 ns, in vec3 wi, \n"
    "          in vec3 ambientColor, in vec3 diffuseColor, in vec3 specularColor,\n"
    "          in float ambient, in float diffuse, in float specular, in float exponent)\n"
    "{\n"
    "  float shine = 0.0;\n"
    "  float ns_dot_wi = max(0.0, dot(ns, wi));\n"
    "  if(0.0 < ns_dot_wi)\n"
    "  {\n"
    "  // Phong\n"
    "    vec3 R = reflect(-wi, ns);\n"
    "    float r_dot_wo = max(0.0, dot(R, wo));\n"
    "    shine = 0.0 < exponent ? pow(r_dot_wo, exponent) : 1.0;\n"
    "  }\n"
    "  return ambient * ambientColor +\n"
    "         ns_dot_wi * diffuse * diffuseColor +\n"
    "         shine * specular * specularColor;\n"
    "}\n\n"

    "void main(void)\n"
    "{\n"
    "  vec3 emissiveColor;\n"
    "  vec3 ambientColor;\n"
    "  vec3 diffuseColor;\n"
    "  vec3 specularColor;\n"
    "  float ambient;\n"
    "  float diffuse;\n"
    "  float specular;\n"
    "  float exponent;\n"

    "  emissiveColor = vec3(0.0, 0.0, 0.0);\n"
    "  ambientColor  = vec3(0.0, 0.0, 0.0);\n"
    "  diffuseColor  = texture(diffuseTex, vTexCoord).xyz;\n"
    "  specularColor = vec3(1.0, 1.0, 1.0);\n"
    "  exponent      = 16.0;\n"
    "  ambient       = 0.0;\n"
    "  diffuse       = 1.0;\n"
    "  specular      = 1.0;\n\n"

    "  //Direction from our Eye to the fragment at hand in tangent space\n"
    "  vec3 wo = normalize(vEyePos - vPosition);\n\n"

    "  //Normalized light direction in tangent space\n"
    "  vec3 wi = normalize(lightDir);\n\n"

    "  //Normal at fragment at hand read from the normal map\n"
    "  vec3 ns = normalize(vNormal);\n"
    "  vec3 rgb = eval( wo, ns, wi, \n"
    "              ambientColor, diffuseColor, specularColor, \n"
    "              ambient, diffuse, specular, exponent);\n"
    "  Color = vec4( rgb, 1.0 );\n\n"

    "}\n";

  ProgramShaderCode fragShader( fragmentShader, ShaderType::FRAGMENT_SHADER );


  // Program Parameters

  ProgramParameter vertexVarProgramParameters[] = {
    ProgramParameter("model2world", ContainerParameterType::MAT4X4),
    ProgramParameter("model2worldIT", ContainerParameterType::MAT4X4)
  };

  ProgramParameter fragmentVarProgramParameters[] = {
    ProgramParameter("diffuseTex", ContainerParameterType::SAMPLER)
  };

  ProgramParameter fragmentConstProgramParameters[] = {
    ProgramParameter("lightDir", ContainerParameterType::FLOAT3)
  };


  // Container Descriptors

  ////Descriptors
  ContainerDescriptorSharedHandle fragConstContainerDescriptor =
    m_rix->containerDescriptorCreate( ProgramParameterDescriptorCommon( fragmentConstProgramParameters,
    sizeof testfw::core::array(fragmentConstProgramParameters) ) );

  ContainerDescriptorSharedHandle vertVarContainerDescriptor =
    m_rix->containerDescriptorCreate( ProgramParameterDescriptorCommon( vertexVarProgramParameters,
    sizeof testfw::core::array(vertexVarProgramParameters) ) );

  ContainerDescriptorSharedHandle fragVarContainerDescriptor =
    m_rix->containerDescriptorCreate( ProgramParameterDescriptorCommon( fragmentVarProgramParameters,
    sizeof testfw::core::array(fragmentVarProgramParameters) ) );

  ////Per Shader Stage Arrays
  ContainerDescriptorSharedHandle vertContainerDescriptors[] = { m_vertContainerDescriptorCamera, vertVarContainerDescriptor };
  ContainerDescriptorSharedHandle fragVarContainerDescriptors[] = { fragVarContainerDescriptor, fragConstContainerDescriptor };


  // Container Entries

  //// Global
  m_containerEntryLightDir    = m_rix->containerDescriptorGetEntry( fragConstContainerDescriptor, "lightDir" );

  //// Local
  ContainerEntry containerEntryModel2world = m_rix->containerDescriptorGetEntry( vertVarContainerDescriptor, "model2world" );
  ContainerEntry containerEntryModel2worldIT = m_rix->containerDescriptorGetEntry( vertVarContainerDescriptor, "model2worldIT" );

  ContainerEntry containerEntryTexture     = m_rix->containerDescriptorGetEntry( fragVarContainerDescriptor, "diffuseTex" );


  // Program Descriptors

  ProgramDescription vertProgramDescription( vertShader, vertContainerDescriptors, sizeof testfw::core::array(vertContainerDescriptors) );
  ProgramDescription fragVarProgramDescription( fragShader, fragVarContainerDescriptors, sizeof testfw::core::array(fragVarContainerDescriptors) );


  // Programs

  ProgramSharedHandle vertProgram = m_rix->programCreate( vertProgramDescription );
  ProgramSharedHandle fragVarProgram = m_rix->programCreate( fragVarProgramDescription );


  // Program Pipeline

  ProgramSharedHandle programs[] = {vertProgram, fragVarProgram};
  ProgramPipelineSharedHandle programPipeline = m_rix->programPipelineCreate( programs, sizeof testfw::core::array(programs) );


  // Containers

  //// Local
  ContainerSharedHandle vertCylinderTransformContainer = m_rix->containerCreate( vertVarContainerDescriptor );
  ContainerSharedHandle fragCylinderMaterialContainer  = m_rix->containerCreate( fragVarContainerDescriptor );

  ContainerSharedHandle vertCylinderCutTransformContainer = m_rix->containerCreate( vertVarContainerDescriptor );
  ContainerSharedHandle fragCylinderCutMaterialContainer  = m_rix->containerCreate( fragVarContainerDescriptor );

  ContainerSharedHandle vertCylinderCutTubeTransformContainer = m_rix->containerCreate( vertVarContainerDescriptor );
  ContainerSharedHandle fragCylinderCutTubeMaterialContainer  = m_rix->containerCreate( fragVarContainerDescriptor );

  //// Global
  m_fragConstContainerScene = m_rix->containerCreate( fragConstContainerDescriptor );



  // Local Data

  Trafo model2world0;
  model2world0.setTranslation( Vec3f(-3.0f, 0.0f, 0.0f) );
  Mat44f model2world0IT = ~model2world0.getInverse();

  Trafo model2world1;
  model2world1.setTranslation( Vec3f(0.0f, 0.0f, 0.0f) );
  model2world1.setOrientation( Quatf( Vec3f(0.0f, 1.0f, 0.0), 5.0f*PI_QUARTER) );
  Mat44f model2world1IT = ~model2world1.getInverse();

  Trafo model2world2;
  model2world2.setTranslation( Vec3f(3.0f, 0.0f, 0.0f) );
  model2world2.setOrientation( Quatf( Vec3f(0.0f, 1.0f, 0.0), 4.5f*PI_QUARTER) );
  Mat44f model2world2IT = ~model2world2.getInverse();

  Vec3f lightDirection(-0.5f, 1.0f, 1.0f);

  SamplerStateDataCommon samplerStateDataCommon( SamplerStateFilterMode::NEAREST, SamplerStateFilterMode::NEAREST );
  SamplerStateSharedHandle samplerStateHandle = m_rix->samplerStateCreate(samplerStateDataCommon);
  TextureSharedHandle diffuseMap = dp::rix::util::generateTexture( m_rix, dp::rix::util::createTextureGradient( Vec2ui(128, 128), Vec4f(1.0, 0.0f, 0.0f, 1.0), Vec4f(0.0, 1.0f, 0.0f, 1.0), Vec4f(0.0, 0.0f, 1.0f, 1.0) ), dp::PixelFormat::RGBA, dp::DataType::UNSIGNED_INT_32, InternalTextureFormat::RGBA8 );


  // Set Container Data

  //// Global
  m_rix->containerSetData( m_fragConstContainerScene, m_containerEntryLightDir, ContainerDataRaw( 0, &lightDirection[0], 3*sizeof(float) ) );

  //// Local
  rix::core::SamplerSharedHandle samplerDiffuseMap = m_rix->samplerCreate();
  m_rix->samplerSetTexture( samplerDiffuseMap, diffuseMap );
  m_rix->samplerSetSamplerState( samplerDiffuseMap, samplerStateHandle );

  m_rix->containerSetData( vertCylinderTransformContainer,   containerEntryModel2world,   ContainerDataRaw( 0, model2world0.getMatrix().getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( vertCylinderTransformContainer,   containerEntryModel2worldIT, ContainerDataRaw( 0, model2world0IT.getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( fragCylinderMaterialContainer,    containerEntryTexture,       ContainerDataSampler( samplerDiffuseMap ) );

  m_rix->containerSetData( vertCylinderCutTransformContainer,   containerEntryModel2world,   ContainerDataRaw( 0, model2world1.getMatrix().getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( vertCylinderCutTransformContainer,   containerEntryModel2worldIT, ContainerDataRaw( 0, model2world1IT.getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( fragCylinderCutMaterialContainer,    containerEntryTexture,       ContainerDataSampler( samplerDiffuseMap ) );

  m_rix->containerSetData( vertCylinderCutTubeTransformContainer,   containerEntryModel2world,   ContainerDataRaw( 0, model2world2.getMatrix().getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( vertCylinderCutTubeTransformContainer,   containerEntryModel2worldIT, ContainerDataRaw( 0, model2world2IT.getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( fragCylinderCutTubeMaterialContainer,    containerEntryTexture,       ContainerDataSampler( samplerDiffuseMap ) );


  // Geometry Instances

  GeometryInstanceSharedHandle geometryInstance0 = m_rix->geometryInstanceCreate();
  m_rix->geometryInstanceSetGeometry( geometryInstance0, cylinder );
  m_rix->geometryInstanceSetProgramPipeline( geometryInstance0, programPipeline );
  m_rix->geometryInstanceUseContainer( geometryInstance0, m_vertViewProjContainer );
  m_rix->geometryInstanceUseContainer( geometryInstance0, vertCylinderTransformContainer );
  m_rix->geometryInstanceUseContainer( geometryInstance0, m_fragConstContainerScene );
  m_rix->geometryInstanceUseContainer( geometryInstance0, fragCylinderMaterialContainer );

  GeometryInstanceSharedHandle geometryInstance1 = m_rix->geometryInstanceCreate();
  m_rix->geometryInstanceSetGeometry( geometryInstance1, cylinderCut );
  m_rix->geometryInstanceSetProgramPipeline( geometryInstance1, programPipeline );
  m_rix->geometryInstanceUseContainer( geometryInstance1, m_vertViewProjContainer );
  m_rix->geometryInstanceUseContainer( geometryInstance1, vertCylinderCutTransformContainer );
  m_rix->geometryInstanceUseContainer( geometryInstance1, m_fragConstContainerScene );
  m_rix->geometryInstanceUseContainer( geometryInstance1, fragCylinderCutMaterialContainer );

  GeometryInstanceSharedHandle geometryInstance2 = m_rix->geometryInstanceCreate();
  m_rix->geometryInstanceSetGeometry( geometryInstance2, cylinderCutTube );
  m_rix->geometryInstanceSetProgramPipeline( geometryInstance2, programPipeline );
  m_rix->geometryInstanceUseContainer( geometryInstance2, m_vertViewProjContainer );
  m_rix->geometryInstanceUseContainer( geometryInstance2, vertCylinderCutTubeTransformContainer );
  m_rix->geometryInstanceUseContainer( geometryInstance2, m_fragConstContainerScene );
  m_rix->geometryInstanceUseContainer( geometryInstance2, fragCylinderCutTubeMaterialContainer );


  // Render Group

  m_renderGroupScene = m_rix->renderGroupCreate();
  m_renderData->setRenderGroup(m_renderGroupScene);
  m_rix->renderGroupAddGeometryInstance( m_renderGroupScene, geometryInstance0 );
  m_rix->renderGroupAddGeometryInstance( m_renderGroupScene, geometryInstance1 );
  m_rix->renderGroupAddGeometryInstance( m_renderGroupScene, geometryInstance2 );

}

void Feature_FBO::createSecondPass()
{
  // Geometry

  GeometryDataSharedPtr geoDataScreenQuad = createQuad( AttributeID::POSITION | AttributeID::TEXCOORD0, math::Vec3f(-m_aspectRatio, -1.0f, 0.0f), math::Vec3f(m_aspectRatio, -1.0f, 0.0f), math::Vec3f(-m_aspectRatio, 1.0f, 0.0f) );
  GeometrySharedHandle geoScreenQuad = rix::util::generateGeometry(geoDataScreenQuad, m_rix);

  // Shader Code

  const char * vertexShader = ""
    "#version 400\n"
    "layout(location=0) in vec3 Position;\n"
    "layout(location=8) in vec2 TexCoord;\n\n"

    "uniform mat4 world2clip;\n\n"

    "out vec2 vTexCoord;\n"

    "void main(void)\n"
    "{\n"
    "  vTexCoord     = TexCoord;\n"
    "  gl_Position   = world2clip * vec4( Position, 1.0 );\n"
    "}\n";

  ProgramShaderCode vertShader( vertexShader, ShaderType::VERTEX_SHADER );


  const char * fragmentShader = ""
    "#version 400\n"
    "uniform sampler2D FBOTex;\n"

    "in vec2 vTexCoord;\n"
    "layout(location = 0, index = 0) out vec4 Color;\n\n"

    "void main(void)\n"
    "{\n"
    "  vec3 curSample = texture(FBOTex, vTexCoord).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(0, 1)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(0, -1)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(-1, 0)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(1, 0)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(1, 1)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(-1, 1)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(1, -1)).xyz;"
    "  curSample += textureOffset(FBOTex, vTexCoord, ivec2(-1, -1)).xyz;"
    "  Color = vec4( curSample/9.0, 1.0 );\n"
    "}\n";

  ProgramShaderCode fragShader( fragmentShader, ShaderType::FRAGMENT_SHADER );


  // Program Parameters

  ProgramParameter vertexProgramParameters[] = {
    ProgramParameter("world2clip", ContainerParameterType::MAT4X4)
  };

  ProgramParameter fragmentProgramParameters[] = {
    ProgramParameter("FBOTex", ContainerParameterType::SAMPLER)
  };

  // Container Descriptors

  ////Descriptors

  ContainerDescriptorSharedHandle vertContainerDescriptor =
    m_rix->containerDescriptorCreate( ProgramParameterDescriptorCommon( vertexProgramParameters,
    sizeof testfw::core::array(vertexProgramParameters) ) );

  ContainerDescriptorSharedHandle fragContainerDescriptor =
    m_rix->containerDescriptorCreate( ProgramParameterDescriptorCommon( fragmentProgramParameters,
    sizeof testfw::core::array(fragmentProgramParameters) ) );

  ////Per Shader Stage Arrays
  ContainerDescriptorSharedHandle vertContainerDescriptors[] = { vertContainerDescriptor };
  ContainerDescriptorSharedHandle fragContainerDescriptors[] = { fragContainerDescriptor };


  // Container Entries

  //// Global
  m_containerEntryFBOTexture = m_rix->containerDescriptorGetEntry( fragContainerDescriptor, "FBOTex" );

  //// Local
  ContainerEntry containerEntryScreenProj = m_rix->containerDescriptorGetEntry( vertContainerDescriptor, "world2clip" );


  // Program Descriptors

  ProgramDescription vertProgramDescription( vertShader, vertContainerDescriptors, sizeof testfw::core::array(vertContainerDescriptors) );
  ProgramDescription fragmentProgramDescription( fragShader, fragContainerDescriptors, sizeof testfw::core::array(fragContainerDescriptors) );


  // Programs

  ProgramSharedHandle vertProgram = m_rix->programCreate( vertProgramDescription );
  ProgramSharedHandle fragProgram = m_rix->programCreate( fragmentProgramDescription );


  // Program Pipeline

  ProgramSharedHandle programs[] = {vertProgram, fragProgram};
  ProgramPipelineSharedHandle programPipeline = m_rix->programPipelineCreate( programs, sizeof testfw::core::array(programs) );


  // Containers

  //// Global
  m_fragContainerScreenPass = m_rix->containerCreate( fragContainerDescriptor );

  //// Local
  ContainerSharedHandle vertContainerScreenPass  = m_rix->containerCreate( vertContainerDescriptor );


  // Data

  //// Global
  m_fbo = static_cast<test::framework::RiXBackend*>(&(*m_backend))->createAuxiliaryRenderTarget(2*m_width, 2*m_height);

  //// Local

  //Make a native GL texture for our FBO attachment
  TextureSharedHandle textureFBO;
  {
    TextureDescription textureDescription( TextureType::_2D, InternalTextureFormat::RGBA32F, dp::PixelFormat::RGBA, dp::DataType::FLOAT_32, 2*m_width, 2*m_height );
    textureFBO = m_rix->textureCreate( textureDescription );

    m_colorBuf = gl::Texture2D::create( GL_RGBA32F, GL_RGBA, GL_UNSIGNED_BYTE, 2*m_width, 2*m_height );
    m_depthBuf = gl::Texture2D::create( GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 2*m_width, 2*m_height );

    std::static_pointer_cast<dp::gl::RenderTargetFBO>(m_fbo)->setAttachment(gl::RenderTargetFBO::AttachmentTarget::COLOR0, m_colorBuf);
    std::static_pointer_cast<dp::gl::RenderTargetFBO>(m_fbo)->setAttachment(gl::RenderTargetFBO::AttachmentTarget::DEPTH, m_depthBuf);

    rix::gl::TextureDataGLTexture textureDataGLTexture( m_colorBuf );
    m_rix->textureSetData( textureFBO, textureDataGLTexture );
  }

  Trafo model2worldScreen;
  model2worldScreen.setTranslation( Vec3f(0.0f, 0.0f, 0.0f) );
  Mat44f model2worldScreenIT = ~model2worldScreen.getInverse();

  Mat44f world2ViewI = m_world2View;
  world2ViewI.invert();

  Mat44f viewOrtho = makeOrtho( -m_aspectRatio, m_aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
  Mat44f modelOrtho = cIdentity44f;
  Mat44f modelClip = model2worldScreen.getMatrix() * viewOrtho;


  // Set Container Data
  rix::core::SamplerSharedHandle sampler = m_rix->samplerCreate();
  m_rix->samplerSetTexture( sampler, textureFBO );

  m_rix->containerSetData( vertContainerScreenPass, containerEntryScreenProj, ContainerDataRaw( 0, modelClip.getPtr(),  16*sizeof(float) ) );
  m_rix->containerSetData( m_fragContainerScreenPass, m_containerEntryFBOTexture, ContainerDataSampler( sampler ) );


  // Geometry Instances

  GeometryInstanceSharedHandle geometryInstanceScreen = m_rix->geometryInstanceCreate();
  m_rix->geometryInstanceSetGeometry( geometryInstanceScreen, geoScreenQuad );
  m_rix->geometryInstanceSetProgramPipeline( geometryInstanceScreen, programPipeline );
  m_rix->geometryInstanceUseContainer( geometryInstanceScreen, vertContainerScreenPass );
  m_rix->geometryInstanceUseContainer( geometryInstanceScreen, m_fragContainerScreenPass );


  // Render Group

  m_renderGroupSecondPass = m_rix->renderGroupCreate();
  m_renderData->setRenderGroup(m_renderGroupSecondPass);
  m_rix->renderGroupAddGeometryInstance( m_renderGroupSecondPass, geometryInstanceScreen );
}

void Feature_FBO::setupCamera( dp::math::Vec3f eye, dp::math::Vec3f center, dp::math::Vec3f up )
{
  // Set up world2view matrix
  m_world2View = makeLookAt<float>( eye, center, up );

  // Set up view2clip matrix
  float fovyFactor = tan(0.5f*m_fovy);
  m_view2Clip = makeFrustum<float>( /* LEFT, RIGHT */  -m_nearPlane * fovyFactor * m_aspectRatio, m_nearPlane * fovyFactor * m_aspectRatio
                                    /* BOTTOM, TOP */, -m_nearPlane * fovyFactor                , m_nearPlane * fovyFactor
                                    /* NEAR, FAR   */,  m_nearPlane, m_farPlane);

  // Get the inverse of world2view
  Mat44f world2ViewI = m_world2View;
  world2ViewI.invert();

  // Calculate the world2clip matrix
  Mat44f world2Clip = m_world2View * m_view2Clip;

  // Make sure we set these up previously
  DP_ASSERT(m_containerEntryView2clip);
  DP_ASSERT(m_containerEntryWorld2view);
  DP_ASSERT(m_containerEntryView2world);
  DP_ASSERT(m_containerEntryWorld2clip);

  m_rix->containerSetData( m_vertViewProjContainer,  m_containerEntryView2clip,  ContainerDataRaw( 0, m_view2Clip.getPtr(),   16*sizeof(float) ) );
  m_rix->containerSetData( m_vertViewProjContainer,  m_containerEntryWorld2view, ContainerDataRaw( 0, m_world2View.getPtr(),  16*sizeof(float) ) );
  m_rix->containerSetData( m_vertViewProjContainer,  m_containerEntryView2world, ContainerDataRaw( 0, world2ViewI.getPtr(), 16*sizeof(float) ) );
  m_rix->containerSetData( m_vertViewProjContainer,  m_containerEntryWorld2clip, ContainerDataRaw( 0, world2Clip.getPtr(),  16*sizeof(float) ) );
}
