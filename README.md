# CHM VOICE MIDI v0.5 — Compact UI

New compact dark/cyan JUCE interface based on the approved concept. This is **source code, not a tested Windows binary**.

## Build
Upload `src/Main.cpp`, `CMakeLists.txt`, and `.github/workflows/build-windows.yml` to the repository. GitHub Actions compiles the EXE and uploads an artifact.

## Controls
- MIC / AUDIO SETTINGS: select audio device and microphone.
- Select loopMIDI port under MIDI ROUTING, refresh ports when needed.
- Select mode (melody, major chords, experimental kick transient), scale and key.
- START VOICE MIDI starts pitch detection; STOP sends note-off.
- Input gate controls minimum microphone level.
- Capture and export MIDI events to Documents.

## Limitations
- UI is coded but **not compiled or runtime tested** here.
- Waveform uses microphone input; it will be flat without signal.
- Pitch detection is an experimental monophonic prototype; accuracy and latency are unverified.
- Beatbox only detects transients for kick, not full drum classification.
- Requires loopMIDI (or equivalent virtual MIDI port) for FL Studio.
- No proprietary Dubler 2 source or assets used.
