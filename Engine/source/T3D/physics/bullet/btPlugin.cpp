// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "T3D/physics/bullet/btPlugin.h"

#include "T3D/physics/physicsShape.h"
#include "T3D/physics/bullet/btWorld.h"
#include "T3D/physics/bullet/btBody.h"
#include "T3D/physics/bullet/btPlayer.h"
#include "T3D/physics/bullet/btCollision.h"
#include "T3D/gameBase/gameProcess.h"
#include "core/util/tNamedFactory.h"


AFTER_MODULE_INIT( Sim )
{
   NamedFactory<PhysicsPlugin>::add( "Bullet", &BtPlugin::create );

   #if defined(TORQUE_OS_MAC)
      NamedFactory<PhysicsPlugin>::add( "default", &BtPlugin::create );
   #endif   
}


PhysicsPlugin* BtPlugin::create()
{
   return new BtPlugin();
}

BtPlugin::BtPlugin()
{
}

BtPlugin::~BtPlugin()
{
}

void BtPlugin::destroyPlugin()
{
   // Cleanup any worlds that are still kicking.
   Map<StringNoCase, PhysicsWorld*>::Iterator iter = mPhysicsWorldLookup.begin();
   for ( ; iter != mPhysicsWorldLookup.end(); iter++ )
   {
      iter->value->destroyWorld();
      delete iter->value;
   }
   mPhysicsWorldLookup.clear();

   delete this;
}

void BtPlugin::reset()
{
   // First delete all the cleanup objects.
   if ( getPhysicsCleanup() )
      getPhysicsCleanup()->deleteAllObjects();

   getPhysicsResetSignal().trigger( PhysicsResetEvent_Restore );

   // Now let each world reset itself.
   Map<StringNoCase, PhysicsWorld*>::Iterator iter = mPhysicsWorldLookup.begin();
   for ( ; iter != mPhysicsWorldLookup.end(); iter++ )
      iter->value->reset();
}

PhysicsCollision* BtPlugin::createCollision()
{
   return new BtCollision();
}

PhysicsBody* BtPlugin::createBody()
{
   return new BtBody();
}

PhysicsPlayer* BtPlugin::createPlayer()
{
   return new BtPlayer();
}

bool BtPlugin::isSimulationEnabled() const
{
   bool ret = false;
   BtWorld *world = static_cast<BtWorld*>( getWorld( smClientWorldName ) );
   if ( world )
   {
      ret = world->getEnabled();
      return ret;
   }

   world = static_cast<BtWorld*>( getWorld( smServerWorldName ) );
   if ( world )
   {
      ret = world->getEnabled();
      return ret;
   }

   return ret;
}

void BtPlugin::enableSimulation( const String &worldName, bool enable )
{
   BtWorld *world = static_cast<BtWorld*>( getWorld( worldName ) );
   if ( world )
      world->setEnabled( enable );
}

void BtPlugin::setTimeScale( const F32 timeScale )
{
   // Grab both the client and
   // server worlds and set their time
   // scales to the passed value.
   BtWorld *world = static_cast<BtWorld*>( getWorld( smClientWorldName ) );
   if ( world )
      world->setEditorTimeScale( timeScale );

   world = static_cast<BtWorld*>( getWorld( smServerWorldName ) );
   if ( world )
      world->setEditorTimeScale( timeScale );
}

const F32 BtPlugin::getTimeScale() const
{
   // Grab both the client and
   // server worlds and call 
   // setEnabled( true ) on them.
   BtWorld *world = static_cast<BtWorld*>( getWorld( smClientWorldName ) );
   if ( !world )
   {
      world = static_cast<BtWorld*>( getWorld( smServerWorldName ) );
      if ( !world )
         return 0.0f;
   }
   
   return world->getEditorTimeScale();
}

bool BtPlugin::createWorld( const String &worldName )
{
   Map<StringNoCase, PhysicsWorld*>::Iterator iter = mPhysicsWorldLookup.find( worldName );
   PhysicsWorld *world = NULL;
   
   iter != mPhysicsWorldLookup.end() ? world = (*iter).value : world = NULL; 

   if ( world ) 
   {
      Con::errorf( "BtPlugin::createWorld - %s world already exists!", worldName.c_str() );
      return false;
   }

   world = new BtWorld();
   
   if ( worldName.equal( smClientWorldName, String::NoCase ) )
      world->initWorld( false, ClientProcessList::get() );
   else
      world->initWorld( true, ServerProcessList::get() );

   mPhysicsWorldLookup.insert( worldName, world );

   return world != NULL;
}

void BtPlugin::destroyWorld( const String &worldName )
{
   Map<StringNoCase, PhysicsWorld*>::Iterator iter = mPhysicsWorldLookup.find( worldName );
   if ( iter == mPhysicsWorldLookup.end() )
      return;

   PhysicsWorld *world = (*iter).value;
   world->destroyWorld();
   delete world;
   
   mPhysicsWorldLookup.erase( iter );
}

PhysicsWorld* BtPlugin::getWorld( const String &worldName ) const
{
   if ( mPhysicsWorldLookup.isEmpty() )
      return NULL;

   Map<StringNoCase, PhysicsWorld*>::ConstIterator iter = mPhysicsWorldLookup.find( worldName );

   return iter != mPhysicsWorldLookup.end() ? (*iter).value : NULL;
}

PhysicsWorld* BtPlugin::getWorld() const
{
   if ( mPhysicsWorldLookup.size() == 0 )
      return NULL;

   Map<StringNoCase, PhysicsWorld*>::ConstIterator iter = mPhysicsWorldLookup.begin();
   return iter->value;
}

U32 BtPlugin::getWorldCount() const
{ 
   return mPhysicsWorldLookup.size(); 
}