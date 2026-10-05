# Portraits

Give each character another art on every screen that shows it: the character select portrait and its
card in the grid, the versus screen, the face on the battle gauge, the winner screen and the main menu.

Press **F1**, open the **Misc** section of the main window, press **Open misc** and go to the
**Portraits** tab.

## Picking an art

Every character has its own list. **Game's own** is the art the game ships with. The others are:

| Art | Where it comes from |
|---|---|
| Sys:Celes | The game's Gallery, with its effects. Some characters have alternates, such as Orie without Thanatos. |
| Sys:Celes, no effects | The same illustration the game draws, without effects or companion. |
| UNDER NIGHT IN-BIRTH, Exe:Late, Exe:Late[st], Exe:Late[cl-r] | The character select art of the older games. |
| BBTAG | The character select art of BlazBlue Cross Tag Battle. |
| Victory | The victory portrait of the older games. |
| Story | The standing art of the story dialogue, from the game's Gallery. |
| Chibi | The SD art from the game's Gallery. |

Each art is placed by the character's face, so the head lands where the game's own head is on every
screen, whatever the pose. A bust that ends inside a screen fades out instead of showing a hard edge.

Hover a list and turn the mouse wheel to step through it. A change shows the next time each screen
opens.

## Everyone

The **Everyone** list gives every character the same kind of art in one go:

- **Old Arts**: the select art of UNI, Exe:Late, [st] or [cl-r], whichever the character had.
- **Sys:Celes**, **Sys:Celes, no effects**, **BBTAG**, **Victory**, **Story**, **Chibi**.
- **Game's own**: everyone back to the game's art.

A character without that kind of art wears the game's own. Tsurugi, Uzuki, Kaguya, Kuon, Ogre, Izumi
and Zohar are new in UNI2, so they have no old art.

Applying a style to everyone takes about ten seconds: the mod paints up to four characters at once.

## The portrait pack

The Gallery art is read from your copy of the game. The art of the older games, BBTAG and the
renders without effects come from the [inbirth.wiki.gg](https://inbirth.wiki.gg/wiki/Gallery) gallery,
gathered in one portrait pack of about 120 MB.

**Download the portrait pack** fetches it once and keeps the pictures in `UNI2-IM\Portraits\library`.
Picking one of those arts before that fetches the pack too. Arts that still need it are marked
**(download)** in the lists.

The link lives in the mod. To use another one, put it in [the ini file](The-ini-file):

```ini
[Portraits]
PackUrl=https://drive.google.com/file/d/<id>/view?usp=sharing
```

A Google Drive share link works as it is.

## Where the files go

| Folder | What it holds |
|---|---|
| `UNI2-IM\Portraits\library` | The pictures from the pack. |
| `UNI2-IM\Portraits\screens` | The game files painted with your picks. The game reads these instead of its own. |
| `UNI2-IM\Portraits\cards` | One painted grid card per character. |

Each character with another art takes about 70 MB, because the painted screens are stored at twice the
game's resolution. That is also why they look sharper than the game's own at 1080p and above.

**Game's own** removes that character's files. **Everyone > Game's own** puts every character back.

The older "Old portraits" option, which read the art from a copy of UNI[st], is gone. Its files are
removed the first time the Portraits tab opens.
