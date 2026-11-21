# MotifKit

A melody motif generator (VST3 instrument) for FL Studio.
It makes one short rhythm that repeats, and on every repeat **only the notes change**, following the chords.
So you get a looping melody where the rhythm stays the same.

## Build (GitHub Actions)
1. Make a new repo (for example `D-TEKT/MotifKit`) and upload everything in this folder, **including the `.github` folder**.
2. Go to **Actions**: "Build macOS" and "Build Windows" run on their own.
3. When a run is green, download the artifact at the bottom of the run page (`MotifKit-macOS-VST3` / `MotifKit-Windows-VST3`).

## Install
- **Mac:** put `MotifKit.vst3` in `~/Library/Audio/Plug-Ins/VST3/`, then in Terminal run:
  `xattr -cr ~/Library/Audio/Plug-Ins/VST3/MotifKit.vst3`
- **Windows:** put `MotifKit.vst3` in `C:\Program Files\Common Files\VST3\`
- In FL Studio: Options > Manage plugins > Find more plugins, then add **MotifKit** as an **instrument** in the Channel rack.

## How to use
- **NEW MOTIF**: new rhythm + new notes. **NEW NOTES**: same rhythm, new notes. **NEW RHYTHM**: same note order, new rhythm. **MUTATE**: changes a few notes. **UNDO**: goes back.
- **PREVIEW** plays the loop while FL is stopped. When FL plays, the loop follows the song position.
- **DRAG MIDI**: drag it onto FL's piano roll (or the playlist) to drop the melody in as notes.
- **SAVE MIDI**: saves a `.mid` file.
- In the piano roll: drag a note up/down (every repeat follows), right-click deletes it, double-click adds one.

### Harmony
- **Key / Scale / Octave**: where the melody sits.
- **Chords**: the 4-chord progression the repeats follow (shown above each repeat).
- **Note mode**
  - *Smooth*: notes move as little as possible to fit each chord (most "hook" feel).
  - *Sequence*: the whole shape moves up/down with the chord.
  - *Call & Response*: every 2nd repeat answers, and the last one resolves home.
- **Repeats**: 2x / 4x / 8x.

### Motif
- **Style**: rhythm style (Hardstyle, Rawstyle, Trance, House, House Rap / Hip-Hop, Pop, Afro, Cinematic).
- **Length**: 1/2 bar, 1 bar or 2 bars.
- **Density / Catchy / Range**: these are used the next time you press New Motif / New Rhythm.
- **Variation**: small note changes per repeat (the rhythm stays the same).

### Playback
- **Play mode**: *Sync to song*, or *Hold key*: the motif only plays while you hold a key, transposed to that key.
- **Sound**: built-in Pluck / Lead / Soft, or *Off (MIDI only)*.
  To play it through another synth, set the MIDI output port in FL's wrapper (gear icon > MIDI > Output port) and set the same input port on the other synth.
- **Gate / Swing / Volume**.
