/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DSP/FrequencyShifter.h"
#include "Params.h"
#include "MidiProcessor.h"

//==============================================================================
/**
*/
class ModalShiftAudioProcessor  : public juce::AudioProcessor
{
public:
    
    AudioProcessorValueTreeState apvts;
//    AudioParameterFloat* myRootptr;
//    AudioParameterFloat* myResonanceptr;
//    AudioParameterChoice* mySlopeptr;
//    AudioParameterBool* myBypassptr;
    
//    AudioParameterFloat* myFreqShiftptr;
    std::vector<param::RAP*> params;
    
    
    
    //==============================================================================
    ModalShiftAudioProcessor();
    ~ModalShiftAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    
    // possibility of 4th-order band pass, 32 harmonics
    
    std::array<std::array<std::array<juce::dsp::IIR::Filter<float>, MAX_ORDER>, MAX_HARMONICS>, 2> filters;
    std::array<std::array<std::unique_ptr<xynth::FrequencyShifter>, MAX_HARMONICS>, 2> shifters;
    
    dsp::ProcessSpec mySpec;
    
    juce::dsp::Oscillator<float> osc;
    
    std::array<std::array<std::atomic<float>, MAX_HARMONICS>, 2> shiftAmt{0.0f};

    // Create separate buffers for each filter
    std::vector<juce::AudioBuffer<float>> filterBuffers;

    const int rootPID = static_cast<int>(param::PID::Root);
    const int resonancePID = static_cast<int>(param::PID::Resonance);
    const int numHarmonicsPID = static_cast<int>(param::PID::NumHarmonics);
    const int filterOrderPID = static_cast<int>(param::PID::FilterOrder);
    const int stretchPID = static_cast<int>(param::PID::Stretch);

    MidiProcessor midiProcessor;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalShiftAudioProcessor)
};
