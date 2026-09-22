#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace omg
{
    enum class PType { Float, Choice, Bool };

    /** One row of the single source of truth for every plugin parameter.

        The APVTS layout, the tooltips, the right-click menus and the advanced
        panel are all generated from this table, so a control cannot exist in
        the UI without a real parameter behind it.
    */
    struct ParamDesc
    {
        const char* id;
        const char* group;        // host-visible group, e.g. "COMPRESSOR"
        const char* name;
        PType       type;
        float       min, max, def;
        float       skewCentre;   // 0 = linear
        const char* unit;         // may be nullptr
        const char* choices;      // '|' separated, Choice only
        const char* tip;          // one sentence, shown in the tooltip
    };

    /** The host-visible parameter groups, in the order they are declared. */
    const juce::StringArray& parameterGroups();

    const ParamDesc*  params();
    int               numParams();
    const ParamDesc*  findParam (juce::StringRef id);
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Engine modes, in the order the mode selector shows them. */
    enum EngineMode { Underwater = 0, Distortion = 1, Saturation = 2 };

    /** Where the compressor sits relative to the character engine. */
    enum CompPlacement { CompPre = 0, CompPost = 1 };

    namespace pid
    {
        // ---- global -----------------------------------------------------
        inline constexpr const char* engine        = "engine";
        inline constexpr const char* power         = "power";
        inline constexpr const char* character     = "character";
        inline constexpr const char* inGain        = "inGain";
        inline constexpr const char* outGain       = "outGain";
        inline constexpr const char* mix           = "mix";
        inline constexpr const char* phase         = "phase";
        inline constexpr const char* mono          = "mono";

        // ---- output stage -----------------------------------------------
        inline constexpr const char* safety        = "safety";
        inline constexpr const char* limiter       = "limiter";
        inline constexpr const char* ceiling       = "ceiling";
        inline constexpr const char* truePeak      = "truePeak";

        // ---- underwater --------------------------------------------------
        inline constexpr const char* uwDepth       = "uwDepth";
        inline constexpr const char* uwWater       = "uwWater";
        inline constexpr const char* uwMurk        = "uwMurk";
        inline constexpr const char* uwPressure    = "uwPressure";
        inline constexpr const char* uwRipple      = "uwRipple";
        inline constexpr const char* uwWave        = "uwWave";
        inline constexpr const char* uwBubble      = "uwBubble";
        inline constexpr const char* uwMix         = "uwMix";
        inline constexpr const char* uwResonance   = "uwResonance";
        inline constexpr const char* uwSlope       = "uwSlope";
        inline constexpr const char* uwModShape    = "uwModShape";
        inline constexpr const char* uwModPhase    = "uwModPhase";
        inline constexpr const char* uwPressResp   = "uwPressResp";

        // ---- distortion ---------------------------------------------------
        inline constexpr const char* dsDrive       = "dsDrive";
        inline constexpr const char* dsBite        = "dsBite";
        inline constexpr const char* dsBody        = "dsBody";
        inline constexpr const char* dsCrush       = "dsCrush";
        inline constexpr const char* dsEdge        = "dsEdge";
        inline constexpr const char* dsSmooth      = "dsSmooth";
        inline constexpr const char* dsType        = "dsType";
        inline constexpr const char* dsMix         = "dsMix";
        inline constexpr const char* dsBias        = "dsBias";
        inline constexpr const char* dsAsym        = "dsAsym";
        inline constexpr const char* dsPreEmph     = "dsPreEmph";
        inline constexpr const char* dsPostFilter  = "dsPostFilter";

        // ---- saturation ----------------------------------------------------
        inline constexpr const char* satDrive      = "satDrive";
        inline constexpr const char* satWarmth     = "satWarmth";
        inline constexpr const char* satHarmonics  = "satHarmonics";
        inline constexpr const char* satThickness  = "satThickness";
        inline constexpr const char* satTone       = "satTone";
        inline constexpr const char* satSoftClip   = "satSoftClip";
        inline constexpr const char* satModel      = "satModel";
        inline constexpr const char* satDensity    = "satDensity";
        inline constexpr const char* satMix        = "satMix";

        // ---- compressor -----------------------------------------------------
        inline constexpr const char* compOn        = "compOn";
        inline constexpr const char* compThresh    = "compThresh";
        inline constexpr const char* compRatio     = "compRatio";
        inline constexpr const char* compAttack    = "compAttack";
        inline constexpr const char* compRelease   = "compRelease";
        inline constexpr const char* compKnee      = "compKnee";
        inline constexpr const char* compMakeup    = "compMakeup";
        inline constexpr const char* compMix       = "compMix";
        inline constexpr const char* compMode      = "compMode";
        inline constexpr const char* compAuto      = "compAuto";
        inline constexpr const char* compScHpf     = "compScHpf";

        // ---- compressor placement and detector --------------------------------
        inline constexpr const char* compPlace     = "compPlace";
        inline constexpr const char* compDetect    = "compDetect";
        inline constexpr const char* compAutoRel   = "compAutoRel";
        inline constexpr const char* compScExt     = "compScExt";
        inline constexpr const char* compScAmount  = "compScAmount";

        // ---- transient ---------------------------------------------------------
        inline constexpr const char* trOn          = "trOn";
        inline constexpr const char* trAttack      = "trAttack";
        inline constexpr const char* trBody        = "trBody";

        // ---- modulation --------------------------------------------------------
        inline constexpr const char* modOn         = "modOn";
        inline constexpr const char* modMode       = "modMode";
        inline constexpr const char* modRate       = "modRate";
        inline constexpr const char* modDepth      = "modDepth";
        inline constexpr const char* modDetune     = "modDetune";
        inline constexpr const char* modWidth      = "modWidth";
        inline constexpr const char* modMotion     = "modMotion";
        inline constexpr const char* modMix        = "modMix";

        // ---- multiband ----------------------------------------------------------
        inline constexpr const char* mbOn          = "mbOn";
        inline constexpr const char* mbLow         = "mbLow";
        inline constexpr const char* mbMid         = "mbMid";
        inline constexpr const char* mbHigh        = "mbHigh";
        inline constexpr const char* mbCrossLow    = "mbCrossLow";
        inline constexpr const char* mbCrossHigh   = "mbCrossHigh";

        // ---- filter fx --------------------------------------------------------------
        inline constexpr const char* fxOn          = "fxOn";
        inline constexpr const char* fxMode        = "fxMode";
        inline constexpr const char* fxSync        = "fxSync";
        inline constexpr const char* fxDiv         = "fxDiv";
        inline constexpr const char* fxRate        = "fxRate";
        inline constexpr const char* fxFreq        = "fxFreq";
        inline constexpr const char* fxDepth       = "fxDepth";
        inline constexpr const char* fxReso        = "fxReso";
        inline constexpr const char* fxSens        = "fxSens";
        inline constexpr const char* fxShape       = "fxShape";
        inline constexpr const char* fxDrive       = "fxDrive";
        inline constexpr const char* fxStereo      = "fxStereo";
        inline constexpr const char* fxMix         = "fxMix";

        // ---- pitch layer -------------------------------------------------------------
        inline constexpr const char* pitShift      = "pitShift";
        inline constexpr const char* pitMix        = "pitMix";

        // ---- space ---------------------------------------------------------------------
        inline constexpr const char* revOn         = "revOn";
        inline constexpr const char* revMix        = "revMix";
        inline constexpr const char* revSize       = "revSize";
        inline constexpr const char* revDecay      = "revDecay";
        inline constexpr const char* revDamp       = "revDamp";
        inline constexpr const char* revPre        = "revPre";
        inline constexpr const char* revDuck       = "revDuck";
        inline constexpr const char* dlyOn         = "dlyOn";
        inline constexpr const char* dlyMix        = "dlyMix";
        inline constexpr const char* dlySync       = "dlySync";
        inline constexpr const char* dlyDiv        = "dlyDiv";
        inline constexpr const char* dlyTime       = "dlyTime";
        inline constexpr const char* dlyFeedback   = "dlyFeedback";
        inline constexpr const char* dlyTone       = "dlyTone";
        inline constexpr const char* dlyPing       = "dlyPing";
        inline constexpr const char* dlyDuck       = "dlyDuck";
        inline constexpr const char* dlyWarp       = "dlyWarp";

        // ---- dry / wet levels ----------------------------------------------------
        inline constexpr const char* dryLevel      = "dryLevel";
        inline constexpr const char* wetLevel      = "wetLevel";

        // ---- de-esser --------------------------------------------------------
        inline constexpr const char* deOn          = "deOn";
        inline constexpr const char* deFreq        = "deFreq";
        inline constexpr const char* deThresh      = "deThresh";
        inline constexpr const char* deAmount      = "deAmount";
        inline constexpr const char* deRange       = "deRange";
        inline constexpr const char* deAttack      = "deAttack";
        inline constexpr const char* deRelease     = "deRelease";
        inline constexpr const char* deListen      = "deListen";

        // ---- macros -----------------------------------------------------------
        inline constexpr const char* macBody       = "macBody";
        inline constexpr const char* macColor      = "macColor";
        inline constexpr const char* macDamage     = "macDamage";
        inline constexpr const char* macDepth      = "macDepth";
        inline constexpr const char* macMotion     = "macMotion";
        inline constexpr const char* macSpace      = "macSpace";

        // ---- signature ---------------------------------------------------------
        inline constexpr const char* sigPunch      = "sigPunch";
        inline constexpr const char* sigChaos      = "sigChaos";
        inline constexpr const char* sigAir        = "sigAir";

        // ---- dsp / routing -------------------------------------------------------
        inline constexpr const char* oversampling  = "oversampling";
        inline constexpr const char* eqOn          = "eqOn";
        inline constexpr const char* stWidth       = "stWidth";
        inline constexpr const char* stMS          = "stMS";
        inline constexpr const char* stSide        = "stSide";
        inline constexpr const char* stMonoComp    = "stMonoComp";
    }

    // ---- EQ ------------------------------------------------------------------
    inline constexpr int kNumEqBands = 7;

    /** Band display names, in draw order. */
    const juce::StringArray& eqBandNames();

    /** Parameter id for one field of one band, e.g. eqId (2, "Freq") -> "eq2Freq". */
    juce::String eqId (int band, const char* field);

    enum EqFilterType { EqHighPass = 0, EqLowShelf, EqBell, EqHighShelf, EqLowPass, EqNotch };

    /** Every UI control records the parameter it drives here as it is built.

        It exists so the build can prove the rule the specification sets: no
        decorative controls, no fake meters, nothing on the panel that is not
        wired to a real AudioProcessorValueTreeState parameter.
    */
    struct ControlRegistry
    {
        static void note (const juce::String& parameterID);
        static const juce::StringArray& ids();
        static void clear();
    };
}
