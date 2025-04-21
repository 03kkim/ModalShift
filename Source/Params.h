/*
  ==============================================================================

    Params.h
    Created: 18 Mar 2025 6:11:37pm
    Author:  q

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <unordered_map>
#include <cmath>

static const int MAX_ORDER = 4;
static const int MAX_HARMONICS = 8;


namespace param
{
using APVTS = AudioProcessorValueTreeState;
using Layout = APVTS::ParameterLayout;
using RAP = RangedAudioParameter;
using UniqueRAP = std::unique_ptr<RAP>;
using UniqueRAPVector = std::vector<UniqueRAP>;
using APP = AudioProcessorParameter;
using APPCategory = APP::Category;
using APF = AudioParameterFloat;
using RangeF = NormalisableRange<float>;

// Parameter ID
enum class PID
{
    GainWet,
    Root,
    Resonance,
    Shift,
    NumHarmonics,
    FilterOrder,
    NumParams
};
static constexpr int NumParams = static_cast<int>(PID::NumParams);

enum class Unit
{
    Db,
    Hz,
    Unitless,
    Integer,
    NoteUnit,
    NumUnits
};

inline float midiNoteToFrequency(int midiNote) {
    return 440.f * std::pow(2.f, (midiNote - 69) / 12.f);
}

inline String toName(PID pID)
{
    switch (pID)
    {
        case PID::GainWet:
            return "Gain Wet";
        case PID::Root:
            return "Root";
        case PID::Resonance:
            return "Resonance";
        case PID::Shift:
            return "Shift";
        case PID::NumHarmonics:
            return "Num of Harmonics";
        case PID::FilterOrder:
            return "Filter Order";
        default:
            return "Unknown";
    }
}

inline ParameterID toID(const String& name)
{
    return ParameterID{name.toLowerCase().removeCharacters(" "), 1};
}

inline ParameterID toID(PID pID)
{
    return toID(toName(pID));
}

inline String toString(Unit unit)
{
    switch (unit)
    {
        case Unit::Db: return "dB";
        case Unit::Hz: return "Hz";
        case Unit::Unitless: return "";
        case Unit::Integer: return "";
        case Unit::NoteUnit: return "";
        default: return "Unknown";
    }
}

namespace range
    {
        inline RangeF biased(float start, float end, float bias) noexcept
        {
            // https://www.desmos.com/calculator/ps8q8gftcr
            const auto a = bias * .5f + .5f;
            const auto a2 = 2.f * a;
            const auto aM = 1.f - a;

            const auto r = end - start;
            const auto aR = r * a;
            if (bias != 0.f)
                return
            {
                    start, end,
                    [a2, aM, aR](float min, float, float x)
                    {
                        const auto denom = aM - x + a2 * x;
                        if (denom == 0.f)
                            return min;
                        return min + aR * x / denom;
                    },
                    [a2, aM, aR](float min, float, float x)
                    {
                        const auto denom = a2 * min + aR - a2 * x - min + x;
                        if (denom == 0.f)
                            return 0.f;
                        auto val = aM * (x - min) / denom;
                        return val > 1.f ? 1.f : val;
                    },
                    [](float min, float max, float x)
                    {
                        return x < min ? min : x > max ? max : x;
                    }
            };
            else return { start, end };
        }

        inline RangeF stepped(float start, float end, float steps = 1.f) noexcept
        {
            return { start, end, steps };
        }

        inline RangeF toggle() noexcept
        {
            return stepped(0.f, 1.f);
        }

        inline RangeF lin(float start, float end) noexcept
        {
            const auto range = end - start;

            return
            {
                start,
                end,
                [range](float min, float, float normalized)
                {
                    return min + normalized * range;
                },
                [inv = 1.f / range](float min, float, float denormalized)
                {
                    return (denormalized - min) * inv;
                },
                [](float min, float max, float x)
                {
                    return juce::jlimit(min, max, x);
                }
            };
        }

        inline RangeF withCentre(float start, float end, float centre) noexcept
        {
            const auto r = end - start;
            const auto v = (centre - start) / r;

            return biased(start, end, 2.f * v - 1.f);
        }
    }

using ValToStr = std::function<String(float, int)>;
using StrToVal = std::function<float(const String&)>;

// note that the 2nd param for these funcs is the max string length, but we do not actually use it in our implementations
namespace valToStr
{
inline ValToStr db()
{
    return [](float val, int)
        {
            return String(val, 2) + " dB";
        };
}

inline ValToStr hz()
{
    return [](float val, int)
    {
        if (val < 100.f)
            return String(val, 2) + " hz";
        else if (val < 1000.f)
            return String(val, 1) + " hz";
        else if (val >= 1000.f)
        {
            auto k = val / 1000.f;
            return String(k, 1) + " khz";
        }
        return String();
    };
}

inline ValToStr unitless()
{
    return [](float val, int)
        {
            return String(val, 2);
        };
}

inline ValToStr integer()
{
    return [](float val, int)
    {
        return String(static_cast<int>(val));
    };
}

inline ValToStr noteunit()
{
    return [](float val, int)
    {
        const std::vector<String> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
//        const int flooredVal = static_cast<int>(std::floor(val));
//        const int noteInOctave = flooredVal % 12;
//        const int octave = (flooredVal / 12) - 1; // MIDI note 0 corresponds to C-1
//
//        return noteNames[noteInOctave] + String(octave);
        double midiNote = 12 * std::log2(val / 440.0) + 69;
        int roundedMidiNote = std::round(midiNote);
        
        int octave = (roundedMidiNote - 12) / 12;
        int noteIndex = (roundedMidiNote - 12) % 12;
        
        if (noteIndex < 0) noteIndex += 12;
        
        return noteNames[noteIndex] + String(octave);
    };
}
}

namespace strToVal
{
inline StrToVal db()
{
    return [](const String& str)
    {
        return str.removeCharacters(toString(Unit::Db)).getFloatValue();
    };
}

inline StrToVal hz()
{
    return [](const String& str)
    {
        auto s = str.removeCharacters(toString(Unit::Hz));
        if (s.endsWith("k"))
        {
            s = s.dropLastCharacters(1);
            return s.getFloatValue() * 1000.f;
        }
        return s.getFloatValue();
    };
}

inline StrToVal unitless()
{
    return [](const String& str)
    {
        return str.getFloatValue();
    };
}

inline StrToVal integer()
{
    return [](const String& str)
    {
        return static_cast<float>(str.getIntValue());
    };
}

inline StrToVal noteunit()
{
    return [](const String& str)
    {
        static const std::unordered_map<String, int> noteMap = {
                {"C", 0}, {"C#", 1}, {"Db", 1}, {"D", 2}, {"D#", 3}, {"Eb", 3},
                {"E", 4}, {"F", 5}, {"F#", 6}, {"Gb", 6}, {"G", 7}, {"G#", 8},
                {"Ab", 8}, {"A", 9}, {"A#", 10}, {"Bb", 10}, {"B", 11}
            };
        String noteName = str.substring(0, str.length() - 1);
        auto octaveChar = str.getLastCharacter();
        int octave = octaveChar - '0';
        int midiNote = noteMap.at(noteName) + (octave + 1) * 12;
        
        return midiNoteToFrequency(midiNote);
    };
}
}


inline void createParam(UniqueRAPVector& vec, PID pID, RangeF range, float defaultVal, const Unit unit)
{
    ValToStr valToStr;
    StrToVal strToVal;
    
    const auto name = toName(pID);
    const ParameterID id = toID(name);
    
    switch (unit)
    {
        case Unit::Db:
            valToStr = valToStr::db();
            strToVal = strToVal::db();
            break;
        case Unit::Hz:
            valToStr = valToStr::hz();
            strToVal = strToVal::hz();
            break;
        case Unit::Unitless:
            valToStr = valToStr::unitless();
            strToVal = strToVal::unitless();
            break;
        case Unit::Integer:
            valToStr = valToStr::integer();
            strToVal = strToVal::integer();
            break;
        case Unit::NoteUnit:
            valToStr = valToStr::noteunit();
            strToVal = strToVal::noteunit();
            break;
    }
    
    vec.push_back(std::make_unique<APF>
    (
        id,
        name,
        range,
        defaultVal,
        toString(unit),
        APPCategory::genericParameter,
        valToStr,
        strToVal
    ));
}

inline Layout createParameterLayout()
{
    UniqueRAPVector params;
    
//    createParam(params, PID::GainWet, range::lin(-12.f, 12.f), 0.f, Unit::Db);
    createParam(params, PID::Root, range::withCentre(midiNoteToFrequency(0), midiNoteToFrequency(127), midiNoteToFrequency(69)), midiNoteToFrequency(69), Unit::NoteUnit);
    createParam(params, PID::Resonance, range::lin(0.707f,  20.f), 2.66f, Unit::Unitless);
    createParam(params, PID::NumHarmonics, range::stepped(1.f, static_cast<float>(MAX_HARMONICS)), 8.f, Unit::Integer);
    createParam(params, PID::FilterOrder, range::stepped(1.f, 4.f), 2.f, Unit::Integer);
    
//    createParam(params, PID::Shift, range::lin(-20000.f, 20000.f), 0.f, Unit::Hz);
    
    return {params.begin(), params.end()};
}

}
