#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::StringArray fxNames { "off", "stutter", "reverse", "tapestop", "crush", "gate" };
}

HakaiSysProcessor::HakaiSysProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "params", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout HakaiSysProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < numSteps; ++i)
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { stepParamID (i), 1 },
            "Step " + juce::String (i + 1), fxNames, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "stutterDiv", 1 }, "Stutter Div",
        juce::StringArray { "1/32", "1/64", "1/128" }, 1));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "crushBits", 1 }, "Crush Bits", 2, 12, 6));

    return layout;
}

void HakaiSysProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    capLen = juce::nextPowerOfTwo ((int) (sampleRate * 2.0)); // 2秒ぶんの過去
    capture.setSize (2, capLen);
    capture.clear();
    capPos = 0;
    currentStep = -1;
    stepPhase = 0.0;
    wetGain.reset (sampleRate, 0.003); // 3ms フェード
    wetGain.setCurrentAndTargetValue (0.0f);
}

bool HakaiSysProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
         || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void HakaiSysProcessor::startStep (int stepIndex, double initialPhase)
{
    currentStep = stepIndex;
    uiCurrentStep.store (stepIndex % numSteps);

    auto* p = dynamic_cast<juce::AudioParameterChoice*> (
        apvts.getParameter (stepParamID (stepIndex % numSteps)));
    currentFx = p != nullptr ? (StepFx) p->getIndex() : StepFx::off;

    // 途中から入ったときは、ステップの頭がどこだったかまで巻き戻して覚える
    // （そうしないと stutter/reverse が「まだ書いていない場所」を読んでゴミが鳴る）
    stepStartCapPos = capPos - (int) initialPhase;
    tapePos = 0.0;
    holdCounter = 0;
    wetGain.setTargetValue (currentFx == StepFx::off ? 0.0f : 1.0f);
}

float HakaiSysProcessor::readCapture (int channel, double pos) const
{
    // pos は絶対位置（サンプル）。循環バッファを線形補間で読む。
    const auto* data = capture.getReadPointer (juce::jmin (channel, capture.getNumChannels() - 1));
    const int mask = capLen - 1;
    const int i0 = ((int) std::floor (pos)) & mask;
    const int i1 = (i0 + 1) & mask;
    const float frac = (float) (pos - std::floor (pos));
    return data[i0] + frac * (data[i1] - data[i0]);
}

void HakaiSysProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    double bpm = 120.0;
    bool hostSync = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm())
                bpm = juce::jmax (20.0, *pos->getBpm());

            if (pos->getIsPlaying() && pos->getPpqPosition())
            {
                // ホスト再生中はトランスポートに吸着する（サンプル精度で再同期）
                const double stepFloat = *pos->getPpqPosition() * 4.0; // 1/16 単位
                const int stepIdx = (int) std::floor (stepFloat);
                stepLenSamples = sr * 60.0 / bpm / 4.0;
                const double newPhase = (stepFloat - (double) stepIdx) * stepLenSamples;

                if (stepIdx != currentStep)
                {
                    stepPhase = newPhase;
                    startStep (stepIdx, newPhase);
                }
                else
                {
                    stepPhase = newPhase;
                }
                hostSync = true;
            }
        }
    }

    stepLenSamples = juce::jmax (32.0, sr * 60.0 / bpm / 4.0);

    if (currentStep < 0)
        startStep (0);

    const float mix = apvts.getRawParameterValue ("mix")->load();
    const int divIdx = (int) apvts.getRawParameterValue ("stutterDiv")->load();
    const double sliceLen = juce::jmax (16.0, stepLenSamples / std::pow (2.0, divIdx + 1)); // 1/32, 1/64, 1/128
    const int bits = (int) apvts.getRawParameterValue ("crushBits")->load();
    const float quant = (float) (1 << (bits - 1));
    const int mask = capLen - 1;

    for (int n = 0; n < numSamples; ++n)
    {
        // 1) 入力を録る（エフェクトはこの過去から読む）
        for (int ch = 0; ch < numCh; ++ch)
            capture.setSample (ch, capPos, buffer.getSample (ch, n));

        // 2) ステップ境界
        if (stepPhase >= stepLenSamples)
        {
            stepPhase -= stepLenSamples;
            startStep (currentStep + 1, stepPhase);
        }

        const float g = wetGain.getNextValue();
        const double phase = stepPhase;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = buffer.getSample (ch, n);
            float wet = dry;

            switch (currentFx)
            {
                case StepFx::stutter:
                {
                    const double idx = std::fmod (phase, sliceLen);
                    wet = readCapture (ch, (double) stepStartCapPos + idx);
                    break;
                }
                case StepFx::reverse:
                {
                    // ステップ開始直前の音を逆向きに歩く
                    wet = readCapture (ch, (double) stepStartCapPos - 1.0 - phase + capLen);
                    break;
                }
                case StepFx::tapestop:
                {
                    wet = readCapture (ch, (double) stepStartCapPos + tapePos);
                    break;
                }
                case StepFx::crush:
                {
                    if (ch == 0 && holdCounter == 0)
                    {
                        heldSample[0] = std::floor (buffer.getSample (0, n) * quant) / quant;
                        if (numCh > 1)
                            heldSample[1] = std::floor (buffer.getSample (1, n) * quant) / quant;
                    }
                    wet = heldSample[juce::jmin (ch, 1)];
                    break;
                }
                case StepFx::gate:
                {
                    const double gateLen = stepLenSamples / 8.0;
                    const bool on = ((int) (phase / gateLen)) % 2 == 0;
                    wet = on ? dry : 0.0f;
                    break;
                }
                case StepFx::off:
                case StepFx::count:
                default:
                    break;
            }

            const float fxOut = dry + g * (wet - dry);        // ステップ境界フェード
            buffer.setSample (ch, n, dry + mix * (fxOut - dry)); // 全体 Mix
        }

        // 3) 位置を進める
        if (currentFx == StepFx::tapestop)
        {
            const double rate = juce::jmax (0.0, 1.0 - phase / stepLenSamples);
            tapePos += rate;
        }
        if (currentFx == StepFx::crush)
            holdCounter = (holdCounter + 1) % 4; // 1/4 レートに間引く

        capPos = (capPos + 1) & mask;
        stepPhase += 1.0;
        juce::ignoreUnused (hostSync);
    }
}

void HakaiSysProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HakaiSysProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HakaiSysProcessor::createEditor()
{
    return new HakaiSysEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HakaiSysProcessor();
}
