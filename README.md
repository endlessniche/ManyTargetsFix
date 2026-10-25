# Many Targets Fix

This mod fixes the notorious ghost bullet bug and Instinct issues on contracts with a lot of targets.

- Bullets now properly register and no longer ghost through NPCs or walls
- Instinct now outlines every single target on the map instead of being capped at ~60 targets
- Missions with many targets run noticeably faster (e.g. kill everyone contracts)

Note that on IOI's official servers, contracts with hundreds of targets can still crash when the contract ends. That crash happens in the game itself, and this mod neither causes nor fixes it.

**Requires ZHMModSDK.** Last tested on version 3.280.

Also on [Nexus Mods](https://www.nexusmods.com/hitman3/mods/1468).

## Installation

1. Install [ZHMModSDK](https://github.com/OrfeasZ/ZHMModSDK).
2. Put `ManyTargetsFix.dll` in `HITMAN 3\Retail\mods`.
3. Start the game and enable Many Targets Fix in the SDK's mod list.

## Updates and Compatibility

After a game update, any fix that no longer works turns itself off and shows as not active in the window instead of crashing your game.

Compatible with:

- Simple Mod Framework mods, including Instinct mods like Pure Instinct.
- Other ZHMModSDK mods, unless they also change PhysX's ray batches or the code that draws Instinct.

## Credits

- Orfeas Zafeiris for ZHMModSDK
- Tsuda Kageyu for MinHook

## License

GPL-3.0
