# Sounds

The recordings in [`sounds/`](sounds/) come from [Blanket](https://github.com/rafaelmardojai/blanket) (thank you!), which collected and edited them for looping. potter downloads each one the first time you turn it on. White, pink and brown noise aren't files: mpv generates them live (FFmpeg's `anoisesrc`).

| Sound | Author | Edited by | License |
| --- | --- | --- | --- |
| [Birds](https://freesound.org/people/kvgarlic/sounds/156826/) | kvgarlic | Porrumentzio | CC0 |
| [Boat](https://freesound.org/people/Falcet/sounds/439365/) | Falcet | Porrumentzio | CC0 |
| [City](https://freesound.org/people/gezortenplotz/sounds/44796/) | gezortenplotz | Porrumentzio | CC BY |
| [Coffee shop](https://soundbible.com/1664-Restaurant-Ambiance.html) | stephan | – | Public domain |
| [Fireplace](https://soundbible.com/1543-Fireplace.html) | ezwa | – | Public domain |
| [Rain](https://freesound.org/people/alex36917/sounds/524605/) | alex36917 | Porrumentzio | CC BY |
| [Storm](https://freesound.org/people/digifishmusic/sounds/41739/) | Digifish music | Porrumentzio | CC BY |
| [Stream](https://freesound.org/people/gluckose/sounds/333987/) | gluckose | – | CC0 |
| [Summer night](https://soundbible.com/2083-Crickets-Chirping-At-Night.html) | Lisa Redfern | – | Public domain |
| [Train](https://freesound.org/people/SDLx/sounds/259988/) | SDLx | – | CC BY 3.0 |
| [Waves](https://freesound.org/people/Luftrum/sounds/48412/) | Luftrum | Porrumentzio | CC BY |
| [Wind](https://freesound.org/people/felix.blume/sounds/217506/) | felix.blume | Porrumentzio | CC0 |

Licensing details as listed in Blanket's [SOUNDS_LICENSING.md](https://github.com/rafaelmardojai/blanket/blob/master/SOUNDS_LICENSING.md).

Adding a sound to potter: put the `.ogg` in `sounds/` (never rename or remove one there: older potter versions download them by name), add it to `builtin_sounds()` in `src/core/sounds.cpp` with its size, and credit it here.
