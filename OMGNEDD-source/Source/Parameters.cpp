#include "Parameters.h"

namespace omg
{
    static const char* kEqTypes = "HP|L SHELF|BELL|H SHELF|LP|NOTCH";

    #define EQ_BAND(N, LABEL, FDEF, QDEF, TDEF)                                                                     \
        { "eq" #N "Freq", LABEL " Freq",  PType::Float, 20.0f, 20000.0f, FDEF, 800.0f, "Hz", nullptr,               \
          "Corner or centre frequency of the " LABEL " band." },                                                    \
        { "eq" #N "Gain", LABEL " Gain",  PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,                   \
          "How much the " LABEL " band lifts or cuts. Ignored by pass filters." },                                  \
        { "eq" #N "Q",    LABEL " Q",     PType::Float, 0.1f, 18.0f, QDEF, 1.0f, nullptr, nullptr,                  \
          "Width of the " LABEL " band. Higher is narrower." },                                                     \
        { "eq" #N "Type", LABEL " Type",  PType::Choice, 0.0f, 5.0f, (float) (TDEF), 0.0f, nullptr, kEqTypes,       \
          "Filter shape used by the " LABEL " band." },                                                             \
        { "eq" #N "On",   LABEL " On",    PType::Bool, 0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,                    \
          "Switches the " LABEL " band in or out of the signal path." },                                            \
        { "eq" #N "Dyn",  LABEL " Dyn",   PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,                    \
          "Makes the " LABEL " band level dependent, so it only works when the vocal pushes into it." }

    static const ParamDesc kParams[] =
    {
        // ---------------------------------------------------------------- global
        { pid::engine,    "Engine",      PType::Choice, 0.0f, 2.0f, 0.0f, 0.0f, nullptr,
          "UNDERWATER|DISTORTION|SATURATION",
          "Which character engine the CHARACTER knob and the centre controls drive." },
        { pid::power,     "Power",       PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Master bypass. The panel stays visible and the audio passes through untouched." },
        { pid::character, "Character",   PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "The main OMGNEDD macro. Moves the whole selected engine from subtle to extreme." },
        { pid::inGain,    "Input Gain",  PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level going into the processing, set before any character is added." },
        { pid::outGain,   "Output Gain", PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level leaving the plugin, after the limiter." },
        { pid::mix,       "Mix",         PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the processed vocal against the dry vocal, at the very end of the chain." },
        { pid::phase,     "Phase",       PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Inverts the polarity of the output, for checking against another track." },
        { pid::mono,      "Mono",        PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Sums the output to mono, for checking what a club or phone will hear." },

        // ------------------------------------------------------------ output stage
        { pid::safety,    "Safety",      PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Hard guard against anything leaving the plugin above 0 dBFS, even with the limiter off." },
        { pid::limiter,   "Limiter",     PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Transparent brickwall limiter on the output." },
        { pid::ceiling,   "Ceiling",     PType::Float, -12.0f, 0.0f, -0.3f, 0.0f, "dB", nullptr,
          "Highest level the limiter will let through." },
        { pid::truePeak,  "True Peak",   PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Limits against reconstructed inter-sample peaks rather than sample peaks." },

        // ------------------------------------------------------------- underwater
        { pid::uwDepth,     "Depth",      PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "Overall submerged character, from just damp through to fully sunk." },
        { pid::uwWater,     "Water",      PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Intensity of the water filter that closes over the top of the vocal." },
        { pid::uwMurk,      "Murk",       PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Low-mid darkness, from clear through to fully muffled." },
        { pid::uwPressure,  "Pressure",   PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "Compression and density, as if the vocal is being squeezed by the water above it." },
        { pid::uwRipple,    "Ripple",     PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Fast, fine modulation across the surface of the sound." },
        { pid::uwWave,      "Wave",       PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "Slow movement of the water filter, the swell under the vocal." },
        { pid::uwBubble,    "Bubble",     PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "Watery high-frequency texture sitting on top of the vocal." },
        { pid::uwMix,       "UW Mix",     PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the underwater engine against the signal feeding it." },
        { pid::uwResonance, "Resonance",  PType::Float, 0.1f, 12.0f, 1.2f, 1.0f, nullptr, nullptr,
          "Emphasis at the water filter's corner. High values start to whistle." },
        { pid::uwSlope,     "Filter Slope", PType::Choice, 0.0f, 3.0f, 1.0f, 0.0f, nullptr,
          "12 DB|24 DB|36 DB|48 DB",
          "Steepness of the water filter, in dB per octave." },
        { pid::uwModShape,  "Mod Shape",  PType::Choice, 0.0f, 3.0f, 0.0f, 0.0f, nullptr,
          "SINE|TRIANGLE|RANDOM|SQUARE",
          "Waveform driving the ripple and wave modulation." },
        { pid::uwModPhase,  "Mod Phase",  PType::Float, 0.0f, 360.0f, 90.0f, 0.0f, "deg", nullptr,
          "Phase offset between the left and right modulators, which widens the movement." },
        { pid::uwPressResp, "Pressure Response", PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "How quickly the pressure stage reacts, from slow swell to fast grab." },

        // ------------------------------------------------------------- distortion
        { pid::dsDrive,     "Drive",      PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Amount of nonlinear harmonic processing applied to the vocal." },
        { pid::dsBite,      "Bite",       PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "Upper-mid aggression driven into the distortion stage." },
        { pid::dsBody,      "Body",       PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Weight kept underneath the distortion so the vocal does not thin out." },
        { pid::dsCrush,     "Crush",      PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Bit and sample rate reduction, from clean through to broken." },
        { pid::dsEdge,      "Edge",       PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "Hardness of the distortion knee, from rounded to sharp." },
        { pid::dsSmooth,    "Smooth",     PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Post-distortion filtering that takes the fizz off without losing the drive." },
        { pid::dsType,      "Type",       PType::Choice, 0.0f, 7.0f, 0.0f, 0.0f, nullptr,
          "SOFT|HARD|TUBE|TAPE|FUZZ|FOLD|DIGITAL|RECTIFY",
          "Which distortion algorithm the drive stage runs." },
        { pid::dsMix,       "DS Mix",     PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the distortion engine against the signal feeding it." },
        { pid::dsBias,      "Bias",       PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Shifts the signal off centre before the nonlinearity, which brings in even harmonics." },
        { pid::dsAsym,      "Asymmetry",  PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Makes the curve clip harder on one half of the waveform than the other." },
        { pid::dsPreEmph,   "Pre-Emphasis", PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "High-frequency lift before the nonlinearity, restored afterwards." },
        { pid::dsPostFilter,"Post Filter", PType::Float, 500.0f, 20000.0f, 16000.0f, 4000.0f, "Hz", nullptr,
          "Low pass placed after the distortion to control the top end it generates." },

        // ------------------------------------------------------------- saturation
        { pid::satDrive,     "Sat Drive",  PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "How hard the vocal is pushed into the saturation model." },
        { pid::satWarmth,    "Warmth",     PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "Low-mid lift and softening that comes with the saturation." },
        { pid::satHarmonics, "Harmonics",  PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "Balance of even and odd harmonics the model generates." },
        { pid::satThickness, "Thickness",  PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Density and glue, how much the saturation pulls the vocal together." },
        { pid::satTone,      "Tone",       PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Tilt across the saturated signal, dark on the left and bright on the right." },
        { pid::satSoftClip,  "Soft Clip",  PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "Rounds the peaks after saturation instead of letting them square off." },
        { pid::satModel,     "Model",      PType::Choice, 0.0f, 4.0f, 0.0f, 0.0f, nullptr,
          "TAPE|TUBE|CONSOLE|TRANSFORMER|WARM",
          "Which saturation model the stage runs." },
        { pid::satDensity,   "Density",    PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "How tightly the model packs the signal before it saturates." },
        { pid::satMix,       "SAT Mix",    PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the saturation engine against the signal feeding it." },

        // ------------------------------------------------------------- compressor
        { pid::compOn,      "Comp On",     PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Switches the compressor in or out of the chain." },
        { pid::compThresh,  "Threshold",   PType::Float, -60.0f, 0.0f, -18.0f, 0.0f, "dB", nullptr,
          "Level above which the compressor starts working." },
        { pid::compRatio,   "Ratio",       PType::Float, 1.0f, 20.0f, 3.0f, 4.0f, ":1", nullptr,
          "How much the signal above the threshold is reduced." },
        { pid::compAttack,  "Attack",      PType::Float, 0.1f, 200.0f, 12.0f, 20.0f, "ms", nullptr,
          "How quickly the compressor grabs a loud syllable." },
        { pid::compRelease, "Release",     PType::Float, 5.0f, 1000.0f, 140.0f, 150.0f, "ms", nullptr,
          "How quickly the compressor lets go once the vocal drops." },
        { pid::compKnee,    "Knee",        PType::Float, 0.0f, 24.0f, 6.0f, 0.0f, "dB", nullptr,
          "How gradually compression comes in around the threshold." },
        { pid::compMakeup,  "Makeup",      PType::Float, -12.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Gain added after compression to put the level back." },
        { pid::compMix,     "Comp Mix",    PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Parallel blend of the compressed vocal against the uncompressed one." },
        { pid::compMode,    "Comp Mode",   PType::Choice, 0.0f, 5.0f, 1.0f, 0.0f, nullptr,
          "CLEAN|VOCAL|PUNCH|SMOOTH|AGGRESSIVE|OPTO",
          "Detector and timing character of the compressor." },
        { pid::compAuto,    "Auto",        PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Sets attack, release and makeup from the programme material." },
        { pid::compScHpf,   "Sidechain HPF", PType::Float, 20.0f, 400.0f, 90.0f, 120.0f, "Hz", nullptr,
          "Keeps low frequencies out of the detector so plosives do not pump the whole vocal." },

        // --------------------------------------------------------------- de-esser
        { pid::deOn,        "De-Ess On",   PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Switches the de-esser in or out of the chain." },
        { pid::deFreq,      "De-Ess Freq", PType::Float, 2000.0f, 16000.0f, 6500.0f, 6000.0f, "Hz", nullptr,
          "Centre of the band the de-esser listens to and reduces." },
        { pid::deThresh,    "De-Ess Threshold", PType::Float, -60.0f, 0.0f, -26.0f, 0.0f, "dB", nullptr,
          "Level in the sibilance band above which the de-esser acts." },
        { pid::deAmount,    "De-Ess Amount", PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "How hard the de-esser pulls the sibilance down." },
        { pid::deRange,     "De-Ess Range", PType::Float, 0.0f, 24.0f, 8.0f, 0.0f, "dB", nullptr,
          "Most the de-esser is ever allowed to reduce." },
        { pid::deAttack,    "De-Ess Attack", PType::Float, 0.1f, 50.0f, 1.0f, 5.0f, "ms", nullptr,
          "How quickly the de-esser reacts to an S." },
        { pid::deRelease,   "De-Ess Release", PType::Float, 5.0f, 500.0f, 60.0f, 80.0f, "ms", nullptr,
          "How quickly the de-esser recovers after an S." },
        { pid::deListen,    "Listen",      PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Monitors only the band the de-esser is working on, so you can aim it." },

        // ----------------------------------------------------------------- macros
        { pid::macBody,   "Body",   PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Weight and chest of the vocal: low shelf, thickness and compression together." },
        { pid::macColor,  "Color",  PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Tonal tilt of the whole chain, dark through neutral to bright." },
        { pid::macDamage, "Damage", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Destruction macro: distortion, saturation, bit reduction and harmonic generation at once." },
        { pid::macDepth,  "Depth",  PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "How far back the vocal sits, through the underwater filters and pressure." },
        { pid::macMotion, "Motion", PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "Movement macro: filter modulation, pitch drift, stereo movement and wow and flutter." },
        { pid::macSpace,  "Space",  PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Width and air around the vocal without adding an obvious effect." },

        // -------------------------------------------------------------- signature
        { pid::sigPunch, "Punch", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Reshapes the compressor's transient response so consonants hit harder." },
        { pid::sigChaos, "Chaos", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Adds controlled variation to the modulators and the nonlinear stages, so no two passes are identical." },
        { pid::sigAir,   "Air",   PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Adds controlled high-frequency presence above the de-esser's band." },

        // ------------------------------------------------------------ dsp/routing
        { pid::oversampling, "Oversampling", PType::Choice, 0.0f, 3.0f, 1.0f, 0.0f, nullptr,
          "OFF|2X|4X|8X",
          "How far above the session rate the nonlinear stages run, which controls aliasing." },
        { pid::eqOn,     "EQ On",    PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Switches the whole seven band equaliser in or out." },
        { pid::stWidth,  "Width",    PType::Float, 0.0f, 200.0f, 100.0f, 0.0f, "%", nullptr,
          "Stereo width of the processed vocal. 100 percent is untouched." },
        { pid::stMS,     "M/S",      PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Processes mid and side separately instead of left and right." },
        { pid::stSide,   "Side Level", PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level of the side signal on its own." },
        { pid::stMonoComp, "Mono Compatibility", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Progressively narrows the low end so the vocal survives a mono system." },

        // --------------------------------------------------------------------- EQ
        EQ_BAND (1, "HPF",      80.0f,    0.707f, EqHighPass),
        EQ_BAND (2, "LOW",      150.0f,   0.700f, EqLowShelf),
        EQ_BAND (3, "LOW MID",  420.0f,   1.100f, EqBell),
        EQ_BAND (4, "MID",      1200.0f,  1.000f, EqBell),
        EQ_BAND (5, "HIGH MID", 3500.0f,  1.300f, EqBell),
        EQ_BAND (6, "HIGH",     8000.0f,  0.700f, EqHighShelf),
        EQ_BAND (7, "LPF",      18000.0f, 0.707f, EqLowPass)
    };

    #undef EQ_BAND

    static constexpr int kCount = (int) (sizeof (kParams) / sizeof (kParams[0]));

    const ParamDesc* params()  { return kParams; }
    int              numParams() { return kCount; }

    const ParamDesc* findParam (juce::StringRef id)
    {
        for (int i = 0; i < kCount; ++i)
            if (juce::String (id) == kParams[i].id)
                return &kParams[i];
        return nullptr;
    }

    const juce::StringArray& eqBandNames()
    {
        static const juce::StringArray names { "HPF", "LOW", "LOW MID", "MID", "HIGH MID", "HIGH", "LPF" };
        return names;
    }

    static juce::StringArray& registryStorage()
    {
        static juce::StringArray storage;
        return storage;
    }

    void ControlRegistry::note (const juce::String& parameterID)
    {
        if (! registryStorage().contains (parameterID))
            registryStorage().add (parameterID);
    }

    const juce::StringArray& ControlRegistry::ids() { return registryStorage(); }
    void ControlRegistry::clear() { registryStorage().clear(); }

    juce::String eqId (int band, const char* field)
    {
        return "eq" + juce::String (band + 1) + field;
    }

    static juce::NormalisableRange<float> makeRange (const ParamDesc& d)
    {
        juce::NormalisableRange<float> r (d.min, d.max);
        if (d.skewCentre > d.min && d.skewCentre < d.max)
            r.setSkewForCentre (d.skewCentre);
        return r;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        for (int i = 0; i < kCount; ++i)
        {
            const auto& d = kParams[i];
            const juce::ParameterID pidv { d.id, 1 };

            switch (d.type)
            {
                case PType::Bool:
                    layout.add (std::make_unique<juce::AudioParameterBool> (pidv, d.name, d.def > 0.5f));
                    break;

                case PType::Choice:
                {
                    auto choices = juce::StringArray::fromTokens (juce::String (d.choices), "|", "");
                    layout.add (std::make_unique<juce::AudioParameterChoice> (pidv, d.name, choices,
                                                                             juce::jlimit (0, choices.size() - 1, (int) d.def)));
                    break;
                }

                case PType::Float:
                default:
                {
                    juce::AudioParameterFloatAttributes attrs;
                    if (d.unit != nullptr)
                        attrs = attrs.withLabel (d.unit);

                    layout.add (std::make_unique<juce::AudioParameterFloat> (pidv, d.name, makeRange (d), d.def, attrs));
                    break;
                }
            }
        }

        return layout;
    }
}
