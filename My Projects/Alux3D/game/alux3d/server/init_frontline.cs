// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

function onServerCreated()
{
   // Server::GameType is sent to the master server.
   // This variable should uniquely identify your game and/or mod.
   $Server::GameType = $appName;

   // Server::MissionType sent to the master server.  Clients can
   // filter servers based on mission type.
   $Server::MissionType = "Frontline";

   // GameStartTime is the sim time the game started. Used to calculated
   // game elapsed time.
   $Game::StartTime = 0;

   // Create the server physics world.
   physicsInitWorld( "server" );
   
   // Load up any core datablocks
   exec("core/art/datablocks/datablockExec.cs");
   
   // Load up any objects or datablocks saved to the editor managed scripts
   %datablockFiles = new ArrayObject();
   %datablockFiles.add( "art/particles/managedParticleData.cs" );
   %datablockFiles.add( "art/particles/managedParticleEmitterData.cs" );
   %datablockFiles.add( "art/decals/managedDecalData.cs" );
   %datablockFiles.add( "art/datablocks/managedDatablocks.cs" );
   %datablockFiles.add( "art/forest/managedItemData.cs" );
   %datablockFiles.add( "art/datablocks/datablockExec.cs" );   
   loadDatablockFiles( %datablockFiles, true );
   
   exec("./base/commands.cs");
   exec("./base/misc.cs");
   exec("./base/bleedfx.cs");
   exec("./base/etherform.cs");
   exec("./forms/light/exec.cs");
   exec("./weapons/badger/v1/exec.cs");
   exec("./weapons/raptor/v1/exec.cs");
   exec("./weapons/bulldog/v1/exec.cs");

   exec("./dm/exec.cs");
   exec("./frontline/exec.cs");

   // Keep track of when the game started
   $Game::StartTime = $Sim::Time;
}

function onServerDestroyed()
{
   // This function is called as part of a server shutdown.

   physicsDestroyWorld( "server" );

   // Clean up the GameCore package here as it persists over the
   // life of the server.
   if (isPackage(GameCore))
   {
      deactivatePackage(GameCore);
   }
}