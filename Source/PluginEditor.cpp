#include "PluginEditor.h"

#include <cmath>
#include <limits>
#include <utility>

namespace
{
const juce::Colour background { 0xff0b1015 };
const juce::Colour surface { 0xff121a22 };
const juce::Colour surfaceRaised { 0xff18232d };
const juce::Colour border { 0xff2b3b48 };
const juce::Colour text { 0xffe9f2f5 };
const juce::Colour muted { 0xff8499a6 };
const juce::Colour accent { 0xff65e0bf };
const juce::Colour warm { 0xffffb66b };
const juce::Colour danger { 0xffff786f };

float normalizedMeter(float db)
{
    return juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
}
}

NorthstarMasteringAudioProcessorEditor::NorthstarMasteringAudioProcessorEditor(
    NorthstarMasteringAudioProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse),
      processor(processorToUse),
      eqGraph(processorToUse)
{
    setSize(1120, 720);
    setResizable(true, true);
    setResizeLimits(850, 610, 1500, 980);

    brandLabel.setText("NORTHSTAR", juce::dontSendNotification);
    brandLabel.setFont(juce::Font(juce::FontOptions(21.0f, juce::Font::bold)));
    brandLabel.setColour(juce::Label::textColourId, text);
    addAndMakeVisible(brandLabel);
    subtitleLabel.setText("MASTERING SYSTEM  /  10 SECOND LEARN", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    subtitleLabel.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(subtitleLabel);

    for (auto* tab : { &eqTab, &loudnessTab, &saturationTab, &stereoTab, &compressorTab })
    {
        styleButton(*tab, false);
        tab->setClickingTogglesState(false);
        addAndMakeVisible(*tab);
    }
    eqTab.onClick = [this] { setPage(Page::eq); };
    loudnessTab.onClick = [this] { setPage(Page::loudness); };
    saturationTab.onClick = [this] { setPage(Page::saturation); };
    stereoTab.onClick = [this] { setPage(Page::stereo); };
    compressorTab.onClick = [this] { setPage(Page::compressor); };

    styleButton(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(
        processor.parameters, "bypass", bypassButton);
    addAndMakeVisible(bypassButton);

    analyzeButton.setColour(juce::TextButton::buttonColourId, accent);
    analyzeButton.setColour(juce::TextButton::textColourOffId, background);
    analyzeButton.setColour(juce::TextButton::buttonOnColourId, accent);
    analyzeButton.onClick = [this] { processor.requestAnalysis(); };
    addAndMakeVisible(analyzeButton);
    styleButton(learnButton, false);
    learnButton.setButtonText("LEARN 10 SEC");
    learnButton.onClick = [this] { processor.requestAnalysis(); };
    addAndMakeVisible(learnButton);

    const auto styleLabel = [this](juce::Label& target, const juce::String& value)
    {
        target.setText(value, juce::dontSendNotification);
        target.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        target.setColour(juce::Label::textColourId, muted);
        addAndMakeVisible(target);
    };
    styleLabel(inputLabel, "INPUT  /  LUFS");
    styleLabel(outputLabel, "OUTPUT  /  LUFS");
    styleLabel(reductionLabel, "GAIN REDUCTION");
    styleLabel(statusLabel, "READY");

    addAndMakeVisible(meter);
    addAndMakeVisible(spectrum);
    addAndMakeVisible(eqGraph);
    eqGraph.onBandSelected = [this](int band)
    {
        selectedEQBand = band;
        updateEQControls();
    };

    eqModeSelector.addItem("AUTO", 1);
    eqModeSelector.addItem("MANUAL", 2);
    eqModeSelector.setTooltip("Automatic uses the 10 second learn result. Manual keeps your node positions.");
    eqModeAttachment = std::make_unique<ComboAttachment>(processor.parameters, "eqMode", eqModeSelector);
    addAndMakeVisible(eqModeSelector);

    for (auto* button : { &eqPhaseButton, &eqEnabledButton, &eqDynamicButton,
                          &stereoEnabledButton, &compressorEnabledButton })
    {
        styleToggle(*button);
        addAndMakeVisible(*button);
    }
    eqPhaseAttachment = std::make_unique<ButtonAttachment>(
        processor.parameters, "eqLinearPhase", eqPhaseButton);
    eqEnabledAttachment = std::make_unique<ButtonAttachment>(
        processor.parameters, "eqEnabled", eqEnabledButton);
    stereoEnabledAttachment = std::make_unique<ButtonAttachment>(
        processor.parameters, "stereoEnabled", stereoEnabledButton);
    compressorEnabledAttachment = std::make_unique<ButtonAttachment>(
        processor.parameters, "compressorEnabled", compressorEnabledButton);
    eqDynamicButton.setToggleState(false, juce::dontSendNotification);
    eqDynamicButton.onClick = [this]
    {
        processor.setEQDynamic(selectedEQBand, eqDynamicButton.getToggleState());
    };
    eqAutoButton.setColour(juce::TextButton::buttonColourId, surfaceRaised);
    eqAutoButton.setColour(juce::TextButton::textColourOffId, text);
    eqAutoButton.onClick = [this] { processor.resetEQToAuto(); updateEQControls(); };
    addAndMakeVisible(eqAutoButton);

    styleSlider(eqFrequencySlider, " Hz");
    styleSlider(eqGainSlider, " dB");
    styleSlider(eqQSlider, " Q");
    styleControlLabel(eqControlLabels[0], "FREQUENCY");
    styleControlLabel(eqControlLabels[1], "GAIN");
    styleControlLabel(eqControlLabels[2], "Q");
    eqFrequencySlider.setRange(20.0, 20000.0, 0.01);
    eqFrequencySlider.setSkewFactorFromMidPoint(800.0);
    eqGainSlider.setRange(-18.0, 18.0, 0.01);
    eqQSlider.setRange(0.1, 12.0, 0.01);
    eqFrequencySlider.onValueChange = [this]
    {
        if (!updatingEQControls) processor.setEQFrequency(selectedEQBand,
                                                           static_cast<float>(eqFrequencySlider.getValue()));
        eqGraph.repaint();
    };
    eqGainSlider.onValueChange = [this]
    {
        if (!updatingEQControls) processor.setEQGain(selectedEQBand,
                                                      static_cast<float>(eqGainSlider.getValue()));
        eqGraph.repaint();
    };
    eqQSlider.onValueChange = [this]
    {
        if (!updatingEQControls) processor.setEQQ(selectedEQBand,
                                                   static_cast<float>(eqQSlider.getValue()));
        eqGraph.repaint();
    };
    addAndMakeVisible(eqFrequencySlider);
    addAndMakeVisible(eqGainSlider);
    addAndMakeVisible(eqQSlider);
    addAndMakeVisible(eqBandLabel);
    eqBandLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    eqBandLabel.setColour(juce::Label::textColourId, accent);

    styleSlider(targetSlider, " LUFS");
    styleSlider(volumeSlider, " dB");
    targetSlider.setRange(-24.0, -8.0, 0.1);
    volumeSlider.setRange(-24.0, 24.0, 0.01);
    styleControlLabel(loudnessControlLabels[0], "TARGET LUFS");
    styleControlLabel(loudnessControlLabels[1], "CLEAN VOLUME");
    targetAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "targetLufs", targetSlider);
    volumeAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "volume", volumeSlider);
    addAndMakeVisible(targetSlider);
    addAndMakeVisible(volumeSlider);

    styleSlider(saturationMixSlider, " %");
    styleControlLabel(saturationControlLabel, "MIX");
    saturationMixSlider.setRange(0.0, 100.0, 0.1);
    saturationMixAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "saturationMix", saturationMixSlider);
    saturationPresetSelector.addItem("WARM", 1);
    saturationPresetSelector.addItem("COLD / AIRY", 2);
    saturationPresetSelector.addItem("DISTORTION", 3);
    saturationPresetSelector.addItem("TUBE", 4);
    saturationPresetAttachment = std::make_unique<ComboAttachment>(
        processor.parameters, "saturationPreset", saturationPresetSelector);
    addAndMakeVisible(saturationMixSlider);
    addAndMakeVisible(saturationPresetSelector);

    for (int index = 0; index < 3; ++index)
    {
        styleSlider(crossoverSliders[static_cast<size_t>(index)], " Hz");
        styleControlLabel(crossoverLabels[static_cast<size_t>(index)],
                          "XOVER " + juce::String(index + 1));
        crossoverSliders[static_cast<size_t>(index)].setRange(40.0, 18000.0, 0.1);
        crossoverSliders[static_cast<size_t>(index)].setSkewFactorFromMidPoint(800.0);
        crossoverAttachments[static_cast<size_t>(index)] = std::make_unique<SliderAttachment>(
            processor.parameters, "stereoXover" + juce::String(index + 1),
            crossoverSliders[static_cast<size_t>(index)]);
    }
    for (int index = 0; index < 4; ++index)
    {
        styleSlider(widthSliders[static_cast<size_t>(index)], " %");
        styleControlLabel(widthLabels[static_cast<size_t>(index)],
                          "BAND " + juce::String(index + 1) + " WIDTH");
        widthSliders[static_cast<size_t>(index)].setRange(0.0, 200.0, 0.1);
        widthAttachments[static_cast<size_t>(index)] = std::make_unique<SliderAttachment>(
            processor.parameters, "stereoWidth" + juce::String(index + 1),
            widthSliders[static_cast<size_t>(index)]);
        stereoPresetSelectors[static_cast<size_t>(index)].addItem("NEUTRAL", 1);
        stereoPresetSelectors[static_cast<size_t>(index)].addItem("WIDE / AIRY", 2);
        stereoPresetSelectors[static_cast<size_t>(index)].addItem("NARROW / FOCUS", 3);
        stereoPresetSelectors[static_cast<size_t>(index)].addItem("WIDE / TUBE", 4);
        stereoPresetAttachments[static_cast<size_t>(index)] = std::make_unique<ComboAttachment>(
            processor.parameters, "stereoPreset" + juce::String(index + 1),
            stereoPresetSelectors[static_cast<size_t>(index)]);
        addAndMakeVisible(crossoverSliders[static_cast<size_t>(index)]);
        addAndMakeVisible(widthSliders[static_cast<size_t>(index)]);
        addAndMakeVisible(stereoPresetSelectors[static_cast<size_t>(index)]);
    }

    const std::array<std::pair<juce::Slider*, const char*>, 6> compControls {
        std::pair { &compAttackSlider, " ms" }, std::pair { &compReleaseSlider, " ms" },
        std::pair { &compRatioSlider, " :1" }, std::pair { &compInputSlider, " dB" },
        std::pair { &compOutputSlider, " dB" }, std::pair { &compThresholdSlider, " dB" }
    };
    for (auto [slider, suffix] : compControls)
    {
        styleSlider(*slider, suffix);
        addAndMakeVisible(*slider);
    }
    const std::array<const char*, 6> compressorLabels {
        "ATTACK", "RELEASE", "RATIO", "INPUT GAIN", "MAKEUP GAIN", "THRESHOLD"
    };
    for (size_t index = 0; index < compressorLabels.size(); ++index)
        styleControlLabel(compressorControlLabels[index], compressorLabels[index]);
    compAttackSlider.setRange(1.0, 200.0, 0.1);
    compReleaseSlider.setRange(20.0, 2000.0, 0.1);
    compRatioSlider.setRange(1.0, 20.0, 0.01);
    compInputSlider.setRange(-24.0, 24.0, 0.01);
    compOutputSlider.setRange(-24.0, 24.0, 0.01);
    compThresholdSlider.setRange(-48.0, 0.0, 0.1);
    compAttackAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "compAttack", compAttackSlider);
    compReleaseAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "compRelease", compReleaseSlider);
    compRatioAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "compRatio", compRatioSlider);
    compInputAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "compInput", compInputSlider);
    compOutputAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "compOutput", compOutputSlider);
    compThresholdAttachment = std::make_unique<SliderAttachment>(
        processor.parameters, "compThreshold", compThresholdSlider);

    setPage(Page::eq);
    updateEQControls();
    startTimerHz(20);
}

void NorthstarMasteringAudioProcessorEditor::styleSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 24);
    slider.setTextValueSuffix(suffix);
    if (suffix.contains("Hz") || suffix.contains("%") || suffix.contains("ms"))
        slider.setNumDecimalPlacesToDisplay(0);
    else if (suffix.contains(":1"))
        slider.setNumDecimalPlacesToDisplay(1);
    else if (suffix.contains(" Q"))
        slider.setNumDecimalPlacesToDisplay(2);
    else
        slider.setNumDecimalPlacesToDisplay(1);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, border);
    slider.setColour(juce::Slider::thumbColourId, text);
    slider.setColour(juce::Slider::textBoxTextColourId, text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, surfaceRaised);
    slider.setColour(juce::Slider::textBoxOutlineColourId, border);
}

void NorthstarMasteringAudioProcessorEditor::styleControlLabel(
    juce::Label& label, const juce::String& value)
{
    label.setText(value, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, muted);
    label.setJustificationType(juce::Justification::centred);
    label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label);
}

void NorthstarMasteringAudioProcessorEditor::styleButton(juce::TextButton& button, bool toggle)
{
    button.setClickingTogglesState(toggle);
    button.setColour(juce::TextButton::buttonColourId, surfaceRaised);
    button.setColour(juce::TextButton::buttonOnColourId, accent);
    button.setColour(juce::TextButton::textColourOffId, text);
    button.setColour(juce::TextButton::textColourOnId, background);
}

void NorthstarMasteringAudioProcessorEditor::styleToggle(juce::ToggleButton& button)
{
    button.setClickingTogglesState(true);
    button.setColour(juce::ToggleButton::textColourId, text);
    button.setColour(juce::ToggleButton::tickColourId, accent);
    button.setColour(juce::ToggleButton::tickDisabledColourId, border);
}

void NorthstarMasteringAudioProcessorEditor::setPage(Page nextPage)
{
    page = nextPage;
    const auto setTab = [](juce::TextButton& button, bool active)
    {
        button.setColour(juce::TextButton::buttonColourId, active ? accent : surfaceRaised);
        button.setColour(juce::TextButton::textColourOffId, active ? background : text);
    };
    setTab(eqTab, page == Page::eq);
    setTab(loudnessTab, page == Page::loudness);
    setTab(saturationTab, page == Page::saturation);
    setTab(stereoTab, page == Page::stereo);
    setTab(compressorTab, page == Page::compressor);

    const auto eqVisible = page == Page::eq;
    const auto loudnessVisible = page == Page::loudness;
    const auto satVisible = page == Page::saturation;
    const auto stereoVisible = page == Page::stereo;
    const auto compVisible = page == Page::compressor;
    const auto setComponentsVisible = [](bool visible,
                                         std::initializer_list<juce::Component*> components)
    {
        for (auto* component : components)
            component->setVisible(visible);
    };
    setComponentsVisible(eqVisible, { &eqGraph, &eqModeSelector, &eqPhaseButton,
                                      &eqEnabledButton, &eqDynamicButton, &eqAutoButton,
                                      &eqFrequencySlider, &eqGainSlider, &eqQSlider,
                                      &eqBandLabel });
    for (auto& label : eqControlLabels)
        label.setVisible(eqVisible);
    setComponentsVisible(loudnessVisible, { &targetSlider, &volumeSlider, &learnButton });
    for (auto& label : loudnessControlLabels)
        label.setVisible(loudnessVisible);
    saturationMixSlider.setVisible(satVisible);
    saturationPresetSelector.setVisible(satVisible);
    saturationControlLabel.setVisible(satVisible);
    stereoEnabledButton.setVisible(stereoVisible);
    setComponentsVisible(stereoVisible, { &crossoverSliders[0], &crossoverSliders[1],
                                          &crossoverSliders[2], &widthSliders[0],
                                          &widthSliders[1], &widthSliders[2],
                                          &widthSliders[3], &stereoPresetSelectors[0],
                                          &stereoPresetSelectors[1], &stereoPresetSelectors[2],
                                          &stereoPresetSelectors[3] });
    for (auto& label : crossoverLabels)
        label.setVisible(stereoVisible);
    for (auto& label : widthLabels)
        label.setVisible(stereoVisible);
    compressorEnabledButton.setVisible(compVisible);
    setComponentsVisible(compVisible, { &compAttackSlider, &compReleaseSlider,
                                        &compRatioSlider, &compInputSlider,
                                        &compOutputSlider, &compThresholdSlider });
    for (auto& label : compressorControlLabels)
        label.setVisible(compVisible);
    resized();
    repaint();
}

void NorthstarMasteringAudioProcessorEditor::updateEQControls()
{
    updatingEQControls = true;
    eqFrequencySlider.setValue(processor.getEQFrequency(selectedEQBand), juce::dontSendNotification);
    eqGainSlider.setValue(processor.getEQGain(selectedEQBand), juce::dontSendNotification);
    eqQSlider.setValue(processor.getEQQ(selectedEQBand), juce::dontSendNotification);
    eqDynamicButton.setToggleState(processor.getEQDynamic(selectedEQBand), juce::dontSendNotification);
    eqBandLabel.setText("NODE " + juce::String(selectedEQBand + 1) + " / 15",
                        juce::dontSendNotification);
    updatingEQControls = false;
    eqGraph.setSelectedBand(selectedEQBand);
}

void NorthstarMasteringAudioProcessorEditor::updateLabels()
{
    inputLabel.setText("INPUT  " + juce::String(processor.getLoudnessEstimate(), 1) + " LUFS",
                       juce::dontSendNotification);
    outputLabel.setText("OUTPUT  " + juce::String(processor.getOutputLoudness(), 1) + " LUFS",
                        juce::dontSendNotification);
    reductionLabel.setText("GAIN REDUCTION  -" + juce::String(processor.getGainReduction(), 1) + " dB",
                           juce::dontSendNotification);
    statusLabel.setText(processor.isAnalysisRunning()
                            ? "LEARNING  " + juce::String(static_cast<int>(
                                  processor.getAnalysisProgress() * 100.0f)) + "%"
                            : (processor.hasAnalysis() ? "AUTO PROFILE READY" : "READY"),
                        juce::dontSendNotification);
    analyzeButton.setButtonText(processor.isAnalysisRunning()
                                    ? "ANALYZING  " + juce::String(static_cast<int>(
                                          processor.getAnalysisProgress() * 100.0f)) + "%"
                                    : "ANALYZE 10 SEC");
}

void NorthstarMasteringAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(surface);
    g.fillRoundedRectangle(18.0f, 85.0f, static_cast<float>(getWidth() - 36),
                           static_cast<float>(getHeight() - 103), 12.0f);
    g.setColour(border);
    g.drawRoundedRectangle(18.0f, 85.0f, static_cast<float>(getWidth() - 36),
                           static_cast<float>(getHeight() - 103), 12.0f, 1.0f);
    g.setColour(muted);
    g.setFont(10.0f);
    g.drawText("REAL-TIME MASTERING / HOST AUTOMATION READY", 32, 98, 360, 18,
               juce::Justification::left);
    const auto footerTop = getHeight() - 142;
    g.setColour(border);
    g.drawHorizontalLine(footerTop - 10, 32.0f, static_cast<float>(getWidth() - 32));
    g.setColour(muted);
    g.drawText("OUTPUT LEVEL", 235, footerTop - 7, 170, 16, juce::Justification::left);
    g.setFont(9.0f);
    g.drawText("-48 dB", 235, footerTop + 29, 48, 14, juce::Justification::left);
    g.drawText("-24", 235 + (getWidth() - 265) * 0.50f - 18, footerTop + 29,
               36, 14, juce::Justification::centred);
    g.drawText("-12", 235 + (getWidth() - 265) * 0.75f - 18, footerTop + 29,
               36, 14, juce::Justification::centred);
    g.drawText("0 dB", getWidth() - 72, footerTop + 29, 40, 14,
               juce::Justification::right);
    g.drawText("LIVE INPUT SPECTRUM", 32, footerTop + 62, 220, 16,
               juce::Justification::left);

    if (page == Page::eq)
    {
        g.setColour(muted);
        g.drawText("Drag any node. Double click a node to reset its gain.", 42, 125,
                   getWidth() - 84, 18, juce::Justification::left);
        g.drawText("20 Hz", 46, 455, 52, 18, juce::Justification::left);
        g.drawText("100 Hz", 205, 455, 52, 18, juce::Justification::centred);
        g.drawText("1 kHz", 400, 455, 52, 18, juce::Justification::centred);
        g.drawText("10 kHz", 665, 455, 60, 18, juce::Justification::centred);
        g.drawText("20 kHz", getWidth() - 100, 455, 60, 18, juce::Justification::right);
    }
    else if (page == Page::loudness)
    {
        g.setColour(text);
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText("LOUDNESS ALIGNMENT", 48, 132, 500, 34, juce::Justification::left);
        g.setColour(muted);
        g.setFont(12.0f);
        g.drawText("Match a LUFS target, then trim the result with a transparent gain control.", 50, 172,
                   getWidth() - 100, 22, juce::Justification::left);
        g.drawText("The volume control is clean gain only — no saturation or clipping stage is added.", 50, 198,
                   getWidth() - 100, 22, juce::Justification::left);
    }
    else if (page == Page::saturation)
    {
        g.setColour(text);
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText("SATURATION", 48, 132, 500, 34, juce::Justification::left);
        g.setColour(muted);
        g.setFont(12.0f);
        g.drawText("Blend harmonics from zero to full wet. Presets change the drive character.", 50, 172,
                   getWidth() - 100, 22, juce::Justification::left);
    }
    else if (page == Page::stereo)
    {
        g.setColour(text);
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText("FOUR-BAND STEREO IMAGER", 48, 132, 600, 34, juce::Justification::left);
        g.setColour(muted);
        g.setFont(12.0f);
        g.drawText("Set the three crossover points and width/preset independently in each band.", 50, 172,
                   getWidth() - 100, 22, juce::Justification::left);
    }
    else
    {
        g.setColour(text);
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText("OPTO COMPRESSOR", 48, 132, 600, 34, juce::Justification::left);
        g.setColour(muted);
        g.setFont(12.0f);
        g.drawText("Manual attack, release, ratio, input, output and threshold with live gain-reduction metering.", 50, 172,
                   getWidth() - 100, 22, juce::Justification::left);
    }
}

void NorthstarMasteringAudioProcessorEditor::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();
    brandLabel.setBounds(26, 16, 180, 28);
    subtitleLabel.setBounds(28, 44, 300, 18);
    analyzeButton.setBounds(w - 226, 18, 164, 32);
    bypassButton.setBounds(w - 54, 18, 42, 32);
    statusLabel.setBounds(w - 410, 47, 280, 18);

    const int tabY = 63;
    const int tabW = juce::jmax(120, (w - 52) / 5);
    juce::TextButton* tabs[] { &eqTab, &loudnessTab, &saturationTab, &stereoTab, &compressorTab };
    for (int i = 0; i < 5; ++i)
        tabs[i]->setBounds(26 + i * tabW, tabY, tabW - 5, 28);

    const auto footerTop = h - 142;
    inputLabel.setBounds(32, footerTop + 1, 190, 20);
    outputLabel.setBounds(32, footerTop + 23, 190, 20);
    reductionLabel.setBounds(32, footerTop + 45, 220, 20);
    meter.setBounds(235, footerTop + 7, w - 265, 22);
    spectrum.setBounds(32, footerTop + 80, w - 64, 42);

    // Bounds are assigned for every page on every resize. Visibility is managed
    // by setPage(); doing this here prevents controls from keeping a 0x0 bounds
    // rectangle after switching to another tab.
    const auto eqGraphWidth = juce::jmax(300, w - 330);
    eqGraph.setBounds(42, 148, eqGraphWidth, 298);
    eqModeSelector.setBounds(w - 268, 148, 224, 30);
    eqEnabledButton.setBounds(w - 268, 190, 108, 28);
    eqPhaseButton.setBounds(w - 152, 190, 108, 28);
    eqBandLabel.setBounds(w - 268, 232, 224, 22);
    eqControlLabels[0].setBounds(w - 282, 252, 120, 18);
    eqControlLabels[1].setBounds(w - 150, 252, 120, 18);
    eqFrequencySlider.setBounds(w - 282, 270, 120, 112);
    eqGainSlider.setBounds(w - 150, 270, 120, 112);
    eqControlLabels[2].setBounds(w - 282, 392, 120, 18);
    eqQSlider.setBounds(w - 282, 410, 120, 112);
    eqDynamicButton.setBounds(w - 150, 394, 120, 28);
    eqAutoButton.setBounds(w - 150, 432, 120, 28);

    loudnessControlLabels[0].setBounds(120, 214, 200, 20);
    loudnessControlLabels[1].setBounds(390, 214, 200, 20);
    targetSlider.setBounds(120, 234, 200, 178);
    volumeSlider.setBounds(390, 234, 200, 178);
    learnButton.setBounds(w - 250, 245, 190, 40);

    saturationControlLabel.setBounds(w / 2 - 115, 202, 230, 20);
    saturationMixSlider.setBounds(w / 2 - 115, 222, 230, 220);
    saturationPresetSelector.setBounds(w / 2 - 130, 470, 260, 34);

    stereoEnabledButton.setBounds(48, 205, 135, 30);
    const auto stereoMargin = 70;
    const auto stereoGap = 14;
    const auto crossoverWidth = juce::jmax(106, (w - 2 * stereoMargin - 2 * stereoGap) / 3);
    const auto crossoverY = 250;
    const auto crossoverHeight = juce::jmax(96, juce::jmin(142, h - 570));
    for (int i = 0; i < 3; ++i)
    {
        const auto x = stereoMargin + i * (crossoverWidth + stereoGap);
        crossoverLabels[static_cast<size_t>(i)].setBounds(x, crossoverY - 20,
                                                            crossoverWidth, 18);
        crossoverSliders[static_cast<size_t>(i)].setBounds(x, crossoverY,
                                                             crossoverWidth, crossoverHeight);
    }
    const auto widthY = crossoverY + crossoverHeight + 32;
    const auto widthHeight = juce::jmax(76, juce::jmin(112, h - widthY - 176));
    const auto widthMargin = 52;
    const auto widthGap = 12;
    const auto width = juce::jmax(100, (w - 2 * widthMargin - 3 * widthGap) / 4);
    for (int i = 0; i < 4; ++i)
    {
        const auto x = widthMargin + i * (width + widthGap);
        widthLabels[static_cast<size_t>(i)].setBounds(x, widthY - 20, width, 18);
        widthSliders[static_cast<size_t>(i)].setBounds(x, widthY, width, widthHeight);
        stereoPresetSelectors[static_cast<size_t>(i)].setBounds(x, widthY + widthHeight + 8,
                                                                  width, 28);
    }

    compressorEnabledButton.setBounds(48, 205, 130, 30);
    const auto compMargin = 40;
    const auto compGap = 10;
    const auto compWidth = juce::jmax(100, (w - 2 * compMargin - 5 * compGap) / 6);
    const auto compHeight = juce::jmax(126, juce::jmin(176, h - 390));
    juce::Slider* compSliders[] {
        &compAttackSlider, &compReleaseSlider, &compRatioSlider,
        &compInputSlider, &compOutputSlider, &compThresholdSlider
    };
    for (int i = 0; i < 6; ++i)
    {
        const auto x = compMargin + i * (compWidth + compGap);
        compressorControlLabels[static_cast<size_t>(i)].setBounds(x, 238, compWidth, 18);
        compSliders[i]->setBounds(x, 256, compWidth, compHeight);
    }
}

void NorthstarMasteringAudioProcessorEditor::timerCallback()
{
    std::array<float, 64> bins {};
    processor.copySpectrum(bins);
    spectrum.setValues(bins);
    eqGraph.setSpectrum(bins);
    meter.setLevels(processor.getPeakDb(), processor.getOutputLoudness(),
                    processor.getGainReduction());
    updateLabels();
    if (page == Page::eq)
    {
        eqGraph.repaint();
        updateEQControls();
    }
}

void NorthstarMasteringAudioProcessorEditor::Meter::setLevels(
    float peak, float loudness, float gainReduction)
{
    peakLevel = normalizedMeter(peak);
    loudnessLevel = normalizedMeter(loudness);
    reductionLevel = juce::jlimit(0.0f, 1.0f, gainReduction / 12.0f);
    repaint();
}

void NorthstarMasteringAudioProcessorEditor::Meter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(border);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(bounds.withWidth(bounds.getWidth() * loudnessLevel).reduced(1.0f), 3.0f);
    g.setColour(warm);
    g.fillRect(bounds.getX() + bounds.getWidth() * peakLevel - 2.0f, bounds.getY(),
               2.0f, bounds.getHeight());
    g.setColour(danger.withAlpha(reductionLevel));
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth() * reductionLevel, bounds.getHeight());
}

void NorthstarMasteringAudioProcessorEditor::Spectrum::setValues(
    const std::array<float, 64>& values)
{
    bins = values;
    repaint();
}

void NorthstarMasteringAudioProcessorEditor::Spectrum::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(surfaceRaised);
    g.fillRoundedRectangle(bounds, 4.0f);
    juce::Path path;
    const auto step = bounds.getWidth() / static_cast<float>(bins.size() - 1);
    for (size_t index = 0; index < bins.size(); ++index)
    {
        const auto x = bounds.getX() + step * static_cast<float>(index);
        const auto y = bounds.getBottom() - bins[index] * bounds.getHeight();
        if (index == 0) path.startNewSubPath(x, y);
        else path.lineTo(x, y);
    }
    g.setColour(accent);
    g.strokePath(path, juce::PathStrokeType(1.3f));
}

NorthstarMasteringAudioProcessorEditor::EQGraph::EQGraph(
    NorthstarMasteringAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::setSpectrum(
    const std::array<float, 64>& values)
{
    spectrumBins = values;
    repaint();
}

float NorthstarMasteringAudioProcessorEditor::EQGraph::xForFrequency(float frequency) const
{
    const auto normalized = std::log10(juce::jlimit(20.0f, 20000.0f, frequency) / 20.0f)
        / std::log10(1000.0f);
    return normalized * static_cast<float>(getWidth());
}

float NorthstarMasteringAudioProcessorEditor::EQGraph::yForGain(float gain) const
{
    return static_cast<float>(getHeight()) * (0.5f - gain / 40.0f);
}

float NorthstarMasteringAudioProcessorEditor::EQGraph::frequencyForX(float x) const
{
    return 20.0f * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f,
                                                    x / static_cast<float>(getWidth())));
}

float NorthstarMasteringAudioProcessorEditor::EQGraph::gainForY(float y) const
{
    return juce::jlimit(-18.0f, 18.0f,
                        (0.5f - y / static_cast<float>(getHeight())) * 40.0f);
}

int NorthstarMasteringAudioProcessorEditor::EQGraph::bandAtPosition(
    juce::Point<float> position) const
{
    int nearest = 0;
    auto distance = std::numeric_limits<float>::max();
    for (int band = 0; band < NorthstarMasteringAudioProcessor::eqBandCount; ++band)
    {
        const auto point = juce::Point<float>(
            xForFrequency(processor.getEQFrequency(band)), yForGain(processor.getEQGain(band)));
        const auto nextDistance = point.getDistanceFrom(position);
        if (nextDistance < distance)
        {
            nearest = band;
            distance = nextDistance;
        }
    }
    return distance < 28.0f ? nearest : -1;
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::setSelectedBand(int band)
{
    selectedBand = juce::jlimit(0, NorthstarMasteringAudioProcessor::eqBandCount - 1, band);
    repaint();
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::updateBandFromPosition(
    juce::Point<float> position)
{
    processor.setEQFrequency(selectedBand, frequencyForX(position.x));
    processor.setEQGain(selectedBand, gainForY(position.y));
    repaint();
    if (onBandSelected != nullptr)
        onBandSelected(selectedBand);
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::mouseDown(const juce::MouseEvent& event)
{
    const auto hit = bandAtPosition(event.position);
    if (hit >= 0)
    {
        selectedBand = hit;
        dragging = true;
        if (onBandSelected != nullptr)
            onBandSelected(selectedBand);
    }
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging)
        updateBandFromPosition(event.position);
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
}

void NorthstarMasteringAudioProcessorEditor::EQGraph::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d151c));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(border);
    for (int line = 0; line <= 4; ++line)
    {
        const auto y = bounds.getY() + bounds.getHeight() * line / 4.0f;
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }
    for (int line = 0; line <= 5; ++line)
    {
        const auto x = bounds.getX() + bounds.getWidth() * line / 5.0f;
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }
    g.setColour(accent.withAlpha(0.20f));
    g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());

    // Live input spectrum, drawn behind the EQ response and nodes. The
    // logarithmic x mapping matches the frequency axis used by the controls.
    juce::Path spectrumShape;
    spectrumShape.startNewSubPath(0.0f, static_cast<float>(getHeight()));
    for (size_t index = 0; index < spectrumBins.size(); ++index)
    {
        const auto normalized = static_cast<float>(index)
            / static_cast<float>(spectrumBins.size() - 1);
        const auto x = normalized * static_cast<float>(getWidth());
        const auto level = juce::jlimit(0.0f, 1.0f, spectrumBins[index]);
        const auto y = static_cast<float>(getHeight()) * (0.96f - level * 0.74f);
        spectrumShape.lineTo(x, y);
    }
    spectrumShape.lineTo(static_cast<float>(getWidth()), static_cast<float>(getHeight()));
    spectrumShape.closeSubPath();
    g.setColour(juce::Colour(0xff3b7b8c).withAlpha(0.18f));
    g.fillPath(spectrumShape);

    juce::Path spectrumLine;
    for (size_t index = 0; index < spectrumBins.size(); ++index)
    {
        const auto normalized = static_cast<float>(index)
            / static_cast<float>(spectrumBins.size() - 1);
        const auto x = normalized * static_cast<float>(getWidth());
        const auto level = juce::jlimit(0.0f, 1.0f, spectrumBins[index]);
        const auto y = static_cast<float>(getHeight()) * (0.96f - level * 0.74f);
        if (index == 0) spectrumLine.startNewSubPath(x, y);
        else spectrumLine.lineTo(x, y);
    }
    g.setColour(juce::Colour(0xff6bb1c2).withAlpha(0.62f));
    g.strokePath(spectrumLine, juce::PathStrokeType(1.0f));

    // Draw the actual musical shape of the EQ: each node contributes a smooth
    // bell curve in log-frequency space. Connecting node centres directly
    // produces misleading straight ramps and does not resemble a peak filter.
    juce::Path curve;
    constexpr int curveSamples = 320;
    for (int sample = 0; sample < curveSamples; ++sample)
    {
        const auto normalized = static_cast<float>(sample) / static_cast<float>(curveSamples - 1);
        const auto frequency = 20.0f * std::pow(1000.0f, normalized);
        auto response = 0.0f;
        for (int band = 0; band < NorthstarMasteringAudioProcessor::eqBandCount; ++band)
        {
            const auto centre = juce::jmax(20.0f, processor.getEQFrequency(band));
            const auto q = juce::jmax(0.1f, processor.getEQQ(band));
            const auto octaveDistance = std::log2(frequency / centre);
            const auto width = juce::jmax(0.045f, 0.50f / q);
            response += processor.getEQGain(band)
                * std::exp(-0.5f * octaveDistance * octaveDistance / (width * width));
        }
        response = juce::jlimit(-18.0f, 18.0f, response);
        const auto x = normalized * static_cast<float>(getWidth());
        const auto y = yForGain(response);
        if (sample == 0) curve.startNewSubPath(x, y);
        else curve.lineTo(x, y);
    }
    g.setColour(accent.withAlpha(0.7f));
    g.strokePath(curve, juce::PathStrokeType(1.2f));
    for (int band = 0; band < NorthstarMasteringAudioProcessor::eqBandCount; ++band)
    {
        const auto x = xForFrequency(processor.getEQFrequency(band));
        const auto y = yForGain(processor.getEQGain(band));
        const auto selected = band == selectedBand;
        g.setColour(selected ? text : (processor.getEQDynamic(band) ? warm : accent));
        g.fillEllipse(x - (selected ? 6.0f : 4.0f), y - (selected ? 6.0f : 4.0f),
                      selected ? 12.0f : 8.0f, selected ? 12.0f : 8.0f);
        if (processor.getEQDynamic(band))
        {
            g.setColour(warm);
            g.drawEllipse(x - 8.0f, y - 8.0f, 16.0f, 16.0f, 1.0f);
        }
    }
}