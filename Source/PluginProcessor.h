#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ステップに置けるエフェクト。値は規則が決める。こちらが決めていいのは順番だけ。
enum class StepFx
{
    off = 0,
    stutter,
    reverse,
    tapestop,
    crush,
    gate,
    count
};

class HakaiSysProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSteps = 16;

    HakaiSysProcessor();
    ~HakaiSysProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                    { return true; }

    const juce::String getName() const override        { return "hakai.sys"; }
    bool acceptsMidi() const override                  { return false; }
    bool producesMidi() const override                 { return false; }
    double getTailLengthSeconds() const override       { return 0.0; }

    int getNumPrograms() override                      { return 1; }
    int getCurrentProgram() override                   { return 0; }
    void setCurrentProgram (int) override              {}
    const juce::String getProgramName (int) override   { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // UI 表示用: いま踏んでいるステップ
    std::atomic<int> uiCurrentStep { -1 };

    static juce::String stepParamID (int step) { return "step" + juce::String (step); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void startStep (int stepIndex, double initialPhase = 0.0);
    float readCapture (int channel, double pos) const; // 線形補間、循環

    double sr = 44100.0;

    // 入力を常に録っておく循環バッファ（過去の音がエフェクトの材料になる）
    juce::AudioBuffer<float> capture;
    int capLen = 0;
    int capPos = 0;

    double stepPhase = 0.0;       // いまのステップ内の位置（サンプル）
    double stepLenSamples = 1.0;
    int currentStep = -1;
    StepFx currentFx = StepFx::off;
    int stepStartCapPos = 0;
    double tapePos = 0.0;
    float heldSample[2] = { 0.0f, 0.0f }; // crush のダウンサンプル用
    int holdCounter = 0;

    juce::SmoothedValue<float> wetGain; // ステップ境界のクリック防止

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HakaiSysProcessor)
};
