#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    constexpr int numFx = (int) StepFx::count;
    const char* rowNames[numFx] = { "OFF", "STUT", "REV", "STOP", "CRSH", "GATE" };

    // こわしかたのドット絵（5×5）。1 のマスを塗る
    const char* icons[numFx][5] = {
        { "     ", "     ", "     ", "     ", "     " },              // off
        { "1 1 1", "1 1 1", "1 1 1", "1 1 1", "1 1 1" },              // stutter 縞
        { "   1 ", "  11 ", " 111 ", "  11 ", "   1 " },              // reverse ◀
        { "111  ", " 11  ", "  11 ", "   11", "    1" },              // tapestop 落ちる
        { "11 11", "11 11", "     ", "11 11", "11 11" },              // crush 大きい粒
        { "1 1 1", "     ", "1 1 1", "     ", "1 1 1" },              // gate 点滅
    };

    void drawIcon (juce::Graphics& g, juce::Rectangle<float> r, int row, juce::Colour c)
    {
        const float px = juce::jmin (r.getWidth(), r.getHeight()) / 7.0f;
        const float x0 = r.getCentreX() - px * 2.5f, y0 = r.getCentreY() - px * 2.5f;
        g.setColour (c);
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 5; ++x)
                if (icons[row][y][x] == '1')
                    g.fillRect (x0 + x * px, y0 + y * px, px + 0.5f, px + 0.5f);
    }
}

juce::Typeface::Ptr hyu::dotFont()
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (
        BinaryData::DotGothic16Regular_ttf, BinaryData::DotGothic16Regular_ttfSize);
    return tf;
}

// ── 紙の見た目 ──────────────────────────────────────────────────
HakaiLook::HakaiLook()
{
    setColour (juce::Label::textColourId, hyu::ink);
    setColour (juce::Slider::textBoxTextColourId, hyu::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, hyu::ink);
    setColour (juce::PopupMenu::backgroundColourId, hyu::paper2);
    setColour (juce::PopupMenu::textColourId, hyu::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, hyu::pink);
    setColour (juce::PopupMenu::highlightedTextColourId, hyu::paper);
}

void HakaiLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                  float pos, float a0, float a1, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h)
                 .reduced (6.0f).withSizeKeepingCentre (
                     juce::jmin (w, h) - 12.0f, juce::jmin (w, h) - 12.0f);
    g.setColour (hyu::paper2);  g.fillRect (r);
    g.setColour (hyu::ink);     g.drawRect (r, 2.0f);

    // 目もり（角ばった点々）
    g.setColour (hyu::rose);
    for (int i = 0; i <= 8; ++i)
    {
        const float ang = a0 + (a1 - a0) * (float) i / 8.0f;
        const auto p = r.getCentre().getPointOnCircumference (r.getWidth() * 0.58f, ang);
        g.fillRect (p.x - 1.5f, p.y - 1.5f, 3.0f, 3.0f);
    }

    // 針（太いドットの線）
    const float ang = a0 + (a1 - a0) * pos;
    const auto c = r.getCentre();
    g.setColour (hyu::pink);
    for (int i = 2; i < 9; ++i)
    {
        const auto p = c.getPointOnCircumference (r.getWidth() * 0.05f * (float) i, ang);
        g.fillRect (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
    }
    g.setColour (hyu::ink);
    g.fillRect (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
}

void HakaiLook::drawComboBox (juce::Graphics& g, int w, int h, bool,
                              int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (hyu::paper2); g.fillRect (r);
    g.setColour (hyu::ink);    g.drawRect (r, 2.0f);
    // ▼ をドットで
    const float bx = (float) w - 16.0f, by = (float) h * 0.5f - 3.0f;
    g.setColour (hyu::pink);
    g.fillRect (bx,        by,        9.0f, 3.0f);
    g.fillRect (bx + 1.5f, by + 3.0f, 6.0f, 3.0f);
    g.fillRect (bx + 3.0f, by + 6.0f, 3.0f, 3.0f);
}

// ── マス目 ──────────────────────────────────────────────────────
StepGrid::StepGrid (HakaiSysProcessor& p) : proc (p)
{
    startTimerHz (30);
}

int StepGrid::stepChoice (int step) const
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (
            proc.apvts.getParameter (HakaiSysProcessor::stepParamID (step))))
        return param->getIndex();
    return 0;
}

void StepGrid::paint (juce::Graphics& g)
{
    const float cw = (float) getWidth()  / HakaiSysProcessor::numSteps;
    const float ch = (float) getHeight() / numFx;
    const int playing = proc.uiCurrentStep.load();

    for (int col = 0; col < HakaiSysProcessor::numSteps; ++col)
    {
        const int active = stepChoice (col);
        const bool beat4 = (col % 4 == 0);

        for (int row = 0; row < numFx; ++row)
        {
            auto r = juce::Rectangle<float> (col * cw, row * ch, cw, ch).reduced (2.0f);
            const bool isActive = (row == active && active != 0);

            if (isActive)
            {
                g.setColour (hyu::pink);  g.fillRect (r);
                g.setColour (hyu::ink);   g.drawRect (r, 2.0f);
                drawIcon (g, r, row, hyu::paper);
            }
            else
            {
                g.setColour (hyu::paper2); g.fillRect (r);
                g.setColour (beat4 ? hyu::rose : hyu::rose.withAlpha (0.35f));
                g.drawRect (r, 1.0f);
                if (row == active)                     // OFF の行のしるし（ちいさな点）
                    { g.setColour (hyu::rose); g.fillRect (r.getCentreX() - 1.5f, r.getCentreY() - 1.5f, 3.0f, 3.0f); }
                else
                    drawIcon (g, r, row, hyu::rose.withAlpha (0.22f));
            }
        }

        if (col == playing)
        {
            g.setColour (hyu::pink.withAlpha (0.14f));
            g.fillRect (juce::Rectangle<float> (col * cw, 0.0f, cw, (float) getHeight()));
            g.setColour (hyu::ink);
            g.fillRect (juce::Rectangle<float> (col * cw + 2.0f, (float) getHeight() - 4.0f, cw - 4.0f, 4.0f));
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int col = juce::jlimit (0, HakaiSysProcessor::numSteps - 1,
                                  (int) (e.position.x / ((float) getWidth() / HakaiSysProcessor::numSteps)));
    const int row = juce::jlimit (0, numFx - 1,
                                  (int) (e.position.y / ((float) getHeight() / numFx)));

    if (auto* param = proc.apvts.getParameter (HakaiSysProcessor::stepParamID (col)))
    {
        const int newIndex = (stepChoice (col) == row) ? 0 : row;
        param->beginChangeGesture();
        param->setValueNotifyingHost ((float) newIndex / (float) (numFx - 1));
        param->endChangeGesture();
        repaint();
    }
}

// ── /halt/ の子 ─────────────────────────────────────────────────
void KidFace::timerCallback()
{
    ++tick;
    blink = (tick % 40) < 3;                       // ときどき、まばたき
    const int s = proc.uiCurrentStep.load();
    if (s != lastStep)
    {
        lastStep = s;
        if (s >= 0 && [this, s] {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                        proc.apvts.getParameter (HakaiSysProcessor::stepParamID (s))))
                    return p->getIndex() != 0;
                return false;
            }())
            cheekGlow = 1.0f;                      // こわしたら、ほおが光る
    }
    cheekGlow *= 0.86f;
    repaint();
}

void KidFace::paint (juce::Graphics& g)
{
    const float px = juce::jmin ((float) getWidth() / 26.0f, (float) getHeight() / 24.0f);
    const float cx = getWidth() * 0.5f, cy = getHeight() * 0.52f;
    auto R = [&] (float x, float y, float w, float h, juce::Colour c)
    {
        g.setColour (c);
        g.fillRect (cx + x * px, cy + y * px, w * px, h * px);
    };
    R (-9, -9, 18, 18, hyu::ink); R (-11, -5, 22, 11, hyu::ink);
    const auto cheek = hyu::pink.interpolatedWith (juce::Colours::white, cheekGlow * 0.55f);
    R (-10, 1, 4, 3, cheek); R (6, 1, 4, 3, cheek);
    if (blink) { R (-7, -1, 5, 1, hyu::paper); R (2, -1, 5, 1, hyu::paper); }
    else       { R (-7, -3, 4, 4, hyu::paper); R (3, -3, 4, 4, hyu::paper); }
}

// ── まとめ ──────────────────────────────────────────────────────
HakaiSysEditor::HakaiSysEditor (HakaiSysProcessor& p)
    : AudioProcessorEditor (p), proc (p), grid (p), kid (p),
      mixAtt  (p.apvts, "mix",        mixSlider),
      bitsAtt (p.apvts, "crushBits",  bitsSlider)
{
    setLookAndFeel (&look);

    title.setText ("hakai.sys", juce::dontSendNotification);
    title.setFont (juce::FontOptions (26.0f));
    title.setColour (juce::Label::textColourId, hyu::ink);
    addAndMakeVisible (title);

    subtitle.setText (juce::String::fromUTF8 ("こわしかたは、規則がきめる。"), juce::dontSendNotification);
    subtitle.setFont (juce::FontOptions (13.0f));
    subtitle.setColour (juce::Label::textColourId, hyu::rose);
    addAndMakeVisible (subtitle);

    addAndMakeVisible (grid);
    addAndMakeVisible (kid);

    for (auto* s : { &mixSlider, &bitsSlider })
    {
        s->setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        addAndMakeVisible (*s);
    }
    mixLabel.setText ("mix", juce::dontSendNotification);
    bitsLabel.setText ("bits", juce::dontSendNotification);
    divLabel.setText ("stutter", juce::dontSendNotification);
    for (auto* l : { &mixLabel, &bitsLabel, &divLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*l);
    }

    divBox.addItemList (juce::StringArray { "1/32", "1/64", "1/128" }, 1);
    divBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (divBox);
    divAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, "stutterDiv", divBox);

    setSize (760, 380);
}

HakaiSysEditor::~HakaiSysEditor()
{
    setLookAndFeel (nullptr);
}

void HakaiSysEditor::paint (juce::Graphics& g)
{
    g.fillAll (hyu::paper);

    // ふち（紙の額）
    g.setColour (hyu::rose);
    g.drawRect (getLocalBounds(), 3);

    // 行ラベル
    const auto gb = grid.getBounds().toFloat();
    const float ch = gb.getHeight() / (float) numFx;
    g.setColour (hyu::ink);
    g.setFont (juce::FontOptions (13.0f));
    for (int row = 0; row < numFx; ++row)
        g.drawText (rowNames[row],
                    juce::Rectangle<float> (10.0f, gb.getY() + row * ch, 46.0f, ch),
                    juce::Justification::centredLeft);
}

void HakaiSysEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (44);
    kid.setBounds (top.removeFromRight (64));
    title.setBounds (top.removeFromLeft (170));
    subtitle.setBounds (top.withTrimmedLeft (2).withTrimmedTop (12));
    area.removeFromTop (4);

    auto controls = area.removeFromBottom (100);
    area.removeFromBottom (8);

    grid.setBounds (area.withTrimmedLeft (50));

    auto knob = [&controls] (int w) { return controls.removeFromLeft (w).reduced (4); };
    auto mixArea  = knob (110);
    auto bitsArea = knob (110);
    auto divArea  = knob (150);

    mixLabel.setBounds (mixArea.removeFromTop (16));
    mixSlider.setBounds (mixArea);
    bitsLabel.setBounds (bitsArea.removeFromTop (16));
    bitsSlider.setBounds (bitsArea);
    divLabel.setBounds (divArea.removeFromTop (16));
    divBox.setBounds (divArea.removeFromTop (30).reduced (0, 2));
}
