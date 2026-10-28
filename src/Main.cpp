#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <vector>
#include <array>
#include <fstream>

class VoiceComponent final : public juce::AudioAppComponent, private juce::Timer
{
public:
    VoiceComponent()
    {
        addAndMakeVisible(status);
        status.setJustificationType(juce::Justification::centred);
        status.setText("CHM VOICE MIDI | waiting for microphone", juce::dontSendNotification);
        addAndMakeVisible(midiSelector);
        midiSelector.onChange = [this] { openMidi(); };
        addAndMakeVisible(refreshButton);
        refreshButton.setButtonText("Refresh MIDI ports");
        refreshButton.onClick = [this] { refreshMidi(); };
        addAndMakeVisible(channelSelector);
        for (int ch = 1; ch <= 16; ++ch) channelSelector.addItem("MIDI Channel " + juce::String(ch), ch);
        channelSelector.setSelectedId(1);
        channelSelector.onChange = [this] { stopNote(); };
        addAndMakeVisible(routingHelp);
        routingHelp.setButtonText("FL Studio routing help");
        routingHelp.onClick = [this] {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "CHM -> FL Keys", "1. Run loopMIDI and create a port named CHM MIDI.\n"
                "2. Click Refresh MIDI ports here; select CHM MIDI.\n"
                "3. FL Studio: Options > MIDI Settings (F10).\n"
                "4. Input: CHM MIDI > Enable; controller: Generic Controller.\n"
                "5. Open FL Keys and select its channel.\n"
                "6. Sing into the microphone. Melody/chords use the selected MIDI channel; "
                "beatbox uses channel 10.\n\n"
                "Keep loopMIDI running. MIDI monitoring and timing depend on your audio setup.");
        };
        addAndMakeVisible(threshold);
        threshold.setRange(0.005, 0.12, 0.005);
        threshold.setValue(0.025);
        threshold.onValueChange = [this] { gate.store((float) threshold.getValue()); };
        addAndMakeVisible(gateLabel);
        gateLabel.setText("Input gate", juce::dontSendNotification);
        addAndMakeVisible(noteLabel);
        noteLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(modeSelector);
        modeSelector.addItem("Melody", 1);
        modeSelector.addItem("Chords (major triad)", 2);
        modeSelector.addItem("Beatbox (experimental)", 3);
        modeSelector.setSelectedId(1);
        modeSelector.onChange = [this] { stopNote(); };
        addAndMakeVisible(keySelector);
        const char* keys[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        for (int i=0;i<12;++i) keySelector.addItem(keys[i], i+1);
        keySelector.setSelectedId(1);
        addAndMakeVisible(scaleSelector);
        scaleSelector.addItem("Chromatic", 1);
        scaleSelector.addItem("Major", 2);
        scaleSelector.addItem("Minor", 3);
        scaleSelector.setSelectedId(1);
        addAndMakeVisible(recordButton);
        recordButton.setButtonText("Start MIDI capture");
        recordButton.onClick = [this] { toggleRecord(); };
        addAndMakeVisible(saveButton);
        saveButton.setButtonText("Export MIDI file");
        saveButton.onClick = [this] { exportMidi(); };
        addAndMakeVisible(panic);
        panic.setButtonText("Stop MIDI notes");
        panic.onClick = [this] { stopNote(); };
        setSize(680, 530);
        setAudioChannels(1, 0);
        refreshMidi();
        startTimerHz(20);
    }
    ~VoiceComponent() override { stopTimer(); stopNote(); shutdownAudio(); }
    void prepareToPlay(int, double rate) override { sampleRate.store(rate); }
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (info.buffer->getNumChannels() == 0) return;
        const auto* input = info.buffer->getReadPointer(0, info.startSample);
        for (int i = 0; i < info.numSamples; ++i)
        {
            const auto w = writeIndex.fetch_add(1, std::memory_order_relaxed);
            ring[w % ringSize].store(input[i], std::memory_order_relaxed);
        }
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(18);
        status.setBounds(area.removeFromTop(40));
        auto midiRow = area.removeFromTop(35);
        refreshButton.setBounds(midiRow.removeFromRight(170));
        midiSelector.setBounds(midiRow);
        channelSelector.setBounds(area.removeFromTop(32));
        modeSelector.setBounds(area.removeFromTop(34));
        auto scales = area.removeFromTop(34);
        keySelector.setBounds(scales.removeFromLeft(120));
        scaleSelector.setBounds(scales);
        area.removeFromTop(12);
        gateLabel.setBounds(area.removeFromTop(25));
        threshold.setBounds(area.removeFromTop(30));
        noteLabel.setBounds(area.removeFromTop(55));
        auto buttons = area.removeFromTop(35);
        recordButton.setBounds(buttons.removeFromLeft(190));
        saveButton.setBounds(buttons.removeFromLeft(190));
        panic.setBounds(buttons);
        area.removeFromTop(12);
        routingHelp.setBounds(area.removeFromTop(36));
    }
private:
    static constexpr int ringSize = 16384;
    static constexpr int windowSize = 4096;
    std::array<std::atomic<float>, ringSize> ring{};
    std::atomic<uint64_t> writeIndex{0};
    std::atomic<double> sampleRate{44100.0};
    std::atomic<float> gate{0.025f};
    juce::Label status, gateLabel, noteLabel;
    juce::ComboBox midiSelector, modeSelector, keySelector, scaleSelector, channelSelector;
    juce::TextButton recordButton, saveButton, refreshButton, routingHelp;
    juce::MidiMessageSequence captured;
    bool recording = false;
    double recordingStart = 0.0;
    std::array<int, 3> sounding {{-1,-1,-1}};
    bool drumOn = false;
    int drumTicks = 0;
    double previousEnergy = 0.0;
    juce::Slider threshold;
    juce::TextButton panic;
    std::unique_ptr<juce::MidiOutput> output;
    int activeNote = -1;
    int soundingChannel = 1;
    int candidate = -1;
    int stableFrames = 0;
    uint64_t lastRead = 0;

    void refreshMidi()
    {
        juce::String previousIdentifier;
        const auto before = juce::MidiOutput::getAvailableDevices();
        const int previousIndex = midiSelector.getSelectedId() - 2;
        if (juce::isPositiveAndBelow(previousIndex, before.size()))
            previousIdentifier = before[previousIndex].identifier;
        stopNote();
        output.reset();
        midiSelector.clear();
        midiSelector.addItem("Select MIDI output (virtual port)", 1);
        const auto devices = juce::MidiOutput::getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
            midiSelector.addItem(devices[i].name, i + 2);
        int selectedId = 1;
        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].identifier == previousIdentifier && previousIdentifier.isNotEmpty()) selectedId = i + 2;
        midiSelector.setSelectedId(selectedId, juce::dontSendNotification);
        if (selectedId > 1) openMidi();
        status.setText("MIDI ports refreshed: " + juce::String(devices.size()) + " found", juce::dontSendNotification);
    }
    void openMidi()
    {
        stopNote();
        output.reset();
        int index = midiSelector.getSelectedId() - 2;
        const auto devices = juce::MidiOutput::getAvailableDevices();
        if (juce::isPositiveAndBelow(index, devices.size()))
            output = juce::MidiOutput::openDevice(devices[index].identifier);
    }
    void emit(juce::MidiMessage msg)
    {
        if (output) output->sendMessageNow(msg);
        if (recording)
        {
            msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() / 1000.0 - recordingStart);
            captured.addEvent(msg);
        }
    }
    void stopNote()
    {
        for (auto& n : sounding)
        {
            if (n >= 0) emit(juce::MidiMessage::noteOff(soundingChannel, n));
            n = -1;
        }
        if (drumOn) emit(juce::MidiMessage::noteOff(10, 36));
        drumOn = false;
        activeNote = -1;
    }
    int quantize(int note) const
    {
        const int scale = scaleSelector.getSelectedId();
        if (scale == 1) return note;
        const int root = keySelector.getSelectedId() - 1;
        const int major[] = {0,2,4,5,7,9,11};
        const int minor[] = {0,2,3,5,7,8,10};
        const int* allowed = scale == 2 ? major : minor;
        int best = note, dist = 128;
        for (int n = juce::jmax(0,note-2); n <= juce::jmin(127,note+2); ++n)
            for (int i=0;i<7;++i)
                if ((n-root+120)%12 == allowed[i] && std::abs(n-note) < dist)
                    { best=n; dist=std::abs(n-note); }
        return best;
    }
    void toggleRecord()
    {
        if (!recording)
        {
            captured.clear();
            recordingStart = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            recording = true;
            recordButton.setButtonText("Stop capture");
        }
        else
        {
            stopNote();
            recording = false;
            recordButton.setButtonText("Start MIDI capture");
        }
    }
    void exportMidi()
    {
        if (recording) toggleRecord();
        if (captured.getNumEvents() == 0) { status.setText("Nothing recorded", juce::dontSendNotification); return; }
        auto target = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                          .getNonexistentChildFile("CHM-Voice-MIDI-Capture", ".mid");
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote(960);
        juce::MidiMessageSequence track;
        track.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0.0);
        for (int i=0; i<captured.getNumEvents(); ++i)
        {
            auto msg = captured.getEventPointer(i)->message;
            msg.setTimeStamp(msg.getTimeStamp() * 1920.0); // 120 BPM, 960 PPQ
            track.addEvent(msg);
        }
        track.updateMatchedPairs();
        midi.addTrack(track);
        if (auto stream = target.createOutputStream())
        {
            if (midi.writeTo(*stream)) status.setText("Saved: " + target.getFullPathName(), juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        auto now = writeIndex.load(std::memory_order_acquire);
        if (now < windowSize || now == lastRead) return;
        lastRead = now;
        std::vector<float> samples(windowSize);
        double energy = 0;
        for (int i = 0; i < windowSize; ++i)
        {
            float x = ring[(now - windowSize + i) % ringSize].load(std::memory_order_relaxed);
            samples[i] = x;
            energy += x * x;
        }
        const double rms = std::sqrt(energy / windowSize);
        int note = -1;
        const double sr = sampleRate.load();
        if (rms > gate.load() && sr > 0)
        {
            // Normalized difference pitch detector: prototype, not production-grade YIN.
            const int minLag = juce::jmax(2, (int)(sr / 1000.0));
            const int maxLag = juce::jmin(windowSize / 2, (int)(sr / 75.0));
            double best = 1e30;
            int bestLag = 0;
            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double diff = 0, norm = 0;
                for (int j = 0; j < windowSize - maxLag; j += 3)
                {
                    const double a = samples[j], b = samples[j + lag];
                    diff += (a - b) * (a - b);
                    norm += a * a + b * b;
                }
                const double score = diff / (norm + 1e-12);
                if (score < best) { best = score; bestLag = lag; }
            }
            if (bestLag > 0 && best < 0.16)
            {
                const double hz = sr / bestLag;
                const int n = juce::roundToInt(69.0 + 12.0 * std::log2(hz / 440.0));
                if (n >= 36 && n <= 96) note = n;
            }
        }
        if (modeSelector.getSelectedId() == 3)
        {
            // Experimental transient-to-kick mapping, not learned beatbox classification.
            if (drumOn && ++drumTicks >= 2)
            {
                emit(juce::MidiMessage::noteOff(10, 36));
                drumOn = false;
            }
            if (rms > gate.load() * 2.0 && rms > previousEnergy * 1.8 && !drumOn)
            {
                emit(juce::MidiMessage::noteOn(10, 36, (juce::uint8)100));
                drumOn = true;
                drumTicks = 0;
            }
            previousEnergy = rms;
            noteLabel.setText("Beatbox transient -> Kick (experimental)", juce::dontSendNotification);
            return;
        }
        note = note < 0 ? -1 : quantize(note);
        if (note == candidate) ++stableFrames;
        else { candidate = note; stableFrames = 1; }
        if (stableFrames >= 2 && note != activeNote)
        {
            stopNote();
            if (note >= 0 && output)
            {
                activeNote = note;
                soundingChannel = channelSelector.getSelectedId();
                const int intervals[] = {0, 4, 7};
                const int count = modeSelector.getSelectedId() == 2 ? 3 : 1;
                for (int i=0;i<count;++i)
                {
                    sounding[i] = note + intervals[i];
                    if (sounding[i] <= 127)
                        emit(juce::MidiMessage::noteOn(soundingChannel, sounding[i], (juce::uint8)100));
                }
            }
        }
        noteLabel.setText(note < 0 ? "No stable pitch" : juce::MidiMessage::getMidiNoteName(note, true, true, 3), juce::dontSendNotification);
        status.setText(output ? "Microphone -> MIDI output active" : "Select a MIDI output port", juce::dontSendNotification);
    }
};
class VoiceApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "CHM Voice MIDI"; }
    const juce::String getApplicationVersion() override { return "0.3.0"; }
    void initialise(const juce::String&) override
    {
        window = std::make_unique<Window>(getApplicationName());
    }
    void shutdown() override { window.reset(); }
private:
    class Window final : public juce::DocumentWindow
    {
    public:
        explicit Window(juce::String name) : DocumentWindow(name, juce::Colours::darkslategrey, allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new VoiceComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(VoiceApp)
