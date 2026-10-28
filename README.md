# CHM Voice MIDI — experimental Windows standalone application

Original implementation inspired by the **category** of voice-to-MIDI tools; not affiliated with Vochlea. No proprietary Dubler code or assets included.

## Implemented in source (NOT validated in a Windows build)
- Microphone capture with JUCE AudioAppComponent.
- Experimental monophonic pitch-to-MIDI conversion and noise gate.
- MIDI output device selection (use a virtual MIDI cable such as loopMIDI).
- Chromatic/major/minor note quantization and root key selector.
- Experimental fixed major triads from sung root notes.
- Experimental transient-to-kick (MIDI channel 10, note 36); **not** a multi-class beatbox recognizer.
- MIDI event capture and MIDI file export to Documents, assuming 120 BPM.
- Panic/stop notes.

## Not implemented
Dubler 2 feature parity is NOT achieved. No voice-trained kick/snare/hat classification, per-user calibration, advanced velocity mapping, continuous pitch bend, MIDI CC from voice, expressive chord voicings, MIDI capture synchronization to DAW, polished UI, onboarding, signed installer, or end-to-end latency validation.

## Build
Use GitHub Actions `Build Windows EXE` workflow or configure with CMake 3.20+ and Visual Studio 2022 x64. JUCE 8.0.10 is fetched from GitHub at configure time. The workflow must run on Windows to validate the build.

## Runtime
Install and configure loopMIDI (or another MIDI virtual output) on Windows, create a port, select it in this app, and set that port as MIDI input in your DAW. Grant Windows microphone access. Do not assume zero latency; measure round-trip delay in a real DAW test.

## License
See LICENSE. Review JUCE license terms before distribution, especially closed-source or commercial distribution.


## FL Studio / FL Keys routing (v0.3 experimental)
1. Install and run loopMIDI (Tobias Erichsen); create virtual port `CHM MIDI`.
2. Launch CHM Voice MIDI; click **Refresh MIDI ports**, choose `CHM MIDI` as MIDI output.
3. Choose MIDI channel 1 for melody/chords (beatbox uses channel 10).
4. In FL Studio press F10 > MIDI Settings > Input > `CHM MIDI` > Enable, controller type Generic Controller.
5. Open FL Keys in Channel Rack and select the FL Keys channel. Sing into the mic.
6. If the port does not appear, keep loopMIDI open and refresh. Use **Stop MIDI notes** to stop stuck notes.

**Status:** Routing UI and JUCE MIDI output code are implemented, but compilation and end-to-end audio/MIDI testing have NOT been completed. This application does not create its own Windows virtual MIDI port; loopMIDI is a separate dependency.
