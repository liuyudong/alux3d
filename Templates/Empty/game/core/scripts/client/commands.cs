// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

// Sync the Camera and the EditorGui
function clientCmdSyncEditorGui()
{
   if (isObject(EditorGui))
      EditorGui.syncCameraGui();
}