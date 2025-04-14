/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
ModalShiftAudioProcessor::ModalShiftAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", param::createParameterLayout()),
        frequencyShifter(shiftAmt)
#endif
{
    for (auto i = 0; i < param::NumParams; ++i)
    {
        auto pID = static_cast<param::PID>(i);
        params.push_back(apvts.getParameter(param::toID(pID).getParamID()));
    }
}

ModalShiftAudioProcessor::~ModalShiftAudioProcessor()
{
}

//==============================================================================
const juce::String ModalShiftAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ModalShiftAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ModalShiftAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ModalShiftAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ModalShiftAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ModalShiftAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ModalShiftAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ModalShiftAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ModalShiftAudioProcessor::getProgramName (int index)
{
    return {};
}

void ModalShiftAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ModalShiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mySpec.sampleRate = sampleRate;
    mySpec.maximumBlockSize = samplesPerBlock;
    mySpec.numChannels = getTotalNumOutputChannels();
    
    for (auto& filter : filters) {
        filter.prepare(mySpec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.reset();
    }
    
    frequencyShifter.prepare(mySpec);
    frequencyShifter.reset();
}

void ModalShiftAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ModalShiftAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ModalShiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    juce::MidiBuffer processedMidi;
    
    const auto rootPID = static_cast<int>(param::PID::Root);
    const auto rootNorm = params[rootPID]->getValue();
    const auto rootFreq = params[rootPID]->getNormalisableRange().convertFrom0to1(rootNorm);

    const auto resonancePID = static_cast<int>(param::PID::Resonance);
    const auto resonanceNorm = params[resonancePID]->getValue();
    const auto resonance = params[resonancePID]->getNormalisableRange().convertFrom0to1(resonanceNorm);
    
    
    const auto numHarmonicsPID = static_cast<int>(param::PID::NumHarmonics);
    const auto numHarmonicsNorm = params[numHarmonicsPID]->getValue();
    const auto numHarmonics = params[numHarmonicsPID]->getNormalisableRange().convertFrom0to1(numHarmonicsNorm);

    midiProcessor.process(midiMessages, shiftAmt, rootFreq);


    

    // Create separate buffers for each filter
    std::vector<juce::AudioBuffer<float>> filterBuffers(filters.size(), juce::AudioBuffer<float>(buffer.getNumChannels(), buffer.getNumSamples()));

    // Copy the input buffer into each filter buffer
    for (auto& filterBuffer : filterBuffers) {
        filterBuffer.makeCopyOf(buffer);
    }

    // Process only the active filters based on NumHarmonics
    for (int i = 0; i < numHarmonics; ++i) {
        auto harmonicFreq = rootFreq * static_cast<float>(i + 1); // Harmonic frequency
//        DBG(String(harmonicFreq));
        if (harmonicFreq < mySpec.sampleRate / 2.0f) { // Ensure frequency is within Nyquist limit
            filters[i].setCutoffFrequency(harmonicFreq);
            filters[i].setResonance(resonance);

            juce::dsp::AudioBlock<float> block(filterBuffers[i]);
            auto context = juce::dsp::ProcessContextReplacing<float>(block);
            filters[i].process(context);
            
        } else {
            filterBuffers[i].clear(); // Clear buffer if frequency is out of range
        }
    }

    // Sum the processed buffers into the main buffer
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        auto* mainChannelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            float combinedSample = 0.0f;

            for (int i = 0; i < numHarmonics; ++i) {
                combinedSample += filterBuffers[i].getReadPointer(channel)[sample];
            }

//            mainChannelData[sample] = combinedSample / static_cast<float>(numHarmonics); // Normalize to avoid clipping
            mainChannelData[sample] = combinedSample;
        }
    }
}

//==============================================================================
bool ModalShiftAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ModalShiftAudioProcessor::createEditor()
{
    return new GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ModalShiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ModalShiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(ValueTree::fromXml(*xmlState));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModalShiftAudioProcessor();
}
