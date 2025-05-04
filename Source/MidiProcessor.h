/*
  ==============================================================================

    MidiProcessor.h
    Created: 3 Apr 2025 10:08:20pm
    Author:  q

  ==============================================================================
*/

# pragma once

class MidiProcessor
{
public:
    MidiProcessor() : lastNoteFreq(440.0f), lastRootFreq(440.0f), lastStretch(1.0f), lastNumHarmonics(8.f) {}
    
    float process(MidiBuffer& midiMessages, std::array<std::array<std::atomic<float>, MAX_HARMONICS>, 2>& shifts,
                 float rootFreq, float stretch, float numHarmonics)
    {
        // Check if root frequency or stretch has changed
        bool rootChanged = (rootFreq != lastRootFreq);
        bool stretchChanged = (stretch != lastStretch);
        bool numHarmonicsChanged = (numHarmonics != lastNumHarmonics);
        
        if (rootChanged) {
            lastRootFreq = rootFreq;
        }
        
        if (stretchChanged) {
            lastStretch = stretch;
        }
        
        if (numHarmonicsChanged) {
            lastNumHarmonics = numHarmonics;
        }
        
        // Process incoming MIDI messages
        MidiBuffer::Iterator it(midiMessages);
        MidiMessage currentMessage;
        int samplePos;
        bool noteChanged = false;
        
        while (it.getNextEvent(currentMessage, samplePos))
        {
            if (currentMessage.isNoteOn())
            {
                const int noteNum = currentMessage.getNoteNumber();
                lastNoteFreq = param::midiNoteToFrequency(noteNum);
                noteChanged = true;
                // Don't calculate shift here - will do it after the loop
            }
        }
        
        // Update shifts if either note, root, or stretch has changed
        if (noteChanged || rootChanged || stretchChanged) {
            updateShifts(shifts);
        }
        return lastNoteFreq;
    }
    
private:
    float lastNoteFreq;   // Remember the last played note frequency
    float lastRootFreq;   // Remember the last root frequency
    float lastStretch;    // Remember the last stretch value
    float lastNumHarmonics;
    
    void updateShifts(std::array<std::array<std::atomic<float>, MAX_HARMONICS>, 2>& shifts)
    {
        float shift = lastNoteFreq - lastRootFreq;
        
        for (auto channel = 0; channel < 2; ++channel) {
            for (size_t harmonic = 0; harmonic < MAX_HARMONICS; ++harmonic)
            {
                // Apply the stretch factor to each harmonic shift
                shifts[channel][harmonic] = shift * (harmonic + 1.f) + (lastRootFreq * (harmonic/lastNumHarmonics) * lastStretch);
//                shifts[channel][harmonic] = shift * (harmonic + 1.f);
            }
        }
        
        // Uncomment for debugging
        // DBG("Note: " + String(lastNoteFreq) + " | Root: " + String(lastRootFreq) + 
        //     " | Stretch: " + String(lastStretch) + " | Shift: " + String(shift));
    }
};
