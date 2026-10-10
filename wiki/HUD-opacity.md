# HUD opacity

Makes the menus, the HUD and the Training displays see-through during a fight, each on its own, so
you can watch the fight behind them.

Open **Option**, **Display**, **Improvement Mod - HUD Opacity**. Every row goes from 100% (the game
as it is) to 0% (gone), in steps of 10. Press **Confirm** to keep the change.

In Training they are also on the **Improvement Mod** page of the Training menu: select **HUD Opacity**
and press **Confirm**. Up and down pick a part, left and right change it, and you see the result at
once. It is saved as you change it.

The same six settings are in the overlay: press **F1**, open **Config**, tab **Hud**. A slider is saved
when you let go of it.

| Row | What it covers |
|---|---|
| Menus | The Training menu, the pause menu, the Command List and their dialogs. It stops at 10% so a menu never disappears. |
| Battle HUD | Health, timer, EXS and GRD gauges, the character plates and the combo counter. |
| Input history | The input list at the left of the screen in Training. |
| Damage info | The Damage info box in Training. |
| Frame info | The two Frame info boxes in Training, one at each side of Damage info. |
| Mod HUD | This mod's frame meter, GRD popups, health values and proration box. |

Nothing changes outside a fight, and the characters, effects and stage are never touched. The frame
meter's own opacity, in the Training tab of the overlay, still works and multiplies with Mod HUD.

The same settings are in the ini file under [`[HudOpacity]`](The-ini-file).
