# CS:S v34 Bot Behaviors Metamod Plugin
This plugin showcases ways to improve the bots in Counter-Strike: Source by rewriting some of their internal logic and by bringing new features into editing the navigation meshes. The plugin has been tailored for a Windows build of CS:S v34 (build 4044), but the ideas demonstrated should be applicable to all versions of the game. The goal of the plugin has been to make the bots less predictable and behave more like humans in certain aspects.

**NOTE:** The plugin requires a custom signature scanning library and a customized version of AlliedModder's [CDetour](https://github.com/alliedmodders/sourcemod/tree/master/public/CDetour) detouring library, which are currently not publicly available. **THIS MEANS THE PLUGIN CANNOT BE BUILT OUT OF THE BOX!** As of now, the plugin is only meant to showcase the ideas behind it.
If you do however wish to replace the missing dependencies with your own implementations and build the plugin, the repository contains a solution file for Visual Studio 2010 that is more or less configured for the job. The plugin was developed using version 1.8.7 of Metamod and the [episode1](https://github.com/alliedmodders/hl2sdk/tree/episode1) branch of the HL2 SDK.

## Features
The bots' behaviors have been directly altered in multiple ways:
- Bots' paths are more random and bot-dependent. Instead of the whole team computing their paths based on the same map information, each bot gets to pick their own route to travel through the map. This feature comes with tools that allow the user to pinpoint all the different routes that bots can take to travel around the map.
- Bots consider their money as a team and will often eco-rush together if they are low on money or equipment.
- Bot aiming logic has been improved to properly account in weapon recoil. Recoil control amount is dependent on the bot's skill level.
- Bots will only try to open doors that are fully closed.
- Team flashes for bots are disabled and their flashed time has been reduced.

 In addition to the modifications to the bots, new features for editing the navigation mesh have also been added:
 - Users can create and remove hiding spots. When creating a hiding spot, the player's viewing direction is saved and tied to the spot. Bots using the hiding spot will use the saved view angles by default. The navigation mesh is now also easy to clear of all hiding spots with the command nav_remove_all_hiding_spots. The user-added hiding spots and their viewing directions are both saved to the nav file for convenience.
 - A nav identifier can now be tied to the nav file by supplying it as an argument to the nav_save command. The command nav_identify can be used to print the identifier, which can be useful when you have multiple nav files for a single map.
 - When saving the nav, nav data that is generated with auto-analysis but lost with destructive edits like nav_split is now recomputed before saving the navigation mesh. This data includes spot encounters and approach areas, but earliest occupy times are also recalculated. The console variable nav_quicksave 1 can be used to skip these heavy computations to quickly save the nav mesh while editing.

More features are planned to be added at a later time.
