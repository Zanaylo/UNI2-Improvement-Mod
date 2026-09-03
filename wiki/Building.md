# Building

Needs **Visual Studio 2026** - the project targets platform toolset **v145**, which the 2022
install does not have and fails on with `MSB8020`. Everything else is vendored under `depends/`
(Dear ImGui, MinHook). The DirectX SDK is not needed; `d3d9.h` ships with the Windows SDK.

```
MSBuild UNI2_IM.slnx /p:Configuration=Release /p:Platform=Win32
```

`UNI2_IM.sln` is kept beside it for older tooling; both build the same project.

Output is `bin\Release\dinput8.dll`. **Win32 only** - the game is 32-bit. Add
`/p:EnableLogging=true` for a runtime log next to the DLL. The game locks the DLL while it runs, so
close it before building over an installed copy.
