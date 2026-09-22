#include "Parameters.h"

namespace omg
{
    static const char* kEqTypes = "HP|L SHELF|BELL|H SHELF|LP|NOTCH";
    static const char* kSlopes  = "12 DB|24 DB|36 DB|48 DB";
    static const char* kDivs    = "1/1|1/2|1/2D|1/2T|1/4|1/4D|1/4T|1/8|1/8D|1/8T|1/16|1/16D|1/16T|1/32";

    #define EQ_BAND(N, LABEL, FDEF, QDEF, TDEF)                                                                     \
        { "eq" #N "Freq", "EQ", LABEL " Freq",  PType::Float, 20.0f, 20000.0f, FDEF, 800.0f, "Hz", nullptr,               \
          "Corner or centre frequency of the " LABEL " band." },                                                    \
        { "eq" #N "Gain", "EQ", LABEL " Gain",  PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,                   \
          "How much the " LABEL " band lifts or cuts. Ignored by pass filters." },                                  \
        { "eq" #N "Q",    "EQ", LABEL " Q",     PType::Float, 0.1f, 18.0f, QDEF, 1.0f, nullptr, nullptr,                  \
          "Width of the " LABEL " band. Higher is narrower." },                                                     \
        { "eq" #N "Type", "EQ", LABEL " Type",  PType::Choice, 0.0f, 5.0f, (float) (TDEF), 0.0f, nullptr, kEqTypes,       \
          "Filter shape used by the " LABEL " band." },                                                             \
        { "eq" #N "On",   "EQ", LABEL " On",    PType::Bool, 0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,                    \
          "Switches the " LABEL " band in or out of the signal path." },                                            \
        { "eq" #N "Dyn",  "EQ", LABEL " Dyn",   PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,                    \
          "Makes the " LABEL " band level dependent, so it only works when the vocal pushes into it." },            \
        { "eq" #N "Slope", "EQ", LABEL " Slope", PType::Choice, 0.0f, 3.0f, 0.0f, 0.0f, nullptr, kSlopes,            \
          "Steepness of the " LABEL " band when it is a high or low pass, in dB per octave." }

    static const ParamDesc kParams[] =
    {
        // ---------------------------------------------------------------- global
        { pid::engine, "GLOBAL",    "Engine",      PType::Choice, 0.0f, 2.0f, 0.0f, 0.0f, nullptr,
          "UNDERWATER|DISTORTION|SATURATION",
          "Which character engine the CHARACTER knob and the centre controls drive." },
        { pid::power, "GLOBAL",     "Power",       PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Master bypass. The panel stays visible and the audio passes through untouched." },
        { pid::character, "GLOBAL", "Character",   PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "The main OMGNEDD macro. Moves the whole selected engine from subtle to extreme." },
        { pid::inGain, "GLOBAL",    "Input Gain",  PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level going into the processing, set before any character is added." },
        { pid::outGain, "GLOBAL",   "Output Gain", PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level leaving the plugin, after the limiter." },
        { pid::mix, "GLOBAL",       "Mix",         PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the processed vocal against the dry vocal, at the very end of the chain." },
        { pid::phase, "GLOBAL",     "Phase",       PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Inverts the polarity of the output, for checking against another track." },
        { pid::mono, "GLOBAL",      "Mono",        PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Sums the output to mono, for checking what a club or phone will hear." },

        // ------------------------------------------------------------ output stage
        { pid::safety, "OUTPUT",    "Safety",      PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Hard guard against anything leaving the plugin above 0 dBFS, even with the limiter off." },
        { pid::limiter, "OUTPUT",   "Limiter",     PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Transparent brickwall limiter on the output." },
        { pid::ceiling, "OUTPUT",   "Ceiling",     PType::Float, -12.0f, 0.0f, -0.3f, 0.0f, "dB", nullptr,
          "Highest level the limiter will let through." },
        { pid::truePeak, "OUTPUT",  "True Peak",   PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Limits against reconstructed inter-sample peaks rather than sample peaks." },

        // ------------------------------------------------------------- underwater
        { pid::uwDepth, "UNDERWATER",     "Depth",      PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "How far under the vocal goes: the water filter closes from 16 kHz down to 400 Hz." },
        { pid::uwWater, "UNDERWATER",     "Water",      PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "How wet it is: more resonance on the filter, and a dark ducked reverb and delay wash around the vocal." },
        { pid::uwMurk, "UNDERWATER",      "Murk",       PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Low-mid weight and a dark top, the thick muddy density of an underground vocal." },
        { pid::uwPressure, "UNDERWATER",  "Pressure",   PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "Drive into the water filter and a pumping compressor after it: squashed, gritty and dense." },
        { pid::uwRipple, "UNDERWATER",    "Ripple",     PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Pitch warble, up to about 45 cents: the vocal will not sit still on the note." },
        { pid::uwWave, "UNDERWATER",      "Wave",       PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "A slow swell sweeping the filter by up to two octaves: the tide under the vocal." },
        { pid::uwBubble, "UNDERWATER",    "Bubble",     PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "A random gurgle on the filter, 6 to 16 times a second: bubbles." },
        { pid::uwMix, "UNDERWATER",       "UW Mix",     PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the underwater engine against the signal feeding it." },
        { pid::uwResonance, "UNDERWATER", "Resonance",  PType::Float, 0.1f, 12.0f, 1.2f, 1.0f, nullptr, nullptr,
          "Emphasis at the filter's corner. High values whistle; WATER adds more on top." },
        { pid::uwSlope, "UNDERWATER",     "Filter Slope", PType::Choice, 0.0f, 3.0f, 1.0f, 0.0f, nullptr,
          "12 DB|24 DB|36 DB|48 DB",
          "Steepness of the water filter, in dB per octave." },
        { pid::uwModShape, "UNDERWATER",  "Mod Shape",  PType::Choice, 0.0f, 3.0f, 0.0f, 0.0f, nullptr,
          "SINE|TRIANGLE|RANDOM|SQUARE",
          "Waveform of the WAVE swell." },
        { pid::uwModPhase, "UNDERWATER",  "Mod Phase",  PType::Float, 0.0f, 360.0f, 90.0f, 0.0f, "deg", nullptr,
          "Offset between the left and right modulators, which spreads the movement across the stereo image." },
        { pid::uwPressResp, "UNDERWATER", "Pressure Response", PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "How fast the PRESSURE pump grabs and lets go, from slow swell to fast choke." },

        // ------------------------------------------------------------- distortion
        { pid::dsDrive, "DISTORTION",     "Drive",      PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Amount of nonlinear harmonic processing applied to the vocal." },
        { pid::dsBite, "DISTORTION",      "Bite",       PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "Upper-mid aggression driven into the distortion stage." },
        { pid::dsBody, "DISTORTION",      "Body",       PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Weight kept underneath the distortion so the vocal does not thin out." },
        { pid::dsCrush, "DISTORTION",     "Crush",      PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Bit and sample rate reduction, from clean through to broken." },
        { pid::dsEdge, "DISTORTION",      "Edge",       PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "Hardness of the distortion knee, from rounded to sharp." },
        { pid::dsSmooth, "DISTORTION",    "Smooth",     PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Post-distortion filtering that takes the fizz off without losing the drive." },
        { pid::dsType, "DISTORTION",      "Type",       PType::Choice, 0.0f, 10.0f, 0.0f, 0.0f, nullptr,
          "SOFT|HARD|TUBE|TAPE|FUZZ|FOLD|DIGITAL|RECTIFY|SHAPER|BITCRUSH|ASYM",
          "Which distortion algorithm the drive stage runs." },
        { pid::dsMix, "DISTORTION",       "DS Mix",     PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the distortion engine against the signal feeding it." },
        { pid::dsBias, "DISTORTION",      "Bias",       PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Shifts the signal off centre before the nonlinearity, which brings in even harmonics." },
        { pid::dsAsym, "DISTORTION",      "Asymmetry",  PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Makes the curve clip harder on one half of the waveform than the other." },
        { pid::dsPreEmph, "DISTORTION",   "Pre-Emphasis", PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "High-frequency lift before the nonlinearity, restored afterwards." },
        { pid::dsPostFilter, "DISTORTION","Post Filter", PType::Float, 500.0f, 20000.0f, 16000.0f, 4000.0f, "Hz", nullptr,
          "Low pass placed after the distortion to control the top end it generates." },

        // ------------------------------------------------------------- saturation
        { pid::satDrive, "SATURATION",     "Sat Drive",  PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "How hard the vocal is pushed into the saturation model." },
        { pid::satWarmth, "SATURATION",    "Warmth",     PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "Low-mid lift and softening that comes with the saturation." },
        { pid::satHarmonics, "SATURATION", "Harmonics",  PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "Balance of even and odd harmonics the model generates." },
        { pid::satThickness, "SATURATION", "Thickness",  PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Density and glue, how much the saturation pulls the vocal together." },
        { pid::satTone, "SATURATION",      "Tone",       PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Tilt across the saturated signal, dark on the left and bright on the right." },
        { pid::satSoftClip, "SATURATION",  "Soft Clip",  PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "Rounds the peaks after saturation instead of letting them square off." },
        { pid::satModel, "SATURATION",     "Model",      PType::Choice, 0.0f, 5.0f, 0.0f, 0.0f, nullptr,
          "TAPE|TUBE|CONSOLE|TRANSFORMER|WARM|CLEAN",
          "Which saturation model the stage runs." },
        { pid::satDensity, "SATURATION",   "Density",    PType::Float, 0.0f, 100.0f, 40.0f, 0.0f, "%", nullptr,
          "How tightly the model packs the signal before it saturates." },
        { pid::satMix, "SATURATION",       "SAT Mix",    PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the saturation engine against the signal feeding it." },

        // ------------------------------------------------------------- compressor
        { pid::compOn, "COMPRESSOR",      "Comp On",     PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Switches the compressor in or out of the chain." },
        { pid::compThresh, "COMPRESSOR",  "Threshold",   PType::Float, -60.0f, 0.0f, -18.0f, 0.0f, "dB", nullptr,
          "Level above which the compressor starts working." },
        { pid::compRatio, "COMPRESSOR",   "Ratio",       PType::Float, 1.0f, 20.0f, 3.0f, 4.0f, ":1", nullptr,
          "How much the signal above the threshold is reduced." },
        { pid::compAttack, "COMPRESSOR",  "Attack",      PType::Float, 0.1f, 200.0f, 12.0f, 20.0f, "ms", nullptr,
          "How quickly the compressor grabs a loud syllable." },
        { pid::compRelease, "COMPRESSOR", "Release",     PType::Float, 5.0f, 1000.0f, 140.0f, 150.0f, "ms", nullptr,
          "How quickly the compressor lets go once the vocal drops." },
        { pid::compKnee, "COMPRESSOR",    "Knee",        PType::Float, 0.0f, 24.0f, 6.0f, 0.0f, "dB", nullptr,
          "How gradually compression comes in around the threshold." },
        { pid::compMakeup, "COMPRESSOR",  "Makeup",      PType::Float, -12.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Gain added after compression to put the level back." },
        { pid::compMix, "COMPRESSOR",     "Comp Mix",    PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Parallel blend of the compressed vocal against the uncompressed one." },
        { pid::compMode, "COMPRESSOR",    "Comp Mode",   PType::Choice, 0.0f, 5.0f, 1.0f, 0.0f, nullptr,
          "CLEAN|VOCAL|PUNCH|SMOOTH|AGGRESSIVE|OPTO",
          "Detector and timing character of the compressor." },
        { pid::compAuto, "COMPRESSOR",    "Auto",        PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Sets attack, release and makeup from the programme material." },
        { pid::compScHpf, "COMPRESSOR",   "Sidechain HPF", PType::Float, 20.0f, 400.0f, 90.0f, 120.0f, "Hz", nullptr,
          "Keeps low frequencies out of the detector so plosives do not pump the whole vocal." },

        { pid::compPlace, "COMPRESSOR",   "Comp Placement", PType::Choice, 0.0f, 1.0f, 0.0f, 0.0f, nullptr,
          "PRE|POST",
          "Whether the compressor works on the vocal before the character engine or on what comes out of it." },
        { pid::compDetect, "COMPRESSOR",  "Detector",    PType::Choice, 0.0f, 1.0f, 0.0f, 0.0f, nullptr,
          "PEAK|RMS",
          "Whether the detector follows peaks, which catches consonants, or RMS, which follows loudness." },
        { pid::compAutoRel, "COMPRESSOR", "Auto Release", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Lets the release time follow the programme, fast on a short word and slow under a sustained note." },
        { pid::compScExt, "COMPRESSOR",   "External SC", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Feeds the compressor's detector from the sidechain input instead of the vocal itself." },
        { pid::compScAmount, "COMPRESSOR", "SC Amount",  PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "How much of the external sidechain the detector hears, blended against the vocal." },

        // --------------------------------------------------------------- transient
        { pid::trOn, "TRANSIENT",     "Transient On", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Switches the vocal attack and body shaper in or out of the chain." },
        { pid::trAttack, "TRANSIENT", "Attack",       PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Lifts or softens the front of every word, which is what makes a vocal sound close or held back." },
        { pid::trBody, "TRANSIENT",   "Body",         PType::Float, -100.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Lifts or thins the part of the note after the consonant, the sustain the vocal rides on." },

        // -------------------------------------------------------------- modulation
        { pid::modOn, "MODULATION",     "Mod On",   PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Switches the pitch and time modulation section in or out of the chain." },
        { pid::modMode, "MODULATION",   "Mod Mode", PType::Choice, 0.0f, 4.0f, 0.0f, 0.0f, nullptr,
          "CHORUS|MICRO PITCH|VIBRATO|TAPE WOW|TAPE FLUTTER",
          "Which kind of movement the modulation section produces." },
        { pid::modRate, "MODULATION",   "Mod Rate", PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Speed of the modulation, scaled to what the selected mode is for." },
        { pid::modDepth, "MODULATION",  "Mod Depth", PType::Float, 0.0f, 100.0f, 30.0f, 0.0f, "%", nullptr,
          "How far the modulation moves the vocal." },
        { pid::modDetune, "MODULATION", "Detune",   PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "Pitch offset of the doubled voices, up to about twenty two cents either side." },
        { pid::modWidth, "MODULATION",  "Mod Width", PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Phase offset between the left and right modulators, which spreads the movement across the image." },
        { pid::modMotion, "MODULATION", "Mod Motion", PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Adds drift and instability on top of the modulator, so the movement is never quite regular." },
        { pid::modMix, "MODULATION",    "Mod Mix",  PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "Blend of the modulated voices against the signal feeding them." },

        // --------------------------------------------------------------- multiband
        { pid::mbOn, "MULTIBAND",        "Multiband On", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Lets the low, mid and high bands hit the character engine at different levels." },
        { pid::mbLow, "MULTIBAND",       "MB Low",  PType::Float, -12.0f, 12.0f, 0.0f, 0.0f, "dB", nullptr,
          "How much harder or softer the low band is driven into the character engine." },
        { pid::mbMid, "MULTIBAND",       "MB Mid",  PType::Float, -12.0f, 12.0f, 0.0f, 0.0f, "dB", nullptr,
          "How much harder or softer the mid band is driven into the character engine." },
        { pid::mbHigh, "MULTIBAND",      "MB High", PType::Float, -12.0f, 12.0f, 0.0f, 0.0f, "dB", nullptr,
          "How much harder or softer the high band is driven into the character engine." },
        { pid::mbCrossLow, "MULTIBAND",  "MB Cross Low",  PType::Float, 60.0f, 700.0f, 220.0f, 250.0f, "Hz", nullptr,
          "Where the low band ends and the mid band begins." },
        { pid::mbCrossHigh, "MULTIBAND", "MB Cross High", PType::Float, 1000.0f, 9000.0f, 2600.0f, 3000.0f, "Hz", nullptr,
          "Where the mid band ends and the high band begins." },

        // --------------------------------------------------------------- filter fx
        { pid::fxOn,     "FILTER FX", "Filter FX On", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Switches the wah, wobble, talk and phaser section in or out." },
        { pid::fxMode,   "FILTER FX", "FX Mode", PType::Choice, 0.0f, 4.0f, 1.0f, 0.0f, nullptr,
          "AUTO WAH|WOBBLE|LFO WAH|TALK|PHASER",
          "AUTO WAH follows the voice, WOBBLE is a synced resonant low pass, LFO WAH sweeps a wah, TALK morphs through vowels, PHASER sweeps notches." },
        { pid::fxSync,   "FILTER FX", "FX Sync", PType::Bool, 0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Locks the sweep to the host tempo and song position." },
        { pid::fxDiv,    "FILTER FX", "FX Division", PType::Choice, 0.0f, 13.0f, 4.0f, 0.0f, nullptr, kDivs,
          "Length of one sweep when synced to the host tempo." },
        { pid::fxRate,   "FILTER FX", "FX Rate", PType::Float, 0.05f, 20.0f, 2.0f, 2.0f, "Hz", nullptr,
          "Speed of the sweep when it is not synced." },
        { pid::fxFreq,   "FILTER FX", "FX Freq", PType::Float, 80.0f, 4000.0f, 500.0f, 600.0f, "Hz", nullptr,
          "Where the sweep starts. In TALK it moves every vowel up or down." },
        { pid::fxDepth,  "FILTER FX", "FX Depth", PType::Float, 0.0f, 100.0f, 65.0f, 0.0f, "%", nullptr,
          "How far the filter sweeps, up to five octaves." },
        { pid::fxReso,   "FILTER FX", "FX Reso", PType::Float, 0.0f, 100.0f, 60.0f, 0.0f, "%", nullptr,
          "Resonance of the moving filter. This is what makes a wah quack and a wobble growl." },
        { pid::fxSens,   "FILTER FX", "FX Sens", PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "How readily the voice opens the AUTO WAH." },
        { pid::fxShape,  "FILTER FX", "FX Shape", PType::Choice, 0.0f, 5.0f, 0.0f, 0.0f, nullptr,
          "SINE|TRIANGLE|SAW UP|SAW DOWN|SQUARE|S&H",
          "Waveform of the sweep. SQUARE chops between two filter settings; S&H jumps to a new one every step." },
        { pid::fxDrive,  "FILTER FX", "FX Drive", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Grit pushed into the moving filter." },
        { pid::fxStereo, "FILTER FX", "FX Stereo", PType::Float, 0.0f, 180.0f, 0.0f, 0.0f, "deg", nullptr,
          "Offset between the left and right sweeps, which makes the movement travel across the image." },
        { pid::fxMix,    "FILTER FX", "FX Mix", PType::Float, 0.0f, 100.0f, 100.0f, 0.0f, "%", nullptr,
          "Blend of the moving filter against the signal feeding it." },

        // ------------------------------------------------------------------- pitch
        { pid::pitShift, "MODULATION", "Pitch Shift", PType::Float, -12.0f, 12.0f, 0.0f, 0.0f, "st", nullptr,
          "Transposes a layer of the vocal. -12 is the octave-down demon layer, +5 to +12 the pitched-up voice." },
        { pid::pitMix,   "MODULATION", "Pitch Mix", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Blend of the shifted layer. At 50 percent it sits under the vocal; at 100 it replaces it." },

        // ------------------------------------------------------------------- space
        { pid::revOn,    "SPACE", "Reverb On", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Switches the reverb in or out. UNDERWATER's WATER knob sends to its own wash either way." },
        { pid::revMix,   "SPACE", "Reverb Mix", PType::Float, 0.0f, 100.0f, 25.0f, 0.0f, "%", nullptr,
          "Reverb level. Up to 50 percent the vocal stays at full level; beyond that it fades into the wash." },
        { pid::revSize,  "SPACE", "Reverb Size", PType::Float, 0.0f, 100.0f, 55.0f, 0.0f, "%", nullptr,
          "Size of the space, from a small room to a cavern." },
        { pid::revDecay, "SPACE", "Reverb Decay", PType::Float, 0.2f, 12.0f, 2.4f, 2.5f, "s", nullptr,
          "How long the tail takes to die away by 60 dB." },
        { pid::revDamp,  "SPACE", "Reverb Damp", PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "How dark the tail is. The underground wash lives in the top half." },
        { pid::revPre,   "SPACE", "Pre-Delay", PType::Float, 0.0f, 250.0f, 20.0f, 60.0f, "ms", nullptr,
          "Gap before the reverb starts, which keeps the words in front of the space." },
        { pid::revDuck,  "SPACE", "Reverb Duck", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Pulls the reverb down while the vocal is loud, so it swells in the gaps instead of burying the words." },
        { pid::dlyOn,    "SPACE", "Delay On", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Switches the delay in or out." },
        { pid::dlyMix,   "SPACE", "Delay Mix", PType::Float, 0.0f, 100.0f, 20.0f, 0.0f, "%", nullptr,
          "Level of the repeats." },
        { pid::dlySync,  "SPACE", "Delay Sync", PType::Bool, 0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Sets the delay time from the host tempo instead of in milliseconds." },
        { pid::dlyDiv,   "SPACE", "Delay Division", PType::Choice, 0.0f, 13.0f, 8.0f, 0.0f, nullptr, kDivs,
          "Delay time as a note length, when synced." },
        { pid::dlyTime,  "SPACE", "Delay Time", PType::Float, 10.0f, 2000.0f, 375.0f, 300.0f, "ms", nullptr,
          "Delay time, when not synced." },
        { pid::dlyFeedback, "SPACE", "Delay Feedback", PType::Float, 0.0f, 100.0f, 35.0f, 0.0f, "%", nullptr,
          "How many repeats there are." },
        { pid::dlyTone,  "SPACE", "Delay Tone", PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "Colour of the repeats, from dark and distant to bright." },
        { pid::dlyPing,  "SPACE", "Ping Pong", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Bounces the repeats between left and right." },
        { pid::dlyDuck,  "SPACE", "Delay Duck", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Pulls the repeats down while the vocal is loud." },
        { pid::dlyWarp,  "SPACE", "Delay Warp", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Tape wow on the repeats, so each one bends slightly in pitch." },

        // -------------------------------------------------------------- dry / wet
        { pid::dryLevel, "OUTPUT", "Dry Level", PType::Float, -24.0f, 12.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level of the untouched vocal inside the final blend." },
        { pid::wetLevel, "OUTPUT", "Wet Level", PType::Float, -24.0f, 12.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level of the processed vocal inside the final blend." },

        // --------------------------------------------------------------- de-esser
        { pid::deOn, "DE-ESSER",        "De-Ess On",   PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Switches the de-esser in or out of the chain." },
        { pid::deFreq, "DE-ESSER",      "De-Ess Freq", PType::Float, 2000.0f, 16000.0f, 6500.0f, 6000.0f, "Hz", nullptr,
          "Centre of the band the de-esser listens to and reduces." },
        { pid::deThresh, "DE-ESSER",    "De-Ess Threshold", PType::Float, -60.0f, 0.0f, -26.0f, 0.0f, "dB", nullptr,
          "Level in the sibilance band above which the de-esser acts." },
        { pid::deAmount, "DE-ESSER",    "De-Ess Amount", PType::Float, 0.0f, 100.0f, 45.0f, 0.0f, "%", nullptr,
          "How hard the de-esser pulls the sibilance down." },
        { pid::deRange, "DE-ESSER",     "De-Ess Range", PType::Float, 0.0f, 24.0f, 8.0f, 0.0f, "dB", nullptr,
          "Most the de-esser is ever allowed to reduce." },
        { pid::deAttack, "DE-ESSER",    "De-Ess Attack", PType::Float, 0.1f, 50.0f, 1.0f, 5.0f, "ms", nullptr,
          "How quickly the de-esser reacts to an S." },
        { pid::deRelease, "DE-ESSER",   "De-Ess Release", PType::Float, 5.0f, 500.0f, 60.0f, 80.0f, "ms", nullptr,
          "How quickly the de-esser recovers after an S." },
        { pid::deListen, "DE-ESSER",    "Listen",      PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Monitors only the band the de-esser is working on, so you can aim it." },

        // ----------------------------------------------------------------- macros
        { pid::macBody, "MACROS",   "Body",   PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Weight and chest of the vocal: low shelf, thickness and compression together." },
        { pid::macColor, "MACROS",  "Color",  PType::Float, 0.0f, 100.0f, 50.0f, 0.0f, "%", nullptr,
          "Tonal tilt of the whole chain, dark through neutral to bright." },
        { pid::macDamage, "MACROS", "Damage", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Destruction macro: distortion, saturation, bit reduction and harmonic generation at once." },
        { pid::macDepth, "MACROS",  "Depth",  PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "How far back the vocal sits, through the underwater filters and pressure." },
        { pid::macMotion, "MACROS", "Motion", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Movement macro: filter modulation, pitch drift, stereo movement and wow and flutter." },
        { pid::macSpace, "MACROS",  "Space",  PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
          "Width and air around the vocal without adding an obvious effect." },

        // -------------------------------------------------------------- signature
        { pid::sigPunch, "MACROS", "Punch", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Reshapes the compressor's transient response so consonants hit harder." },
        { pid::sigChaos, "MACROS", "Chaos", PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Adds controlled variation to the modulators and the nonlinear stages, so no two passes are identical." },
        { pid::sigAir, "MACROS",   "Air",   PType::Bool, 0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Adds controlled high-frequency presence above the de-esser's band." },

        // ------------------------------------------------------------ dsp/routing
        { pid::oversampling, "DSP", "Oversampling", PType::Choice, 0.0f, 3.0f, 1.0f, 0.0f, nullptr,
          "OFF|2X|4X|8X",
          "How far above the session rate the nonlinear stages run, which controls aliasing." },
        { pid::eqOn, "DSP",     "EQ On",    PType::Bool,  0.0f, 1.0f, 1.0f, 0.0f, nullptr, nullptr,
          "Switches the whole seven band equaliser in or out." },
        { pid::stWidth, "DSP",  "Width",    PType::Float, 0.0f, 200.0f, 100.0f, 0.0f, "%", nullptr,
          "Stereo width of the processed vocal. 100 percent is untouched." },
        { pid::stMS, "DSP",     "M/S",      PType::Bool,  0.0f, 1.0f, 0.0f, 0.0f, nullptr, nullptr,
          "Runs the character engine on the mid channel only, so stereo width and doubles in the recording pass through it clean." },
        { pid::stSide, "DSP",   "Side Level", PType::Float, -24.0f, 24.0f, 0.0f, 0.0f, "dB", nullptr,
          "Level of the side signal on its own." },
        { pid::stMonoComp, "DSP", "Mono Compatibility", PType::Float, 0.0f, 100.0f, 0.0f, 0.0f, "%", nullptr,
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

    const juce::StringArray& parameterGroups()
    {
        static const juce::StringArray groups = []
        {
            juce::StringArray g;
            for (int i = 0; i < kCount; ++i)
                g.addIfNotAlreadyThere (kParams[i].group);
            return g;
        }();
        return groups;
    }

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

    /** One parameter, built from its row. */
    static std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParamDesc& d);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // parameters are declared inside host-visible groups, so a host that
        // shows a tree shows one, and automation ids are unaffected either way
        for (const auto& groupName : parameterGroups())
        {
            auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
                groupName.toLowerCase().replaceCharacter (' ', '_'), groupName, "|");

            for (int i = 0; i < kCount; ++i)
                if (groupName == kParams[i].group)
                    group->addChild (makeParameter (kParams[i]));

            layout.add (std::move (group));
        }

        return layout;
    }

    static std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParamDesc& d)
    {
        {
            const juce::ParameterID pidv { d.id, 1 };

            switch (d.type)
            {
                case PType::Bool:
                    return std::make_unique<juce::AudioParameterBool> (pidv, d.name, d.def > 0.5f);

                case PType::Choice:
                {
                    auto choices = juce::StringArray::fromTokens (juce::String (d.choices), "|", "");
                    return std::make_unique<juce::AudioParameterChoice> (pidv, d.name, choices,
                                                                        juce::jlimit (0, choices.size() - 1, (int) d.def));
                }

                case PType::Float:
                default:
                {
                    juce::AudioParameterFloatAttributes attrs;
                    if (d.unit != nullptr)
                        attrs = attrs.withLabel (d.unit);

                    return std::make_unique<juce::AudioParameterFloat> (pidv, d.name, makeRange (d), d.def, attrs);
                }
            }
        }
    }
}
