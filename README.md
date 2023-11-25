# EfpseCullFix
Mod to fix 3d model culling in Easy FPS Engine.

<br>

### Installation:
Place hid.dll next to Game.exe in your game's directory.

### Uninstallation:
Delete hid.dll from your game's directory.

<br>

### How it works:
EFPSE culls 3d objects when you can't see the tile it was placed on.  
This is fine for small objects, but can cause issues with large models.  
This mod works around that by moving every 3d model to be directly in front of the player.  
Then it applies an offset to the 3d model, to make it visually appear to be located in its original position.  

This isn't the best possible solution, but it's potentially better than no fix at all.  
At the moment, it will probably result in performance loss, and could make your game more unstable.  
A better fix would be to tweak the culling code directly, but efpse is closed-source, so it will take a lot of work to modify.

<br>

### What's hid.dll?
This mod makes use of a common exploitation technique called 'dll proxying'.  
Most programs will load many .dll files on startup, for accessing libraries like opengl, directinput, etc..  
When you open Game.exe, it scans the folder it's in for those dll files.  
If it can't find a dll in the current directory, it checks System32 instead (where they're usually found).  
We can exploit this by putting any dll we want in the game's folder, and naming it one of the files the engine searches for.  

This mod uses hid.dll, which is one of the libraries EFPSE loads on launch.  
Because the game still needs to access the *real* hid.dll, we also have to pass any requests to the real hid.dll.  
So we're effectively acting as a middleman between Game.exe and the real hid.dll.  
To make use of this, we can start a new thread and run any code we want, with full access to Game.exe's memory.  
While dll hijacking isn't required to perform this modification, it's one of the most user-friendly methods.

<br>

Proxy made with: [https://github.com/maluramichael/dll-proxy-generator](https://github.com/maluramichael/dll-proxy-generator)  

Props to pixelwolf for testing the mod during development.
