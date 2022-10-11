# What is World Locking Tools
World Locking Tools provides a stable and reliable world-locked coordinate system, binding the virtual/holographic world to the physical world.

World Locking Tools locks the holographic space of your application to the physical world. A hologram put in position relative to physical world features will stay fixed relative to those features, and remain fixed relative to other holograms.

World Locking Tools also includes Space Pins, which reposition the scene to match real-world features like QRCodes.


# Using the Plugin
Add the plugin to your project from either GitHub or the Marketplace and enable it in the Plugins window.  Call the "StartWorldLockingTools" function to start using world locking tools with the given parameters.

This plugin was written for HoloLens, and is meant to be used with the [Microsoft OpenXR](https://github.com/microsoft/Microsoft-OpenXR-Unreal) plugin.  The underlying FrozenWorld binaries that world locking tools depends on are available for Android and iOS, and this plugin uses ARBlueprintLibrary when using core AR features like ARPins, so this can be modified to build for ARKit and ARCore as well.


## From GitHub source
Copy the WLT_Project/Plugins/WorldLockingTools directory into your game's Plugins directory.  Reopen your project and enable the World Locking Tools plugin in your Plugins window.


## World Locking Tools Pawn
World Locking Tools works by repositioning the users' head to match a pose the underlying frozen world system is generating from nearby anchors that have been created while walking around.  To do this, the camera on the pawn must have a parent and grandparent component that are children of the DefaultSceneRoot.  The sample's WLTPawn is setup this way, and one is also included in the plugin under Source/WorldLockingTools/Public/WorldLockingToolsPawn.h.


## Space Pins
Space Pins have a virtual position in your scene and need to be given a physical position in your world to reposition the scene to match the real-world.  This is accomplished by calling SetSpongyPose on a SpacePin with a transform in the device's tracking space.  When multiple space pins are locked in a scene, the relative real-world offsets between them should closely match how they are setup in the virtual scene to minimize drift as you walk from one space pin to another.


## Try the example project
The sample project has two levels which demonstrate starting world locking tools, and using space pins with either hand manipulation in one level or QRCodes in the other level.  Both levels have an 8 meter long hologram with a space pin on either side.  Build, deploy, and run the sample on a HoloLens and take note of the direction the hologram is facing.  Close the application, rotate your body to face a different direction, then relaunch the application.

Without using world locking tools, the scene will face the new direction you are facing.  With world locking tools, the scene will be oriented based on the first saved launch.

Test the space pins by grabbing the cube on one end of the 8 meter hologram.  It will turn pink when it can be interacted with.  Face the direction of the white arrow off of this cube and move your hand around while gripped to move the space pin around.  The entire scene will move around as the space pin moves.  Walk to the other space pin, face the same direction as its white arrow, grip and move this space pin around too.  Once both space pins have been positioned, walk between the two and observe that the scene will shift into place to match the space pin's locked real-world position as you get closer to it.

The second scene does the same thing, but positions the space pins based on the QRCodes in the Images directory.


# See Also
[World Locking Tools](https://learn.microsoft.com/en-us/mixed-reality/world-locking-tools/)

[Space Pins](https://learn.microsoft.com/en-us/mixed-reality/world-locking-tools/documentation/concepts/advanced/spacepins)


## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.


## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft 
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
