# Building

You need **Visual Studio 2026**. The project uses platform toolset **v145**. A 2022 install does not
have it and fails with `MSB8020`. Everything else is included under `depends/` (Dear ImGui, MinHook).
You do not need the DirectX SDK, because `d3d9.h` ships with the Windows SDK.

```
MSBuild UNI2_IM.slnx /p:Configuration=Release /p:Platform=Win32
```

`UNI2_IM.sln` is kept for older tools. Both build the same project.

The output is `bin\Release\dinput8.dll`. **Win32 only**, because the game is 32-bit. Add
`/p:EnableLogging=true` to get a runtime log next to the DLL. The game locks the DLL while it runs, so
close the game before you build over an installed copy.
