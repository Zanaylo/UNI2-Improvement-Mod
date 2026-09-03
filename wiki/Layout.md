# Layout

```
src/Core/       DLL entry, dinput8 forwarding, settings, logging, crash dumps
src/Hooks/      Signature scanning, MinHook wrappers, RTTI vtable lookup, input hooks
src/D3D9/       Direct3DCreate9 and device vtable hooks, present tuning
src/Overlay/    ImGui setup, window registry, individual windows
src/Game/       Game memory layer, structures and resolved pointers
src/Training/   Frame stepper, frame meter, player control, input lag meter
src/Palette/    Palette identity, painting and sharing
```
