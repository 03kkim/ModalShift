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
                       ), apvts(*this, nullptr, "Parameters", param::createParameterLayout())
//        frequencyShifter(shiftAmt)
#endif
{
    // Initialize the shifters array
    for (size_t channel = 0; channel < 2; ++channel)
    {
        for (size_t harmonic = 0; harmonic < MAX_HARMONICS; ++harmonic)
        {
            shifters[channel][harmonic] = std::make_unique<xynth::FrequencyShifter>(shiftAmt[channel][harmonic]);
        }
    }
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
    filterBuffers.resize(MAX_HARMONICS);
    for (auto& buffer : filterBuffers)
    {
        buffer.setSize(mySpec.numChannels, mySpec.maximumBlockSize);
    }
    for (size_t channel = 0; channel < shifters.size(); ++channel)
    {
        for (size_t harmonic = 0; harmonic < shifters[channel].size(); ++harmonic)
        {
            shifters[channel][harmonic]->prepare(mySpec);
            shifters[channel][harmonic]->reset();
        }
    }
    for (auto& filterStack : filters) {
        for (auto i = 0; i < MAX_ORDER; i++)
        {
            auto& filter = filterStack[i];
            filter[0].prepare(mySpec);
            filter[1].prepare(mySpec);
    
            filter[0].reset();
            filter[1].reset();
        }
        
        
    }
    
//    frequencyShifter.prepare(mySpec);
//    frequencyShifter.reset();
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

    
    const auto rootNorm = params[rootPID]->getValue();
    const auto rootFreq = params[rootPID]->getNormalisableRange().convertFrom0to1(rootNorm);

    
    const auto resonanceNorm = params[resonancePID]->getValue();
    const auto resonance = params[resonancePID]->getNormalisableRange().convertFrom0to1(resonanceNorm);
    
    
    const auto numHarmonicsNorm = params[numHarmonicsPID]->getValue();
    const auto numHarmonics = params[numHarmonicsPID]->getNormalisableRange().convertFrom0to1(numHarmonicsNorm);
    
    
    const auto filterOrderNorm = params[filterOrderPID]->getValue();
    const auto filterOrder = params[filterOrderPID]->getNormalisableRange().convertFrom0to1(filterOrderNorm);

    
    midiProcessor.process(midiMessages, shiftAmt, rootFreq);
    
    // Copy the input buffer into each filter buffer
    for (auto& filterBuffer : filterBuffers) {
        // Resize if needed
        if (filterBuffer.getNumSamples() < buffer.getNumSamples() || 
            filterBuffer.getNumChannels() != buffer.getNumChannels())
        {
            filterBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), true, true, true);
        }
        filterBuffer.makeCopyOf(buffer);
    }
    
    bool antiAlias = true;
    
    for (int harmonic = 0; harmonic < numHarmonics; ++harmonic)
        {
            auto harmonicFreq = rootFreq * static_cast<float>(harmonic + 1);
//            if (harmonicFreq < mySpec.sampleRate / 2.0f) // Ensure frequency is below Nyquist
            {
                auto coefs = juce::dsp::IIR::Coefficients<float>::makeBandPass(mySpec.sampleRate, harmonicFreq, resonance);
                
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    juce::dsp::AudioBlock<float> block(filterBuffers[harmonic]);
                    auto channelBlock = block.getSingleChannelBlock(channel);
                    auto context = juce::dsp::ProcessContextReplacing<float>(channelBlock);

                    auto& filter = filters[channel][harmonic];
                    for (int stage = 0; stage < filterOrder; ++stage)
                    {
                        if (stage < filterOrder)
                        {
                            filter[stage].coefficients = coefs;
                            filter[stage].process(context);
                        }
                        else
                        {
                            filter[stage].reset();
                        }
                    }
//                    DBG("Harmonic: " + String(harmonic) + " | Value: " + String(shiftAmt[channel][harmonic]));
                    
                    shifters[channel][harmonic]->process(context, antiAlias);
                }
            }
//            else
            {
                // Clear buffer if frequency is out of range
//                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                    filterBuffers[harmonic].clear(channel, 0, buffer.getNumSamples());
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
