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
                       ), myValueTreeState(*this, nullptr, Identifier("myParameters"), {
         std::make_unique<AudioParameterFloat>("cutoff", "CUTOFF", NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 1000.0f),
         std::make_unique<AudioParameterFloat>("resonance", "RES", 0.707f, 10.0f, 2.66f),
         std::make_unique<AudioParameterFloat>(ParameterID{ "shift",  1 }, "Shift", -1000.f, 1000.f, 0.f),
         std::make_unique<AudioParameterBool>("bypass", "BYPASS", false)
         
     }),
        frequencyShifter(*myValueTreeState.getRawParameterValue("shift"))
#endif
{
    myCutoffptr = dynamic_cast<AudioParameterFloat*>(myValueTreeState.getParameter("cutoff"));
    myResonanceptr = dynamic_cast<AudioParameterFloat*>(myValueTreeState.getParameter("resonance"));
    myBypassptr = dynamic_cast<AudioParameterBool*>(myValueTreeState.getParameter("bypass"));

    
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
    
    myFilter.prepare(mySpec);
    myFilter.reset();
    
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
    
    
    myFilter.setCutoffFrequency(myCutoffptr->get());
    myFilter.setResonance(myResonanceptr->get());
    
    if (! myBypassptr->get())
    {
        auto myBlock = dsp::AudioBlock<float>(buffer);
        
        // Replacing context -> puts the processed audio back into the audio stream
        auto myContext = dsp::ProcessContextReplacing<float>(myBlock);
        
        myFilter.process(myContext);
        frequencyShifter.process(myContext);
    }
    
    
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }
}

//==============================================================================
bool ModalShiftAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ModalShiftAudioProcessor::createEditor()
{
//    return new ModalShiftAudioProcessorEditor (*this);
    return new GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ModalShiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    auto state = myValueTreeState.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ModalShiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(myValueTreeState.state.getType()))
        {
            myValueTreeState.replaceState(ValueTree::fromXml(*xmlState));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModalShiftAudioProcessor();
}
