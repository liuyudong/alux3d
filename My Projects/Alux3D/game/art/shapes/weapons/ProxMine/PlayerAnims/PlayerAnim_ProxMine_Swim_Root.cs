// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

singleton TSShapeConstructor(PlayerAnim_ProxMine_Swim_RootDAE)
{
   baseShape = "./PlayerAnim_ProxMine_Swim_Root.dae";
   neverImport = "EnvironmentAmbientLight";
   loadLights = "0";
};

function PlayerAnim_ProxMine_Swim_RootDAE::onLoad(%this)
{
   %this.setSequenceCyclic("ambient", "true");
   %this.addSequence("ambient", "Swim_Root", "1540", "1599");
}
