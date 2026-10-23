#pragma once

#include "PluginProcessor.h"

// ひゅーまんの紙とドット。
namespace hyu
{
    const juce::Colour paper  { 0xfff7f0e4 };   // 紙
    const juce::Colour paper2 { 0xfffff9ee };   // 紙（あかるい）
    const juce::Colour ink    { 0xff8a3a2e };   // 赤茶
    const juce::Colour pink   { 0xffff6b8a };   // ほお
    const juce::Colour rose   { 0xffd67070 };   // かげ

    juce::Typeface::Ptr dotFont();
}

// 紙の見た目（つまみ・選択箱・メニューぜんぶ角ばらせる）
class HakaiLook : public juce::LookAndFeel_V4
{
public:
    HakaiLook();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return hyu::dotFont(); }
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down,
                       int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

// 16×6 のマス目。クリックでそのステップにこわしかたを置く。
class StepGrid : public juce::Component,
                 private juce::Timer
{
public:
    explicit StepGrid (HakaiSysProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    int stepChoice (int step) const;

    HakaiSysProcessor& proc;
};

// /halt/ の子。右上でまばたきしていて、ステップを踏むとほおが光る。
class KidFace : public juce::Component,
                private juce::Timer
{
public:
    explicit KidFace (HakaiSysProcessor& p) : proc (p) { startTimerHz (12); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    HakaiSysProcessor& proc;
    int tick = 0;
    bool blink = false;
    int lastStep = -1;
    float cheekGlow = 0.0f;
};

class HakaiSysEditor : public juce::AudioProcessorEditor
{
public:
    explicit HakaiSysEditor (HakaiSysProcessor&);
    ~HakaiSysEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HakaiSysProcessor& proc;
    HakaiLook look;

    StepGrid grid;
    KidFace kid;
    juce::Slider mixSlider, bitsSlider;
    juce::ComboBox divBox;
    juce::Label mixLabel, bitsLabel, divLabel, title, subtitle;

    juce::AudioProcessorValueTreeState::SliderAttachment mixAtt, bitsAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HakaiSysEditor)
};
