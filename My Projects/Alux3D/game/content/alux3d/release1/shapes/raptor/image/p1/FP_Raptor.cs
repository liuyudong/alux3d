// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

singleton TSShapeConstructor(FP_RaptorDAE)
{
   baseShape = "./FP_Raptor.DAE";
   lodType = "TrailingNumber";
   loadLights = "0";
};

function FP_RaptorDAE::onLoad(%this)
{
   %this.renameSequence("ambient", "timeline");
   %this.setSequenceCyclic("timeline", "0");
   %this.addSequence("timeline", "root", "120", "121", "1", "0");
   %this.addSequence("timeline", "idle_fidget1", "210", "390", "1", "0");
   %this.addSequence("timeline", "reload", "460", "550", "1", "0");
   %this.addSequence("timeline", "switch_out", "660", "670", "1", "0");
   %this.addSequence("timeline", "switch_in", "680", "690", "1", "0");
   %this.addSequence("timeline", "fire_alt", "580", "625", "1", "0");
   %this.addSequence("timeline", "fire", "420", "428", "1", "0");
   %this.addSequence("timeline", "Run", "10", "39", "1", "0");
   %this.addSequence("timeline", "sprint", "70", "89", "1", "0");
   %this.addSequence("timeline", "idle", "120", "179", "1", "0");
   %this.setSequenceCyclic("root", "1");
   %this.setSequenceCyclic("Run", "1");
   %this.setSequenceCyclic("sprint", "1");
   %this.setSequenceCyclic("idle", "1");
}
