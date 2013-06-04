// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "shaderGen/GLSL/shaderFeatureGLSL.h"

#include "shaderGen/langElement.h"
#include "shaderGen/shaderOp.h"
#include "shaderGen/shaderGenVars.h"
#include "gfx/gfxDevice.h"
#include "materials/matInstance.h"
#include "materials/processedMaterial.h"
#include "materials/materialFeatureTypes.h"
#include "core/util/autoPtr.h"

#include "lighting/advanced/advancedLightBinManager.h"

LangElement * ShaderFeatureGLSL::setupTexSpaceMat( Vector<ShaderComponent*> &, // componentList
                                                   Var **texSpaceMat )
{
   Var *N = (Var*) LangElement::find( "normal" );
   Var *B = (Var*) LangElement::find( "B" );
   Var *T = (Var*) LangElement::find( "T" );
   
   // setup matrix var
   *texSpaceMat = new Var;
   (*texSpaceMat)->setType( "mat3" );
   (*texSpaceMat)->setName( "objToTangentSpace" );

   MultiLine * meta = new MultiLine;
   
   // Recreate the binormal if we don't have one.
   if ( !B )
   {
      B = new Var;
      B->setType( "vec3" );
      B->setName( "B" );
      meta->addStatement( new GenOp( "   @ = cross( @, normalize(@) );\r\n", new DecOp( B ), T, N ) );
   }

   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( *texSpaceMat ) ) );
   meta->addStatement( new GenOp( "   @[0] = vec3(@.x, @.x, normalize(@).x);\r\n", *texSpaceMat, T, B, N ) );
   meta->addStatement( new GenOp( "   @[1] = vec3(@.y, @.y, normalize(@).y);\r\n", *texSpaceMat, T, B, N ) );
   meta->addStatement( new GenOp( "   @[2] = vec3(@.z, @.z, normalize(@).z);\r\n", *texSpaceMat, T, B, N ) );

   return meta;
}

LangElement* ShaderFeatureGLSL::assignColor( LangElement *elem, 
                                             Material::BlendOp blend, 
                                             LangElement *lerpElem, 
                                             ShaderFeature::OutputTarget outputTarget )
{

   // search for color var
   Var *color = (Var*) LangElement::find( getOutputTargetVarName(outputTarget) );

   if ( !color )
   {
      // create color var
      color = new Var;
      color->setName( getOutputTargetVarName( outputTarget ) );
      color->setType( "vec4" );

      return new GenOp( "@ = @", new DecOp(color), elem );
   }

   LangElement *assign;

   switch ( blend )
   {
      case Material::Add:
         assign = new GenOp( "@ += @", color, elem );
         break;

      case Material::Sub:
         assign = new GenOp( "@ -= @", color, elem );
         break;

      case Material::Mul:
         assign = new GenOp( "@ *= @", color, elem );
         break;

      case Material::AddAlpha:
         assign = new GenOp( "@ += @ * @.a", color, elem, elem );
         break;

      case Material::LerpAlpha:
         if ( !lerpElem )
            lerpElem = elem;
         assign = new GenOp( "@.rgb = mix( @.rgb, (@).rgb, (@).a )", color, elem, color, lerpElem );
         break;
      
      case Material::ToneMap:
         assign = new GenOp( "@ = 1.0 - exp(-1.0 * @ * @)", color, color, elem );
         break;
         
      default:
         AssertFatal(false, "Unrecognized color blendOp");
         // Fallthru

      case Material::None:
         assign = new GenOp( "@ = @", color, elem );
         break;      
   }
  
   return assign;
}


LangElement *ShaderFeatureGLSL::expandNormalMap(   LangElement *sampleNormalOp, 
                                                   LangElement *normalDecl, 
                                                   LangElement *normalVar, 
                                                   const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;

   if ( fd.features.hasFeature( MFT_IsDXTnm, getProcessIndex() ) )
   {
      // DXT Swizzle trick
      meta->addStatement( new GenOp( "   @ = vec4( @.ag * 2.0 - 1.0, 0.0, 0.0 );  // DXTnm\r\n", normalDecl, sampleNormalOp ) );
      meta->addStatement( new GenOp( "   @.z = sqrt( 1.0 - dot( @.xy, @.xy ) );  // DXTnm\r\n", normalVar, normalVar, normalVar ) );      
   }
   else
   {
      meta->addStatement( new GenOp( "   @ = @;\r\n", normalDecl, sampleNormalOp ) );
      meta->addStatement( new GenOp( "   @.xyz = @.xyz * 2.0 - 1.0;\r\n", normalVar, normalVar ) );
   }

   return meta;
}

ShaderFeatureGLSL::ShaderFeatureGLSL()
{
   output = NULL;
}

Var * ShaderFeatureGLSL::getVertTexCoord( const String &name )
{
   Var *inTex = NULL;

   for( U32 i=0; i<LangElement::elementList.size(); i++ )
   {
      if( !dStrcmp( (char*)LangElement::elementList[i]->name, name.c_str() ) )
      {
         inTex = dynamic_cast<Var*>( LangElement::elementList[i] );
			if ( inTex )
			{
				break;
			}
      }
   }

   return inTex;
}

Var* ShaderFeatureGLSL::getOutObjToTangentSpace(   Vector<ShaderComponent*> &componentList,
                                                   MultiLine *meta,
                                                   const MaterialFeatureData &fd )
{
   Var *outObjToTangentSpace = (Var*)LangElement::find( "objToTangentSpace" );
   if ( !outObjToTangentSpace )
      meta->addStatement( setupTexSpaceMat( componentList, &outObjToTangentSpace ) );

   return outObjToTangentSpace;
}

Var* ShaderFeatureGLSL::getOutWorldToTangent(   Vector<ShaderComponent*> &componentList,
															MultiLine *meta,
															const MaterialFeatureData &fd )
{
   Var *outWorldToTangent = (Var*)LangElement::find( "outWorldToTangent" );
   if ( outWorldToTangent )
		return outWorldToTangent;
	
	Var *worldToTangent = (Var*)LangElement::find( "worldToTangent" );
   if ( !worldToTangent )
   {
      Var *texSpaceMat = getOutObjToTangentSpace( componentList, meta, fd );
		
      if(!fd.features[MFT_ParticleNormal])
      {
			// turn obj->tangent into world->tangent
         worldToTangent = new Var;
         worldToTangent->setType( "mat3x3" );
         worldToTangent->setName( "worldToTangent" );
         LangElement *worldToTangentDecl = new DecOp( worldToTangent );
			
         // Get the world->obj transform
         Var *worldToObj = (Var*)LangElement::find( "worldToObj" );
			if ( !worldToObj )
         {
				worldToObj = new Var;
				worldToObj->setName( "worldToObj" );
				worldToObj->setType( "mat4x4" );
				worldToObj->uniform = true;
				worldToObj->constSortPos = cspPrimitive;
			}
			
         // assign world->tangent transform
         meta->addStatement( new GenOp( "   @ = @ * mat3x3( @[0].xyz, @[1].xyz, @[2].xyz );\r\n", worldToTangentDecl, texSpaceMat, worldToObj, worldToObj, worldToObj ) );
      }
      else
      {
         worldToTangent = texSpaceMat;
      }
	}
	
	// send transform to pixel shader
	ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
	
	outWorldToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
	outWorldToTangent->setName( "outWorldToTangent" );
	outWorldToTangent->setType( "mat3x3" );
	meta->addStatement( new GenOp( "   @ = @;\r\n", outWorldToTangent, worldToTangent ) );
	
   return outWorldToTangent;
}

Var* ShaderFeatureGLSL::getOutViewToTangent(   Vector<ShaderComponent*> &componentList,
														  MultiLine *meta,
														  const MaterialFeatureData &fd )
{
   Var *outViewToTangent = (Var*)LangElement::find( "outViewToTangent" );
   if ( outViewToTangent )
		return outViewToTangent;
	
	Var *viewToTangent = (Var*)LangElement::find( "viewToTangent" );
   if ( !viewToTangent )
   {
		Var *texSpaceMat = getOutObjToTangentSpace( componentList, meta, fd );
		
		if(!fd.features[MFT_ParticleNormal])
		{
			
			// turn obj->tangent into world->tangent
			viewToTangent = new Var;
			viewToTangent->setType( "mat3" );
			viewToTangent->setName( "viewToTangent" );
			LangElement *viewToTangentDecl = new DecOp( viewToTangent );
			
			// Get the view->obj transform
			Var *viewToObj = getInvWorldView( componentList, fd.features[MFT_UseInstancing], meta );
			
			// assign world->tangent transform
			meta->addStatement( new GenOp( "   mat3 mat3ViewToObj;\r\n" ) );
			meta->addStatement( new GenOp( "   mat3ViewToObj[0] = @[0].xyz;\r\n", viewToObj ) );
			meta->addStatement( new GenOp( "   mat3ViewToObj[1] = @[1].xyz;\r\n", viewToObj ) );
			meta->addStatement( new GenOp( "   mat3ViewToObj[2] = @[2].xyz;\r\n", viewToObj ) );
			meta->addStatement( new GenOp( "   @ = @ * mat3ViewToObj;\r\n", viewToTangentDecl, texSpaceMat ) );
		}
		else
		{
			// Assume particle normal generation has set this up in the proper space
			viewToTangent = texSpaceMat;
		}
	}
   
	// send transform to pixel shader
	ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
	
	outViewToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
	outViewToTangent->setName( "outViewToTangent" );
	outViewToTangent->setType( "mat3" );
	meta->addStatement( new GenOp( "   @ = @;\r\n", outViewToTangent, viewToTangent ) );
   
   return outViewToTangent;
}

Var* ShaderFeatureGLSL::getOutTexCoord(   const char *name,
                                          const char *type,
                                          bool mapsToSampler,
                                          bool useTexAnim,
                                          MultiLine *meta,
                                          Vector<ShaderComponent*> &componentList )
{
   String outTexName = String::ToString( "out_%s", name );
   Var *texCoord = (Var*)LangElement::find( outTexName );
   if ( !texCoord )
   {
      Var *inTex = getVertTexCoord( name );
      AssertFatal( inTex, "ShaderFeatureGLSL::getOutTexCoord - Unknown vertex input coord!" );

      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

      texCoord = connectComp->getElement( RT_TEXCOORD );
      texCoord->setName( outTexName );
      texCoord->setType( type );
      texCoord->mapsToSampler = mapsToSampler;

      if( useTexAnim )
      {
         inTex->setType( "vec4" );
         
         // create texture mat var
         Var *texMat = new Var;
         texMat->setType( "mat4" );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;      
         
			// Statement allows for casting of different types which
		   // eliminates vector truncation problems.
			String statement = String::ToString( "   @ = %s(@ * @);\r\n", type );
			meta->addStatement( new GenOp( statement , texCoord, texMat, inTex ) );      
      }
      else
		{
			// Statement allows for casting of different types which
		   // eliminates vector truncation problems.
			String statement = String::ToString( "   @ = %s(@);\r\n", type );
         meta->addStatement( new GenOp( statement, texCoord, inTex ) );
		}
	}

   AssertFatal( dStrcmp( type, (const char*)texCoord->type ) == 0, 
      "ShaderFeatureGLSL::getOutTexCoord - Type mismatch!" );

   return texCoord;
}

Var* ShaderFeatureGLSL::getInTexCoord( const char *name,
                                       const char *type,
                                       bool mapsToSampler,
                                       Vector<ShaderComponent*> &componentList )
{
   Var* texCoord = (Var*)LangElement::find( name );
   if ( !texCoord )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector*>( componentList[C_CONNECTOR] );
      texCoord = connectComp->getElement( RT_TEXCOORD );
      texCoord->setName( name );
      texCoord->setType( type );
      texCoord->mapsToSampler = mapsToSampler;
   }

   AssertFatal( dStrcmp( type, (const char*)texCoord->type ) == 0, 
      "ShaderFeatureGLSL::getInTexCoord - Type mismatch!" );

   return texCoord;
}

Var* ShaderFeatureGLSL::getInWorldToTangent( Vector<ShaderComponent*> &componentList )
{
   Var *worldToTangent = (Var*)LangElement::find( "outWorldToTangent" );
   if ( !worldToTangent )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      worldToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      worldToTangent->setName( "outWorldToTangent" );
      worldToTangent->setType( "mat3x3" );
   }

   return worldToTangent;
}

Var* ShaderFeatureGLSL::getInViewToTangent( Vector<ShaderComponent*> &componentList )
{
   Var *viewToTangent = (Var*)LangElement::find( "outViewToTangent" );
   if ( !viewToTangent )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      viewToTangent = connectComp->getElement( RT_TEXCOORD, 1, 3 );
      viewToTangent->setName( "outViewToTangent" );
      viewToTangent->setType( "mat3" );
   }

   return viewToTangent;
}

Var* ShaderFeatureGLSL::getNormalMapTex()
{
   Var *normalMap = (Var*)LangElement::find( "bumpMap" );
   if ( !normalMap )
   {
      normalMap = new Var;
      normalMap->setType( "sampler2D" );
      normalMap->setName( "bumpMap" );
      normalMap->uniform = true;
      normalMap->sampler = true;
      normalMap->constNum = Var::getTexUnitNum();
   }

   return normalMap;
}

Var* ShaderFeatureGLSL::getObjTrans(   Vector<ShaderComponent*> &componentList,
												bool useInstancing,
												MultiLine *meta )
{
   Var *objTrans = (Var*) LangElement::find( "objTrans" );   if ( objTrans )
		return objTrans;        
	objTrans = new Var;        objTrans->setType( "mat4x4" );
	objTrans->setName( "objTrans" );
	objTrans->uniform = true;
	objTrans->constSortPos = cspPrimitive;      
	
	return objTrans;
}

Var* ShaderFeatureGLSL::getModelView(  Vector<ShaderComponent*> &componentList,                                       
												 bool useInstancing,
												 MultiLine *meta )
{
	Var *modelview = (Var*)LangElement::find( "modelview" );
   if ( modelview )
      return modelview;
	
	// create modelview variable
	modelview = new Var;
	modelview->setType( "mat4" );
	modelview->setName( "modelview" );
	modelview->uniform = true;
	modelview->constSortPos = cspPrimitive;
	
	return modelview;
}

Var* ShaderFeatureGLSL::getWorldView(  Vector<ShaderComponent*> &componentList,                                       
												 bool useInstancing,
												 MultiLine *meta )
{
   Var *worldView = (Var*)LangElement::find( "worldViewOnly" );
   if ( worldView )
      return worldView;
	
	worldView = new Var;
	worldView->setType( "mat4x4" );
	worldView->setName( "worldViewOnly" );
	worldView->uniform = true;
	worldView->constSortPos = cspPrimitive;  
	
   return worldView;
}		

Var* ShaderFeatureGLSL::getInvWorldView(  Vector<ShaderComponent*> &componentList,                                       
													 bool useInstancing,
													 MultiLine *meta )
{
   Var *viewToObj = (Var*)LangElement::find( "viewToObj" );
   if ( viewToObj )
      return viewToObj;
	
	viewToObj = new Var;
	viewToObj->setType( "mat4" );
	viewToObj->setName( "viewToObj" );
	viewToObj->uniform = true;
	viewToObj->constSortPos = cspPrimitive;
	
   return viewToObj;
}

void ShaderFeatureGLSL::getWsPosition( Vector<ShaderComponent*> &componentList,                                       
												  bool useInstancing,
												  MultiLine *meta,
												  LangElement *wsPosition )
{
   Var *inPosition = (Var*)LangElement::find( "wsPosition" );
   if ( inPosition )
   {
      meta->addStatement( new GenOp( "   @ = @.xyz;\r\n", 
												wsPosition, inPosition ) );
      return;
   }
	
   // Get the input position.
   inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );
	
   AssertFatal( inPosition, "ShaderFeatureGLSL::getWsPosition - The vertex position was not found!" );
	
   Var *objTrans = getObjTrans( componentList, useInstancing, meta );
	
   meta->addStatement( new GenOp( "   @ = ( @ * vec4( @.xyz, 1 ) ).xyz;\r\n", 
											wsPosition, objTrans, inPosition ) );
}

Var* ShaderFeatureGLSL::addOutWsPosition( Vector<ShaderComponent*> &componentList,                                       
													  bool useInstancing,
													  MultiLine *meta )
{
   Var *outWsPosition = (Var*)LangElement::find( "outWsPosition" );
   if ( !outWsPosition )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      outWsPosition = connectComp->getElement( RT_TEXCOORD );
      outWsPosition->setName( "outWsPosition" );
      outWsPosition->setType( "vec3" );
      outWsPosition->mapsToSampler = false;
		
      getWsPosition( componentList, useInstancing, meta, outWsPosition );
   }
	
   return outWsPosition;
}

Var* ShaderFeatureGLSL::getInWsPosition( Vector<ShaderComponent*> &componentList )
{
   Var *wsPosition = (Var*)LangElement::find( "outWsPosition" );
   if ( !wsPosition )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      wsPosition = connectComp->getElement( RT_TEXCOORD );
      wsPosition->setName( "outWsPosition" );
      wsPosition->setType( "vec3" );
   }
	
   return wsPosition;
}

Var* ShaderFeatureGLSL::getWsView( Var *wsPosition, MultiLine *meta )
{
   Var *wsView = (Var*)LangElement::find( "wsView" );
   if ( !wsView )
   {
      wsView = new Var( "wsView", "vec3" );
		
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var;
         eyePos->setType( "vec3" );
         eyePos->setName( "eyePosWorld" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }
		
      meta->addStatement( new GenOp( "   @ = normalize( @ - @ );\r\n", 
												new DecOp( wsView ), eyePos, wsPosition ) );
   }
	
   return wsView;
}

Var* ShaderFeatureGLSL::addOutDetailTexCoord(   Vector<ShaderComponent*> &componentList, 
															MultiLine *meta,
															bool useTexAnim )
{
	// Check if its already added.
	Var *outTex = (Var*)LangElement::find( "detCoord" );
	if ( outTex )
		return outTex;
	
	// Grab incoming texture coords.
	Var *inTex = getVertTexCoord( "texCoord" );
	
	// create detail variable
	Var *detScale = new Var;
	detScale->setType( "vec2" );
	detScale->setName( "detailScale" );
	detScale->uniform = true;
	detScale->constSortPos = cspPotentialPrimitive;
	
	// grab connector texcoord register
	ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
	outTex = connectComp->getElement( RT_TEXCOORD );
	outTex->setName( "detCoord" );
	outTex->setType( "vec2" );
	outTex->mapsToSampler = true;
	
	if ( useTexAnim )
	{
		inTex->setType( "vec4" );
		
		// Find or create the texture matrix.
		Var *texMat = (Var*)LangElement::find( "texMat" );
		if ( !texMat )
		{
			texMat = new Var;
			texMat->setType( "mat4x4" );
			texMat->setName( "texMat" );
			texMat->uniform = true;
			texMat->constSortPos = cspPass;   
		}
		
		meta->addStatement( new GenOp( "   @ = (@ * @) * @;\r\n", outTex, texMat, inTex, detScale ) );
	}
	else
	{
		// setup output to mul texCoord by detail scale
		meta->addStatement( new GenOp( "   @ = @ * @;\r\n", outTex, inTex, detScale ) );
	}
	
	return outTex;
}

//****************************************************************************
// Base Texture
//****************************************************************************

void DiffuseMapFeatGLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
   getOutTexCoord(   "texCoord", 
                     "vec2", 
                     true, 
                     fd.features[MFT_TexAnim], 
                     meta, 
                     componentList );
   output = meta;
}

void DiffuseMapFeatGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   Var *inTex = getInTexCoord( "out_texCoord", "vec2", true, componentList );

   // create texture var
   Var *diffuseMap = new Var;
   diffuseMap->setType( "sampler2D" );
   diffuseMap->setName( "diffuseMap" );
   diffuseMap->uniform = true;
   diffuseMap->sampler = true;
   diffuseMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   if (  fd.features[MFT_CubeMap] )
   {
      MultiLine * meta = new MultiLine;
      
      // create sample color
      Var *diffColor = new Var;
      diffColor->setType( "vec4" );
      diffColor->setName( "diffuseColor" );
      LangElement *colorDecl = new DecOp( diffColor );
   
      meta->addStatement(  new GenOp( "   @ = texture2D(@, @);\r\n", 
                           colorDecl, 
                           diffuseMap, 
                           inTex ) );
      
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( diffColor, Material::Mul ) ) );
      output = meta;
   }
   else if(fd.features[MFT_DiffuseMapAtlas])
   {   
      // Handle atlased textures
      // http://www.infinity-universe.com/Infinity/index.php?option=com_content&task=view&id=65&Itemid=47
      MultiLine * meta = new MultiLine;
      output = meta;

      Var *atlasedTex = new Var;
      atlasedTex->setName("atlasedTexCoord");
      atlasedTex->setType("float2");
      LangElement *atDecl = new DecOp(atlasedTex);

      // Parameters of the texture atlas
      Var *atParams  = new Var;
      atParams->setType("float4");
      atParams->setName("diffuseAtlasParams");
      atParams->uniform = true;
      atParams->constSortPos = cspPotentialPrimitive;

      // Parameters of the texture (tile) this object is using in the atlas
      Var *tileParams  = new Var;
      tileParams->setType("float4");
      tileParams->setName("diffuseAtlasTileParams");
      tileParams->uniform = true;
      tileParams->constSortPos = cspPotentialPrimitive;

      const bool is_sm3 = (GFX->getPixelShaderVersion() > 2.0f);
      if(is_sm3)
      {
         // Figure out the mip level
         meta->addStatement(new GenOp("   vec2 _dx = ddx(@ * @.z);\r\n", inTex, atParams));
         meta->addStatement(new GenOp("   vec2 _dy = ddy(@ * @.z);\r\n", inTex, atParams));
         meta->addStatement(new GenOp("   float mipLod = 0.5 * log2(max(dot(_dx, _dx), dot(_dy, _dy)));\r\n"));
         meta->addStatement(new GenOp("   mipLod = clamp(mipLod, 0.0, @.w);\r\n", atParams));

         // And the size of the mip level
         meta->addStatement(new GenOp("   float mipPixSz = pow(2.0, @.w - mipLod);\r\n", atParams));
         meta->addStatement(new GenOp("   vec2 mipSz = mipPixSz / @.xy;\r\n", atParams));
      }
      else
      {
         meta->addStatement(new GenOp("   vec2 mipSz = float2(1.0, 1.0);\r\n"));
      }

      // Tiling mode
      // TODO: Select wrap or clamp somehow
      if( true ) // Wrap
         meta->addStatement(new GenOp("   @ = fract(@);\r\n", atDecl, inTex));
      else       // Clamp
         meta->addStatement(new GenOp("   @ = saturate(@);\r\n", atDecl, inTex));

      // Finally scale/offset, and correct for filtering
      meta->addStatement(new GenOp("   @ = @ * ((mipSz * @.xy - 1.0) / mipSz) + 0.5 / mipSz + @.xy * @.xy;\r\n", 
         atlasedTex, atlasedTex, atParams, atParams, tileParams));

      // Add a newline
      meta->addStatement(new GenOp( "\r\n"));

      // For the rest of the feature...
      inTex = atlasedTex;

      // create sample color var
      Var *diffColor = new Var;
      diffColor->setType("vec4");
      diffColor->setName("diffuseColor");

      // To dump out UV coords...
      //#define DEBUG_ATLASED_UV_COORDS
#ifdef DEBUG_ATLASED_UV_COORDS
      if(!fd.features[MFT_PrePassConditioner])
      {
         meta->addStatement(new GenOp("   @ = vec4(@.xy, mipLod / @.w, 1.0);\r\n", new DecOp(diffColor), inTex, atParams));
         meta->addStatement(new GenOp("   @; return OUT;\r\n", assignColor(diffColor, Material::Mul)));
         return;
      }
#endif

      if(is_sm3)
      {
         meta->addStatement(new GenOp( "   @ = texture2Dlod(@, float4(@, 0.0, mipLod));\r\n", 
            new DecOp(diffColor), diffuseMap, inTex));
      }
      else
      {
         meta->addStatement(new GenOp( "   @ = texture2D(@, @);\r\n", 
            new DecOp(diffColor), diffuseMap, inTex));
      }

      meta->addStatement(new GenOp( "   @;\r\n", assignColor(diffColor, Material::Mul)));
   }
   else
   {
      LangElement *statement = new GenOp( "texture2D(@, @)", diffuseMap, inTex );
      output = new GenOp( "   @;\r\n", assignColor( statement, Material::Mul ) );
   }
   
}

ShaderFeature::Resources DiffuseMapFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void DiffuseMapFeatGLSL::setTexData(   Material::StageData &stageDat,
                                       const MaterialFeatureData &fd,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_DiffuseMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Overlay Texture
//****************************************************************************

void OverlayTexFeatGLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   Var *inTex = getVertTexCoord( "texCoord2" );
   AssertFatal( inTex, "OverlayTexFeatGLSL::processVert() - The second UV set was not found!" );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "outTexCoord2" );
   outTex->setType( "vec2" );
   outTex->mapsToSampler = true;

   if( fd.features[MFT_TexAnim] )
   {
      inTex->setType( "vec4" );

      // Find or create the texture matrix.
      Var *texMat = (Var*)LangElement::find( "texMat" );
      if ( !texMat )
      {
         texMat = new Var;
         texMat->setType( "mat4x4" );
         texMat->setName( "texMat" );
         texMat->uniform = true;
         texMat->constSortPos = cspPass;   
      }
     
      output = new GenOp( "   @ = @ * @;\r\n", outTex, texMat, inTex );
      return;
   }
   
   // setup language elements to output incoming tex coords to output
   output = new GenOp( "   @ = @;\r\n", outTex, inTex );
}

void OverlayTexFeatGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "outTexCoord2" );
   inTex->setType( "vec2" );
   inTex->mapsToSampler = true;

   // create texture var
   Var *diffuseMap = new Var;
   diffuseMap->setType( "sampler2D" );
   diffuseMap->setName( "overlayMap" );
   diffuseMap->uniform = true;
   diffuseMap->sampler = true;
   diffuseMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   LangElement *statement = new GenOp( "texture2D(@, @)", diffuseMap, inTex );
   output = new GenOp( "   @;\r\n", assignColor( statement, Material::LerpAlpha ) );
}

ShaderFeature::Resources OverlayTexFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;
   return res;
}

void OverlayTexFeatGLSL::setTexData(   Material::StageData &stageDat,
                                       const MaterialFeatureData &fd,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_OverlayMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Diffuse color
//****************************************************************************

void DiffuseFeatureGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   Var* diffuseMaterialColor  = new Var;
   diffuseMaterialColor->setType( "vec4" );
   diffuseMaterialColor->setName( "diffuseMaterialColor" );
   diffuseMaterialColor->uniform = true;
   diffuseMaterialColor->constSortPos = cspPotentialPrimitive;

   MultiLine* meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( diffuseMaterialColor, Material::Mul ) ) );
   output = meta;
}


//****************************************************************************
// Diffuse vertex color
//****************************************************************************

void DiffuseVertColorFeatureGLSL::processVert(  Vector< ShaderComponent* >& componentList, 
                                                const MaterialFeatureData& fd )
{
   // Create vertex color connector if it doesn't exist.
   
   Var* outColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !outColor )
   {
      // Search for vert color.
      
      Var* inColor = dynamic_cast< Var* >( LangElement::find( "diffuse" ) );
      if( !inColor )
      {
         output = NULL;
         return;
      }
      
      // Create connector.

      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[ C_CONNECTOR ] );
      AssertFatal( connectComp, "DiffuseVertColorFeatureGLSL::processVert - C_CONNECTOR is not a ShaderConnector" );
      Var* outColor = connectComp->getElement( RT_COLOR );
      outColor->setName( "vertColor" );
      outColor->setType( "vec4" );

      output = new GenOp( "   @ = @;\r\n", outColor, inColor );
   }
   else
      output = NULL; // Nothing we need to do.
}

void DiffuseVertColorFeatureGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                                const MaterialFeatureData &fd )
{
   Var* vertColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !vertColor )
   {
      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[ C_CONNECTOR ] );
      AssertFatal( connectComp, "DiffuseVertColorFeatureGLSL::processVert - C_CONNECTOR is not a ShaderConnector" );
      vertColor = connectComp->getElement( RT_COLOR );
      vertColor->setName( "vertColor" );
      vertColor->setType( "vec4" );
   }
   
   MultiLine* meta = new MultiLine;
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( vertColor, Material::Mul ) ) );
   output = meta;
}


//****************************************************************************
// Lightmap
//****************************************************************************

void LightmapFeatGLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab tex register from incoming vert
   Var *inTex = (Var*) LangElement::find( "texCoord2" );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outTex = connectComp->getElement( RT_TEXCOORD );
   outTex->setName( "outTexCoord2" );
   outTex->setType( "vec2" );
   outTex->mapsToSampler = true;

   // setup language elements to output incoming tex coords to output
   output = new GenOp( "   @ = @;\r\n", outTex, inTex );
}

void LightmapFeatGLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *inTex = connectComp->getElement( RT_TEXCOORD );
   inTex->setName( "outTexCoord2" );
   inTex->setType( "vec2" );
   inTex->mapsToSampler = true;

   // create texture var
   Var *lightMap = new Var;
   lightMap->setType( "sampler2D" );
   lightMap->setName( "lightMap" );
   lightMap->uniform = true;
   lightMap->sampler = true;
   lightMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   
   // argh, pixel specular should prob use this too
   if( fd.features[MFT_NormalMap] )
   {
      Var *lmColor = new Var;
      lmColor->setName( "lmColor" );
      lmColor->setType( "vec4" );
      LangElement *lmColorDecl = new DecOp( lmColor );
      
      output = new GenOp( "   @ = texture2D(@, @);\r\n", lmColorDecl, lightMap, inTex );
      return;
   }
   
   // Add realtime lighting, if it is available
   LangElement *statement = NULL;
   if( fd.features[MFT_RTLighting] )
   {
      // Advanced lighting is the only dynamic lighting supported right now
      Var *inColor = (Var*) LangElement::find( "d_lightcolor" );
      if(inColor != NULL)
      {
         // Find out if RTLighting should be added or substituted
         bool bPreProcessedLighting = false;
         AdvancedLightBinManager *lightBin;
         if ( Sim::findObject( "AL_LightBinMgr", lightBin ) )
            bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();

         // Lightmap has already been included in the advanced light bin, so
         // no need to do any sampling or anything
         if(bPreProcessedLighting)
            statement = new GenOp( "vec4(@, 1.0)", inColor );
         else
            statement = new GenOp( "texture2D(@, @) + vec4(@.rgb, 0.0)", lightMap, inTex, inColor );
      }
   }
   else
   {
      statement = new GenOp( "texture2D(@, @)", lightMap, inTex );
   }
   
   // Assign to proper render target
   if( fd.features[MFT_LightbufferMRT] )
      output = new GenOp( "   @;\r\n", assignColor( statement, Material::None, NULL, ShaderFeature::RenderTarget1 ) );
   else
      output = new GenOp( "   @;\r\n", assignColor( statement, Material::Mul ) );
}

ShaderFeature::Resources LightmapFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void LightmapFeatGLSL::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_LightMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   else
      passData.mTexType[ texIndex++ ] = Material::Lightmap;
}

U32 LightmapFeatGLSL::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// Tonemap
//****************************************************************************

void TonemapFeatGLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // Grab the connector
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   // Set up the second set of texCoords
   Var *inTex2 = getVertTexCoord( "texCoord2" );

   if ( inTex2 )
   {
      Var *outTex2 = connectComp->getElement( RT_TEXCOORD );
      outTex2->setName( "toneMapCoord" );
      outTex2->setType( "vec2" );
      outTex2->mapsToSampler = true;

      output = new GenOp( "   @ = @;\r\n", outTex2, inTex2 );
   }
}

void TonemapFeatGLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // Grab connector
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   Var *inTex2 = connectComp->getElement( RT_TEXCOORD );
   inTex2->setName( "toneMapCoord" );
   inTex2->setType( "vec2" );
   inTex2->mapsToSampler = true;

   // create texture var
   Var *toneMap = new Var;
   toneMap->setType( "sampler2D" );
   toneMap->setName( "toneMap" );
   toneMap->uniform = true;
   toneMap->sampler = true;
   toneMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   MultiLine * meta = new MultiLine;

   // First get the toneMap color
   Var *toneMapColor = new Var;
   toneMapColor->setType( "vec4" );
   toneMapColor->setName( "toneMapColor" );
   LangElement *toneMapColorDecl = new DecOp( toneMapColor );

   meta->addStatement( new GenOp( "   @ = texture2D(@, @);\r\n", toneMapColorDecl, toneMap, inTex2 ) );

   // We do a different calculation if there is a diffuse map or not
   Material::BlendOp blendOp = Material::Mul;
   if ( fd.features[MFT_DiffuseMap] )
   {
      // Reverse the tonemap
      meta->addStatement( new GenOp( "   @ = -1.0 * log(1.0 - @);\r\n", toneMapColor, toneMapColor ) );

      // Re-tonemap with the current color factored in
      blendOp = Material::ToneMap;
   }

   // Find out if RTLighting should be added
   bool bPreProcessedLighting = false;
   AdvancedLightBinManager *lightBin;
   if ( Sim::findObject( "AL_LightBinMgr", lightBin ) )
      bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();
   
   // Add in the realtime lighting contribution
   if ( fd.features[MFT_RTLighting] )
   {
      // Right now, only Advanced Lighting is supported
      Var *inColor = (Var*) LangElement::find( "d_lightcolor" );
      if(inColor != NULL)
      {
         // Assign value in d_lightcolor to toneMapColor if it exists. This is
         // the dynamic light buffer, and it already has the tonemap included
         if(bPreProcessedLighting)
            meta->addStatement( new GenOp( "   @.rgb = @;\r\n", toneMapColor, inColor ) );
         else
            meta->addStatement( new GenOp( "   @.rgb += @.rgb;\r\n", toneMapColor, inColor ) );
      }
   }

   // Assign to proper render target
   if( fd.features[MFT_LightbufferMRT] )
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( toneMapColor, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( toneMapColor, blendOp ) ) );
   
   output = meta;
}

ShaderFeature::Resources TonemapFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void TonemapFeatGLSL::setTexData(  Material::StageData &stageDat,
                                    const MaterialFeatureData &fd,
                                    RenderPassData &passData,
                                    U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_ToneMap );
   if ( tex )
   {
      passData.mTexType[ texIndex ] = Material::ToneMapTex;
      passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}

U32 TonemapFeatGLSL::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}

//****************************************************************************
// pureLIGHT Lighting
//****************************************************************************

void VertLitGLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   // If we have a lightMap or toneMap then our lighting will be
   // handled by the MFT_LightMap or MFT_ToneNamp feature instead
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] )
   {
      output = NULL;
      return;
   }

   // Create vertex color connector if it doesn't exist.
   
   Var* outColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !outColor )
   {
      // Search for vert color
      Var *inColor = (Var*) LangElement::find( "diffuse" );   

      // If there isn't a vertex color then we can't do anything
      if( !inColor )
      {
         output = NULL;
         return;
      }

      // Grab the connector color
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *outColor = connectComp->getElement( RT_COLOR );
      outColor->setName( "vertColor" );
      outColor->setType( "vec4" );

      output = new GenOp( "   @ = @;\r\n", outColor, inColor );
   }
   else
      output = NULL; // Nothing we need to do.
}

void VertLitGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // If we have a lightMap or toneMap then our lighting will be
   // handled by the MFT_LightMap or MFT_ToneNamp feature instead
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] )
   {
      output = NULL;
      return;
   }
   
   // Grab the connector color register
   Var* vertColor = dynamic_cast< Var* >( LangElement::find( "vertColor" ) );
   if( !vertColor )
   {
      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[ C_CONNECTOR ] );
      AssertFatal( connectComp, "VertLitGLSL::processVert - C_CONNECTOR is not a ShaderConnector" );
      vertColor = connectComp->getElement( RT_COLOR );
      vertColor->setName( "vertColor" );
      vertColor->setType( "vec4" );
   }

   MultiLine * meta = new MultiLine;
   
   // Defaults (no diffuse map)
   Material::BlendOp blendOp = Material::Mul;
   LangElement *outColor = vertColor;

   // We do a different calculation if there is a diffuse map or not
   if ( fd.features[MFT_DiffuseMap] || fd.features[MFT_VertLitTone] )
   {
      Var * finalVertColor = new Var;
      finalVertColor->setName( "finalVertColor" );
      finalVertColor->setType( "vec4" );
      LangElement *finalVertColorDecl = new DecOp( finalVertColor );
      
      // Reverse the tonemap
      meta->addStatement( new GenOp( "   @ = -1.0 * log(1.0 - @);\r\n", finalVertColorDecl, vertColor ) );
      
      // Set the blend op to tonemap
      blendOp = Material::ToneMap;
      outColor = finalVertColor;
      
   }
  
   // Add in the realtime lighting contribution, if applicable
   if ( fd.features[MFT_RTLighting] )
   {
      Var *rtLightingColor = (Var*) LangElement::find( "d_lightcolor" );
      if(rtLightingColor != NULL)
      {
         // Find out if RTLighting should be added or substituted
         bool bPreProcessedLighting = false;
         AdvancedLightBinManager *lightBin;
         if ( Sim::findObject( "AL_LightBinMgr", lightBin ) )
            bPreProcessedLighting = lightBin->MRTLightmapsDuringPrePass();
         
         // Assign value in d_lightcolor to toneMapColor if it exists. This is
         // the dynamic light buffer, and it already has the baked-vertex-color 
         // included in it
         if(bPreProcessedLighting)
            outColor = new GenOp( "vec4(@.rgb, 1.0)", rtLightingColor );
         else
            outColor = new GenOp( "vec4(@.rgb, 0.0) + @", rtLightingColor, outColor );
      }
   }
   
   // Output the color
   if ( fd.features[MFT_LightbufferMRT] )
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( outColor, Material::None, NULL, ShaderFeature::RenderTarget1 ) ) );
   else
      meta->addStatement( new GenOp( "   @;\r\n", assignColor( outColor, blendOp ) ) );
   
   output = meta;
}

U32 VertLitGLSL::getOutputTargets( const MaterialFeatureData &fd ) const
{
   return fd.features[MFT_LightbufferMRT] ? ShaderFeature::RenderTarget1 : ShaderFeature::DefaultTarget;
}


//****************************************************************************
// Detail map
//****************************************************************************

void DetailFeatGLSL::processVert(   Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
	MultiLine *meta = new MultiLine;
	addOutDetailTexCoord( componentList, 
								meta,
								fd.features[MFT_TexAnim] );
	output = meta;
}

void DetailFeatGLSL::processPix( Vector<ShaderComponent*> &componentList, 
                                 const MaterialFeatureData &fd )
{
   // Get the detail texture coord.
   Var *inTex = getInTexCoord( "detCoord", "vec2", true, componentList );

   // create texture var
   Var *detailMap = new Var;
   detailMap->setType( "sampler2D" );
   detailMap->setName( "detailMap" );
   detailMap->uniform = true;
   detailMap->sampler = true;
   detailMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   // We're doing the standard greyscale detail map
   // technique which can darken and lighten the 
   // diffuse texture.

   // TODO: We could add a feature to toggle between this
   // and a simple multiplication with the detail map.

   LangElement *statement = new GenOp( "( texture2D(@, @) * 2.0 ) - 1.0", detailMap, inTex );
   output = new GenOp( "   @;\r\n", assignColor( statement, Material::Add ) );
}

ShaderFeature::Resources DetailFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 
   res.numTex = 1;
   res.numTexReg = 1;

   return res;
}

void DetailFeatGLSL::setTexData( Material::StageData &stageDat,
                                 const MaterialFeatureData &fd,
                                 RenderPassData &passData,
                                 U32 &texIndex )
{
   GFXTextureObject *tex = stageDat.getTex( MFT_DetailMap );
   if ( tex )
      passData.mTexSlot[ texIndex++ ].texObject = tex;
}


//****************************************************************************
// Vertex position
//****************************************************************************

void VertPositionGLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                    const MaterialFeatureData &fd )
{
   // First check for an input position from a previous feature
   // then look for the default vertex position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

   // grab connector position
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outPosition = connectComp->getElement( RT_POSITION );
   outPosition->setName( "gl_Position" );
   
   MultiLine *meta = new MultiLine;
	
	Var *modelview = getModelView( componentList, fd.features[MFT_UseInstancing], meta );
   
	meta->addStatement( new GenOp( "   @ = @ * vec4(@.xyz,1);\r\n", outPosition, modelview, inPosition ) );
   
	output = meta;
}


//****************************************************************************
// Reflect Cubemap
//****************************************************************************

void ReflectCubeFeatGLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine * meta = new MultiLine;

   // If a base or bump tex is present in the material, but not in the
   // current pass - we need to add one to the current pass to use
   // its alpha channel as a gloss map.  Here we just need the tex coords.
   if( !fd.features[MFT_DiffuseMap] &&
       !fd.features[MFT_NormalMap] )
   {
      if( fd.materialFeatures[MFT_DiffuseMap] ||
          fd.materialFeatures[MFT_NormalMap] )
      {
         // find incoming texture var
         Var *inTex = getVertTexCoord( "texCoord" );

         // grab connector texcoord register
         ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
         Var *outTex = connectComp->getElement( RT_TEXCOORD );
         outTex->setName( "outTexCoord" );
         outTex->setType( "vec2" );
         outTex->mapsToSampler = true;

         // setup language elements to output incoming tex coords to output
         meta->addStatement( new GenOp( "   @ = @;\r\n", outTex, inTex ) );
      }
   }

   // create cubeTrans
   Var *cubeTrans = new Var;
   cubeTrans->setType( "mat3" );
   cubeTrans->setName( "cubeTrans" );
   cubeTrans->uniform = true;
   cubeTrans->constSortPos = cspPrimitive;   

   // create cubeEye position
   Var *cubeEyePos = new Var;
   cubeEyePos->setType( "vec3" );
   cubeEyePos->setName( "cubeEyePos" );
   cubeEyePos->uniform = true;
   cubeEyePos->constSortPos = cspPrimitive;   

   // search for vert normal
   Var *inNormal = (Var*) LangElement::find( "normal" );

   // cube vert position
   Var * cubeVertPos = new Var;
   cubeVertPos->setName( "cubeVertPos" );
   cubeVertPos->setType( "vec3" );
   LangElement *cubeVertPosDecl = new DecOp( cubeVertPos );

   meta->addStatement( new GenOp( "   @ = @ * @.xyz;\r\n", 
                       cubeVertPosDecl, cubeTrans, LangElement::find( "position" ) ) );

   // cube normal
   Var * cubeNormal = new Var;
   cubeNormal->setName( "cubeNormal" );
   cubeNormal->setType( "vec3" );
   LangElement *cubeNormDecl = new DecOp( cubeNormal );

   meta->addStatement( new GenOp( "   @ = normalize( @ * normalize(@).xyz );\r\n", 
                       cubeNormDecl, cubeTrans, inNormal ) );

   // eye to vert
   Var * eyeToVert = new Var;
   eyeToVert->setName( "eyeToVert" );
   eyeToVert->setType( "vec3" );
   LangElement *e2vDecl = new DecOp( eyeToVert );

   meta->addStatement( new GenOp( "   @ = @ - @;\r\n", 
                       e2vDecl, cubeVertPos, cubeEyePos ) );

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *reflectVec = connectComp->getElement( RT_TEXCOORD );
   reflectVec->setName( "reflectVec" );
   reflectVec->setType( "vec3" );
   reflectVec->mapsToSampler = true;

   meta->addStatement( new GenOp( "   @ = reflect(@, @);\r\n", reflectVec, eyeToVert, cubeNormal ) );

   output = meta;
}

void ReflectCubeFeatGLSL::processPix(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   MultiLine * meta = new MultiLine;
   Var *glossColor = NULL;
   
   // If a base or bump tex is present in the material, but not in the
   // current pass - we need to add one to the current pass to use
   // its alpha channel as a gloss map.
   if( !fd.features[MFT_DiffuseMap] &&
       !fd.features[MFT_NormalMap] )
   {
      if( fd.materialFeatures[MFT_DiffuseMap] ||
          fd.materialFeatures[MFT_NormalMap] )
      {
         // grab connector texcoord register
         Var *inTex = getInTexCoord( "outTexCoord", "vec2", true, componentList );
      
         // create texture var
         Var *newMap = new Var;
         newMap->setType( "sampler2D" );
         newMap->setName( "glossMap" );
         newMap->uniform = true;
         newMap->sampler = true;
         newMap->constNum = Var::getTexUnitNum();     // used as texture unit num here
      
         // create sample color
         Var *color = new Var;
         color->setType( "vec4" );
         color->setName( "diffuseColor" );
         LangElement *colorDecl = new DecOp( color );

         glossColor = color;
         
         meta->addStatement( new GenOp( "   @ = texture2D( @, @ );\r\n", colorDecl, newMap, inTex ) );
      }
   }
   else
   {
      glossColor = (Var*) LangElement::find( "diffuseColor" );
      if( !glossColor )
         glossColor = (Var*) LangElement::find( "bumpNormal" );
   }

   // grab connector texcoord register
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *reflectVec = connectComp->getElement( RT_TEXCOORD );
   reflectVec->setName( "reflectVec" );
   reflectVec->setType( "vec3" );
   reflectVec->mapsToSampler = true;

   // create cubemap var
   Var *cubeMap = new Var;
   cubeMap->setType( "samplerCube" );
   cubeMap->setName( "cubeMap" );
   cubeMap->uniform = true;
   cubeMap->sampler = true;
   cubeMap->constNum = Var::getTexUnitNum();     // used as texture unit num here

   // TODO: Restore the lighting attenuation here!
   Var *attn = NULL;
   //if ( fd.materialFeatures[MFT_DynamicLight] )
	   //attn = (Var*)LangElement::find("attn");
   //else 
      if ( fd.materialFeatures[MFT_RTLighting] )
      attn =(Var*)LangElement::find("d_NL_Att");

   LangElement *texCube = new GenOp( "textureCube( @, @ )", cubeMap, reflectVec );
   LangElement *lerpVal = NULL;
   Material::BlendOp blendOp = Material::LerpAlpha;

   // Note that the lerpVal needs to be a float4 so that
   // it will work with the LerpAlpha blend.

   if ( glossColor )
   {
      if ( attn )
         lerpVal = new GenOp( "@ * saturate( @ )", glossColor, attn );
      else
         lerpVal = glossColor;
   }
   else
   {
      if ( attn )
         lerpVal = new GenOp( "saturate( @ ).xxxx", attn );
      else
         blendOp = Material::Mul;
   }

   meta->addStatement( new GenOp( "   @;\r\n", assignColor( texCube, blendOp, lerpVal ) ) );         
   output = meta;
}

ShaderFeature::Resources ReflectCubeFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   if( fd.features[MFT_DiffuseMap] ||
       fd.features[MFT_NormalMap] )
   {
      res.numTex = 1;
      res.numTexReg = 1;
   }
   else
   {
      res.numTex = 2;
      res.numTexReg = 2;
   }

   return res;
}

void ReflectCubeFeatGLSL::setTexData(  Material::StageData &stageDat,
                                       const MaterialFeatureData &stageFeatures,
                                       RenderPassData &passData,
                                       U32 &texIndex )
{
   // set up a gloss map if one is not present in the current pass
   // but is present in the current material stage
   if( !passData.mFeatureData.features[MFT_DiffuseMap] &&
       !passData.mFeatureData.features[MFT_NormalMap] )
   {
      GFXTextureObject *tex = stageDat.getTex( MFT_DetailMap );
      if (  tex &&
            stageFeatures.features[MFT_DiffuseMap] )
         passData.mTexSlot[ texIndex++ ].texObject = tex;
      else
      {
         tex = stageDat.getTex( MFT_NormalMap );

         if (  tex &&
               stageFeatures.features[ MFT_NormalMap ] )
            passData.mTexSlot[ texIndex++ ].texObject = tex;
      }
   }
   
   if( stageDat.getCubemap() )
   {
      passData.mCubeMap = stageDat.getCubemap();
      passData.mTexType[texIndex++] = Material::Cube;
   }
   else
   {
      if( stageFeatures.features[MFT_CubeMap] )
      {
         // assuming here that it is a scenegraph cubemap
         passData.mTexType[texIndex++] = Material::SGCube;
      }
   }

}


//****************************************************************************
// RTLighting
//****************************************************************************

RTLightingFeatGLSL::RTLightingFeatGLSL()
   : mDep( "shaders/common/gl/lighting.glsl" )
{
   addDependency( &mDep );
}

void RTLightingFeatGLSL::processVert(  Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
	MultiLine *meta = new MultiLine;
	
	ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
	
   // Special case for lighting imposters. We dont have a vert normal and may not
   // have a normal map. Generate and pass the normal data the pixel shader needs.
   if ( fd.features[MFT_ImposterVert] )
   {
      if ( !fd.features[MFT_NormalMap] )
      {
         Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
         if ( !eyePos )
         {
            eyePos = new Var( "eyePosWorld", "float3" );
            eyePos->uniform = true;
            eyePos->constSortPos = cspPass;
         }
			
			//TODO: should this be the same as "Find the incoming vertex normal" below?
         Var *inPosition = (Var*)LangElement::find( "position" );
			
         Var *outNormal = connectComp->getElement( RT_TEXCOORD );
         outNormal->setName( "wsNormal" );
         outNormal->setStructName( "OUT" );
         outNormal->setType( "float3" );
         outNormal->mapsToSampler = false;
			
         // Transform the normal to world space.
         meta->addStatement( new GenOp( "   @ = normalize( @ - @.xyz );\r\n", outNormal, eyePos, inPosition ) );
      }
		
      addOutWsPosition( componentList, fd.features[MFT_UseInstancing], meta );
		
      output = meta;
		
      return;
   }
		
   // Find the incoming vertex normal.
	Var *inNormal = (Var*)LangElement::find( "inNormal" );
   if ( !inNormal )
      inNormal = (Var*)LangElement::find( "normal" );
	
   // Skip out on realtime lighting if we don't have a normal
   // or we're doing some sort of baked lighting.
   if (  !inNormal || 
         fd.features[MFT_LightMap] || 
         fd.features[MFT_ToneMap] || 
         fd.features[MFT_VertLit] )
      return;

   // Get the transform to world space.
   Var *objTrans = getObjTrans( componentList, fd.features[MFT_UseInstancing], meta );
	
   // If there isn't a normal map then we need to pass
   // the world space normal to the pixel shader ourselves.
   if ( !fd.features[MFT_NormalMap] )
   {
      Var *outNormal = connectComp->getElement( RT_TEXCOORD );
      outNormal->setName( "outWsNormal" );
      outNormal->setType( "vec3" );
      outNormal->mapsToSampler = false;
   
      // Transform the normal to world space.
		meta->addStatement( new GenOp( "   @ = ( @ * vec4( normalize( @ ), 0.0 ) ).xyz;\r\n", outNormal, objTrans, inNormal ) );
   }

   // Get the input position.
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition )
      inPosition = (Var*)LangElement::find( "position" );

	addOutWsPosition( componentList, fd.features[MFT_UseInstancing], meta );
	
   output = meta;
}

void RTLightingFeatGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // Skip out on realtime lighting if we don't have a normal
   // or we're doing some sort of baked lighting.
   //
   // TODO: We can totally detect for this in the material
   // feature setup... we should move it out of here!
   //
   if ( fd.features[MFT_LightMap] || fd.features[MFT_ToneMap] || fd.features[MFT_VertLit] )
      return;
  
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );

   MultiLine *meta = new MultiLine;

   // Look for a wsNormal or grab it from the connector.
   Var *wsNormal = (Var*)LangElement::find( "wsNormal" );
   if ( !wsNormal )
   {
      Var *outWsNormal = connectComp->getElement( RT_TEXCOORD );
      outWsNormal->setName( "outWsNormal" );
      outWsNormal->setType( "vec3" );

      wsNormal = new Var( "wsNormal", "vec3" );

      // If we loaded the normal its our resposibility
      // to normalize it... the interpolators won't.
      meta->addStatement( new GenOp( "   @ = normalize( @ );\r\n", 
         new DecOp( wsNormal ), outWsNormal ) );
   }

	Var *wsPosition = getInWsPosition( componentList );

   // If we have a specular feature then we need to
   // get the world space view vector to pass to the
   // lighting calculation.
   Var *wsView = new Var( "wsView", "vec3" );
   if ( fd.features[MFT_PixSpecular] )
   {
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var;
         eyePos->setType( "vec3" );
         eyePos->setName( "eyePosWorld" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }

      meta->addStatement( new GenOp( "   @ = normalize( @ - @ );\r\n", 
         new DecOp( wsView ), eyePos, wsPosition ) );
   }
   else
      meta->addStatement( new GenOp( "   @ = vec3( 0 );\r\n", new DecOp( wsView ) ) );

   // Create temporaries to hold results of lighting.
   Var *rtShading = new Var( "rtShading", "vec4" );
   Var *specular = new Var( "specular", "vec4" );
   meta->addStatement( new GenOp( "   @; @;\r\n", 
      new DecOp( rtShading ), new DecOp( specular ) ) );   

   // Calculate the diffuse shading and specular powers.
   meta->addStatement( new GenOp( "   compute4Lights( @, @, @, @, @ );\r\n", 
      wsView, wsPosition, wsNormal, rtShading, specular ) );

   // Look for a light mask generated from a previous
   // feature (this is done for BL terrain lightmaps).
   Var *lightMask = (Var*)LangElement::find( "lightMask" );
   if ( lightMask )
      meta->addStatement( new GenOp( "   @.rgb *= @;\r\n", rtShading, lightMask ) );

   // Apply the lighting to the diffuse color.
   LangElement *lighting = new GenOp( "vec4( @.rgb + ambient.rgb, 1 )", rtShading );
   meta->addStatement( new GenOp( "   @;\r\n", assignColor( lighting, Material::Mul ) ) );
   output = meta;  
}

ShaderFeature::Resources RTLightingFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res;

   // These features disable realtime lighting.
   if (  !fd.features[MFT_LightMap] && 
         !fd.features[MFT_ToneMap] &&
         !fd.features[MFT_VertLit] )
   {
      // If enabled we pass the position.
      res.numTexReg = 1;

      // If there isn't a bump map then we pass the
      // world space normal as well.
      if ( !fd.features[MFT_NormalMap] )
         res.numTexReg++;
   }

   return res;
}


//****************************************************************************
// Fog
//****************************************************************************

FogFeatGLSL::FogFeatGLSL()
   : mFogDep( "shaders/common/gl/torque.glsl" )
{
   addDependency( &mFogDep );
}

void FogFeatGLSL::processVert(   Vector<ShaderComponent*> &componentList, 
										const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
	
   const bool vertexFog = Con::getBoolVariable( "$useVertexFog", false );
   if ( vertexFog || GFX->getPixelShaderVersion() < 3.0 )
   {
      // Grab the eye position.
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var( "eyePosWorld", "vec3" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }
		
      Var *fogData = new Var( "fogData", "vec3" );
      fogData->uniform = true;
      fogData->constSortPos = cspPass;   
		
      Var *wsPosition = new Var( "fogPos", "vec3" );
      getWsPosition( componentList, 
						  fd.features[MFT_UseInstancing], 
						  meta,
						  new DecOp( wsPosition ) );
		
      // We pass the fog amount to the pixel shader.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      Var *fogAmount = connectComp->getElement( RT_TEXCOORD );
      fogAmount->setName( "fogAmount" );
      fogAmount->setType( "float" );
      fogAmount->mapsToSampler = false;
		
      meta->addStatement( new GenOp( "   @ = saturate( computeSceneFog( @, @, @.r, @.g, @.b ) );\r\n", 
												fogAmount, eyePos, wsPosition, fogData, fogData, fogData ) );
   }
   else
   {
      // We fog in world space... make sure the world space
      // position is passed to the pixel shader.  This is
      // often already passed for lighting, so it takes up
      // no extra output registers.
      addOutWsPosition( componentList, fd.features[MFT_UseInstancing], meta );
   }
	
   output = meta;
}

void FogFeatGLSL::processPix( Vector<ShaderComponent*> &componentList, 
									  const MaterialFeatureData &fd )
{
   MultiLine *meta = new MultiLine;
	
   Var *fogColor = new Var;
   fogColor->setType( "vec4" );
   fogColor->setName( "fogColor" );
   fogColor->uniform = true;
   fogColor->constSortPos = cspPass;
	
   // Get the out color.
   Var *color = (Var*) LangElement::find( "col" );
   if ( !color )
   {
      color = new Var;
      color->setType( "vec4" );
      color->setName( "col" );
   }
	
   Var *fogAmount;
	
   const bool vertexFog = Con::getBoolVariable( "$useVertexFog", false );
   if ( vertexFog || GFX->getPixelShaderVersion() < 3.0 )
   {
      // Per-vertex.... just get the fog amount.
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      fogAmount = connectComp->getElement( RT_TEXCOORD );
      fogAmount->setName( "fogAmount" );
      fogAmount->setType( "float" );
   }
   else
   {
      Var *wsPosition = getInWsPosition( componentList );
		
      // grab the eye position
      Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
      if ( !eyePos )
      {
         eyePos = new Var( "eyePosWorld", "vec3" );
         eyePos->uniform = true;
         eyePos->constSortPos = cspPass;
      }
		
      Var *fogData = new Var( "fogData", "vec3" );
      fogData->uniform = true;
      fogData->constSortPos = cspPass;   
		
      /// Get the fog amount.
      fogAmount = new Var( "fogAmount", "float" );
      meta->addStatement( new GenOp( "   @ = saturate( computeSceneFog( @, @, @.r, @.g, @.b ) );\r\n", 
												new DecOp( fogAmount ), eyePos, wsPosition, fogData, fogData, fogData ) );
   }
	
   // Lerp between the fog color and diffuse color.
   LangElement *fogLerp = new GenOp( "mix( @.rgb, @.rgb, @ )", fogColor, color, fogAmount );
   meta->addStatement( new GenOp( "   @.rgb = @;\r\n", color, fogLerp ) );
	
   output = meta;
}

ShaderFeature::Resources FogFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res;
   res.numTexReg = 1;
   return res;
}


//****************************************************************************
// Visibility
//****************************************************************************

void VisibilityFeatGLSL::processVert( Vector<ShaderComponent*> &componentList, 
                                      const MaterialFeatureData &fd )
{  
   // Pass screen space position to pixel shader to compute a full screen buffer uv
   Var* ssPos = ( Var* ) LangElement::find( "screenspacePos" );
   if( !ssPos )
   {      
      ShaderConnector* connectComp = dynamic_cast< ShaderConnector* >( componentList[C_CONNECTOR] );
      AssertFatal( connectComp, "VisibilityFeatGLSL::processVert - invalid ShaderConnector" );
      
      Var* ssPos = connectComp->getElement( RT_TEXCOORD );
      ssPos->setName( "screenspacePos" );
      ssPos->setType( "vec4" );

      Var* outPosition = ( Var* ) LangElement::find( "gl_Position" );
      AssertFatal( outPosition, "VisibilityFeatGLSL::processVert - No gl_Position" );

      output = new GenOp( "   @ = @;\r\n", ssPos, outPosition );
   }
}

void VisibilityFeatGLSL::processPix(   Vector<ShaderComponent*> &componentList, 
                                       const MaterialFeatureData &fd )
{
   // Look up output color.
   
   Var* color = ( Var* ) LangElement::find( "col" );
   if( !color )
   {
      output = NULL;
      return;
   }
      
	Var* visibility = (Var*)LangElement::find( "visibility" );

	// Create visibility var.
	if ( !visibility )
	{
		visibility = new Var();
		visibility->setType( "float" );
		visibility->setName( "visibility" );
		visibility->uniform = true;
		visibility->constSortPos = cspPotentialPrimitive;  
	}
	
   Var* ssPos = ( Var* ) LangElement::find( "screenspacePos" );
   if( !ssPos )
   {
      ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
      AssertFatal( connectComp, "VisibilityFeatGLSL::processPix - invalid ShaderConnector" );
      
      ssPos = connectComp->getElement( RT_TEXCOORD );
      ssPos->setName( "screenspacePos" );
      ssPos->setType( "vec4" );
   }

	MultiLine* meta = new MultiLine;      
	output = meta;
	
	//Compute final visibility from incoming visibility and strength of fade effects
	Var *finalVisibility = new Var( "finalVisibility", "float" );
   meta->addStatement(new GenOp( "   @ = @;\r\n", new DecOp( finalVisibility ), visibility ) );
	Var *imposterFade = (Var*) LangElement::find( "imposterFade" );
   if ( imposterFade )
      meta->addStatement( new GenOp( "   @ *= @;\r\n", finalVisibility, imposterFade ) );
	Var *foliageFade = (Var*) LangElement::find( "foliageFade" );
   if ( foliageFade )
      meta->addStatement( new GenOp( "   @ *= @;\r\n", finalVisibility, foliageFade ) );
	
   if( !fd.features[ MFT_IsTranslucent ] )
   {
      // Create fizzMap sampler.
      
      Var* fizzMap = ( Var* ) LangElement::find( "fizzMap" );
      if( !fizzMap )
      {
         fizzMap = new Var;
         fizzMap->setType( "sampler2D" );
         fizzMap->setName( "fizzMap" );
         fizzMap->uniform = true;
         fizzMap->sampler = true;
         fizzMap->constNum = Var::getTexUnitNum();
      }

      // Create fizzScale uniform.
      
      Var* fizzScale = new Var;
      fizzScale->setType( "float2" );
      fizzScale->setName( "fizzScale" );
      fizzScale->uniform = true;
      fizzScale->constSortPos = cspPass; 

      meta->addStatement( new GenOp( "   float fizz = tex2D( @, (@.xy / @.w) * fizzScale ).r + 0.15;\r\n", fizzMap, ssPos, ssPos ) );      
      meta->addStatement( new GenOp( "   if( ( fizz * @ - 0.329 ) < 0.0 )\r\n"
                                     "      discard\r\n;", finalVisibility ) );
	}
   else if( color )
   {
      meta->addStatement( new GenOp( "   @.w *= @;\r\n", color, finalVisibility ) );
   }      
}

ShaderFeature::Resources VisibilityFeatGLSL::getResources( const MaterialFeatureData &fd )
{
   Resources res; 

   if ( !fd.features[ MFT_IsTranslucent ] )
   {   
      res.numTex = 1;
      res.numTexReg = 1;
   }

   return res;
}

void VisibilityFeatGLSL::setTexData( Material::StageData &stageDat,
                                     const MaterialFeatureData &fd,
                                     RenderPassData &passData,
                                     U32 &texIndex )
{
   if ( !fd.features[ MFT_IsTranslucent ] )
   {   
      GFXTexHandle texHandle( "core/art/fizz_noise.dds", &GFXDefaultStaticDiffuseProfile, "VisibilityFeatHLSL_fizz_noise" );
      stageDat.setTex( MFT_Visibility, texHandle );

      GFXTextureObject *tex = stageDat.getTex( MFT_Visibility );
      if ( tex )
         passData.mTexSlot[ texIndex++ ].texObject = tex;
   }
}


//****************************************************************************
// AlphaTest
//****************************************************************************

void AlphaTestGLSL::processPix(  Vector<ShaderComponent*> &componentList,
                                 const MaterialFeatureData &fd )
{
   // If we're below SM3 and don't have a depth output
   // feature then don't waste an instruction here.
   if ( GFX->getPixelShaderVersion() < 3.0 &&
        !fd.features[ MFT_EyeSpaceDepthOut ]  &&
        !fd.features[ MFT_DepthOut ] )
   {
      output = NULL;
      return;
   }

   // If we don't have a color var then we cannot do an alpha test.
   Var *color = (Var*)LangElement::find( "col" );
   if ( !color )
   {
      output = NULL;
      return;
   }

   // Now grab the alpha test value.
   Var *alphaTestVal  = new Var;
   alphaTestVal->setType( "float" );
   alphaTestVal->setName( "alphaTestValue" );
   alphaTestVal->uniform = true;
   alphaTestVal->constSortPos = cspPotentialPrimitive;

   // Do the clip.
   output = new GenOp( "   if ( ( @.a - @ ) < 0 ) discard;\r\n", color, alphaTestVal );
}


//****************************************************************************
// GlowMask
//****************************************************************************

void GlowMaskGLSL::processPix(   Vector<ShaderComponent*> &componentList,
                                 const MaterialFeatureData &fd )
{
   output = NULL;

   // Get the output color... and make it black to mask out 
   // glow passes rendered before us.
   //
   // The shader compiler will optimize out all the other
   // code above that doesn't contribute to the alpha mask.
   Var *color = (Var*)LangElement::find( "col" );
   if ( color )
      output = new GenOp( "   @.rgb = 0;\r\n", color );
}

//****************************************************************************
// RenderTargetZero
//****************************************************************************

void RenderTargetZeroGLSL::processPix( Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd )
{
   // Do not actually assign zero, but instead a number so close to zero it may as well be zero.
   // This will prevent a divide by zero causing an FP special on float render targets
   output = new GenOp( "   @;\r\n", assignColor( new GenOp( "0.00001" ), Material::None, NULL, mOutputTargetMask ) );
}


//****************************************************************************
// HDR Output
//****************************************************************************

HDROutGLSL::HDROutGLSL()
: mTorqueDep( "shaders/common/gl/torque.glsl" )
{
   addDependency( &mTorqueDep );
}
void HDROutGLSL::processPix(  Vector<ShaderComponent*> &componentList,
									 const MaterialFeatureData &fd )
{
   // Let the helper function do the work.
   Var *color = (Var*)LangElement::find( "col" );
   if ( color )
      output = new GenOp( "   @ = hdrEncode( @ );\r\n", color, color );
}


//****************************************************************************
// FoliageFeatureGLSL
//****************************************************************************

#include "T3D/fx/groundCover.h"

FoliageFeatureGLSL::FoliageFeatureGLSL()
: mDep( "shaders/common/gl/foliage.glsl" )
{
   addDependency( &mDep );
}

void FoliageFeatureGLSL::processVert( Vector<ShaderComponent*> &componentList, 
												 const MaterialFeatureData &fd )
{ 
	
   MultiLine *meta = new MultiLine;
	
   // Get the input variables we need.
	
   Var *inPosition = (Var*)LangElement::find( "inPosition" );
   if ( !inPosition ) {
		//   inPosition = (Var*)LangElement::find( "position" );
		inPosition = new Var( "inPosition", "vec3" );
		meta->addStatement( new GenOp( "   @ = @;\n", new DecOp( inPosition ), (Var*)LangElement::find( "position" ) ) );
	}
	
   Var *inColor = (Var*)LangElement::find( "diffuse" );   
   Var *outColor = new Var( "inDiffuse", "vec4" );
	meta->addStatement( new GenOp( "   @ = @;\n", new DecOp( outColor ), inColor ) );
	
	Var *inParams = (Var*)LangElement::find( "texCoord" );   
	Var *outParams = getOutTexCoord(   "texCoord", 
											  "vec2", 
											  true, 
											  fd.features[MFT_TexAnim], 
											  meta, 
											  componentList );
	
	
	
   // Declare the normal and tangent variables since they do not exist
   // in this vert type, but we do need to set them up for others.
	
	Var *inNormal = (Var*)LangElement::find( "inNormal" );
   if ( !inNormal ) {
		inNormal = new Var( "inNormal", "vec3" );
		meta->addStatement( new GenOp( "   @ = @;\n", new DecOp( inNormal ), (Var*)LangElement::find( "normal" ) ) );
	}
   //Var *normal = (Var*)LangElement::find( "normal" );   
   AssertFatal( inNormal, "FoliageFeatureHLSL requires vert normal!" );   
	
   Var *tangent = new Var;
   tangent->setType( "vec3" );
   tangent->setName( "T" );
   LangElement *tangentDec = new DecOp( tangent );
   meta->addStatement( new GenOp( "   @;\n", tangentDec ) );         
	
	// We add a float foliageFade to the OUT structure.
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *fade = connectComp->getElement( RT_TEXCOORD );
   fade->setName( "foliageFade" );
   fade->setType( "float" );
	
   // grab the eye position
   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if ( !eyePos )
   {
      eyePos = new Var( "eyePosWorld", "vec3" );
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }
	
	
   // All actual work is offloaded to this method.
   meta->addStatement( new GenOp( "   foliageProcessVert( @, @, @, @, @, @, @ );\r\n", inPosition, outColor, inParams, outParams, inNormal, tangent, eyePos ) );
	
	// Assign to foliageFade. InColor.a was set to the correct value inside foliageProcessVert.
   meta->addStatement( new GenOp( "   @ = @.a;\r\n", fade, outColor ) );
	
   output = meta;
}

void FoliageFeatureGLSL::processPix( Vector<ShaderComponent*> &componentList, 
												const MaterialFeatureData &fd )
{
   // Find / create IN.foliageFade
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *fade = connectComp->getElement( RT_TEXCOORD );
   fade->setName( "foliageFade" );
   fade->setType( "float" );
}

void FoliageFeatureGLSL::determineFeature( Material *material, const GFXVertexFormat *vertexFormat, U32 stageNum, const FeatureType &type, const FeatureSet &features, MaterialFeatureData *outFeatureData )
{      
	// This isn't really necessary since the outFeatureData will be filtered after
   // this call.
   if ( features.hasFeature( MFT_Foliage  ) )
		outFeatureData->features.addFeature( type );
}

ShaderFeatureConstHandles* FoliageFeatureGLSL::createConstHandles( GFXShader *shader, SimObject *userObject )
{
   GroundCover *gcover = dynamic_cast< GroundCover* >( userObject );
   AssertFatal( gcover != NULL, "FoliageFeatureGLSL::createConstHandles - userObject was not valid!" );
	
   GroundCoverShaderConstHandles *handles = new GroundCoverShaderConstHandles();
   handles->mGroundCover = gcover;
	
   handles->init( shader );
   
   return handles;
}


//****************************************************************************
// ParticleNormal
//****************************************************************************

void ParticleNormalFeatureGLSL::processVert(Vector<ShaderComponent*> &componentList, const MaterialFeatureData &fd)
{
   MultiLine *meta = new MultiLine;
   output = meta;
	
   // Calculate normal and tangent values since we want to keep particle verts
   // as light-weight as possible
	
   Var *normal = (Var*) LangElement::find("normal");
   if(normal == NULL)
   {
      normal = new Var;
      normal->setType( "vec3" );
      normal->setName( "normal" );
		
      // These values are not accidental. It is slightly adjusted from facing straight into the
      // screen because there is a discontinuity at (0, 1, 0) for gbuffer encoding. Do not
      // cause this value to be (0, -1, 0) or interlaced normals will be discontinuous.
      // [11/23/2009 Pat]
      meta->addStatement(new GenOp("   @ = vec3(0.0, -0.97, 0.14);\r\n", new DecOp(normal)));
   }
	
   Var *T = (Var*) LangElement::find( "T" );
   if(T == NULL)
   {
      T = new Var;
      T->setType( "vec3" );
      T->setName( "T" );
      meta->addStatement(new GenOp("   @ = vec3(0.0, 0.0, -1.0);\r\n", new DecOp(T)));
   }
}


//****************************************************************************
// ImposterVertFeatureGLSL
//****************************************************************************

ImposterVertFeatureGLSL::ImposterVertFeatureGLSL()
:  mDep( "shaders/common/gl/imposter.glsl" )
{
   addDependency( &mDep );
}

void ImposterVertFeatureGLSL::processVert(   Vector<ShaderComponent*> &componentList, 
														const MaterialFeatureData &fd )
{      
   MultiLine *meta = new MultiLine;
   output = meta;
	
   // Get the input vertex variables.   
   Var *inPosition = (Var*)LangElement::find( "position" );
   Var *inMiscParams = (Var*)LangElement::find( "tcImposterParams" );   
   Var *inUpVec = (Var*)LangElement::find( "tcImposterUpVec" );   
   Var *inRightVec = (Var*)LangElement::find( "tcImposterRightVec" );   
	
   // Get the input shader constants.
   Var *imposterLimits  = new Var;
   imposterLimits->setType( "vec4" );
   imposterLimits->setName( "imposterLimits" );
   imposterLimits->uniform = true;
   imposterLimits->constSortPos = cspPotentialPrimitive;
	
   Var *imposterUVs  = new Var;
   imposterUVs->setType( "vec4" );
   imposterUVs->setName( "imposterUVs" );
   imposterUVs->arraySize = 64; // See imposter.hlsl
   imposterUVs->uniform = true;
   imposterUVs->constSortPos = cspPotentialPrimitive;
	
   Var *eyePos = (Var*)LangElement::find( "eyePosWorld" );
   if ( !eyePos )
   {
      eyePos = new Var( "eyePosWorld", "vec3" );
      eyePos->uniform = true;
      eyePos->constSortPos = cspPass;
   }
	
   // Declare the outputs from this feature.
   Var *outInPosition = new Var;
   outInPosition->setType( "vec3" );
   outInPosition->setName( "inPosition" );
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( outInPosition ) ) );         
	
   Var *outTexCoord = new Var;
   outTexCoord->setType( "vec2" );
   outTexCoord->setName( "texCoord" );
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( outTexCoord ) ) );         
	
   Var *outWorldToTangent = new Var;
   outWorldToTangent->setType( "mat3" );
   outWorldToTangent->setName( "worldToTangent" );
   meta->addStatement( new GenOp( "   @;\r\n", new DecOp( outWorldToTangent ) ) );
	//Var *outWorldToTangent = getOutWorldToTangent( componentList, meta, fd );
	
   // Add imposterFade to the OUT structure.
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *outFade = connectComp->getElement( RT_TEXCOORD );
   outFade->setName( "imposterFade" );
   outFade->setType( "float" ); 
	
   // Assign OUT.imposterFade
   meta->addStatement( new GenOp( "   @ = @.y;\r\n", outFade, inMiscParams ) );
	
   // All actual work is done in this method.
   meta->addStatement( new GenOp( "   imposter_v( @.xyz, int(@.w), @.x * length(@), normalize(@), normalize(@), int(@.y), int(@.x), @.z, bool(@.w), @, @, @, @, @ );\r\n",
											
											inPosition,
											inPosition,
											
											inMiscParams,
                                 inRightVec,
											
											inUpVec,
                                 inRightVec,
											
											imposterLimits,
											imposterLimits,
											imposterLimits,
											imposterLimits,
											
											eyePos,
											imposterUVs,
											
											outInPosition, 
											outTexCoord,
											outWorldToTangent ) );
	
   // Copy the position to wsPosition for use in shaders 
   // down stream instead of looking for objTrans.
   Var *wsPosition = new Var;
   wsPosition->setType( "vec3" );
   wsPosition->setName( "wsPosition" );
   meta->addStatement( new GenOp( "   @ = @.xyz;\r\n", new DecOp( wsPosition ), outInPosition ) ); 
	
   // If we new viewToTangent... its the same as the
   // world to tangent for an imposter.
   Var *viewToTangent = new Var;
   viewToTangent->setType( "mat3" );
   viewToTangent->setName( "viewToTangent" );
   meta->addStatement( new GenOp( "   @ = @;\r\n", new DecOp( viewToTangent ), outWorldToTangent ) );       
}

void ImposterVertFeatureGLSL::processPix( Vector<ShaderComponent*> &componentList,
													  const MaterialFeatureData &fd )
{
	
   // Find / create IN.imposterFade
   ShaderConnector *connectComp = dynamic_cast<ShaderConnector *>( componentList[C_CONNECTOR] );
   Var *fade = connectComp->getElement( RT_TEXCOORD );
   fade->setName( "imposterFade" );
   fade->setType( "float" );
	
	/*
	 // Find / create visibility
	 Var *visibility = (Var*) LangElement::find( "visibility" );
	 if ( !visibility )
	 {
	 visibility = new Var();
	 visibility->setType( "float" );
	 visibility->setName( "visibility" );
	 visibility->uniform = true;
	 visibility->constSortPos = cspPotentialPrimitive;  
	 }      
	 
	 MultiLine *meta = new MultiLine;
	 
	 // Multiply foliageFade into visibility.
	 //meta->addStatement( new GenOp( "   @ *= @;\r\n", visibility, fade ) );
	 
	 output = meta;
	 */
}

void ImposterVertFeatureGLSL::determineFeature( Material *material, 
															  const GFXVertexFormat *vertexFormat, 
															  U32 stageNum, 
															  const FeatureType &type, 
															  const FeatureSet &features, 
															  MaterialFeatureData *outFeatureData )
{      
   if ( features.hasFeature( MFT_ImposterVert ) )
      outFeatureData->features.addFeature( MFT_ImposterVert );
}