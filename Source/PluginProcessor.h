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
    static const int MAX_ORDER = 8;
    static const int MAX_HARMONICS = 64;
    
    
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

    // Add a fixed-size vector to store filters
    // std::array<juce::dsp::StateVariableTPTFilter<float>, 32> filters;

//    std::array<juce::dsp::IIR::Filter<float>, 32> filters;
    
    // possibility of 8th-order band pass, 32 harmonics
    
    std::array<std::array<std::array<juce::dsp::IIR::Filter<float>, MAX_ORDER>, MAX_HARMONICS>, 2> filters;
    
    dsp::ProcessSpec mySpec;
    
    juce::dsp::Oscillator<float> osc;
    
    std::atomic<float> shiftAmt{0.0f};
    
    xynth::FrequencyShifter frequencyShifter;
    
//    NoteParameter* noteParam;
    MidiProcessor midiProcessor;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModalShiftAudioProcessor)
};
