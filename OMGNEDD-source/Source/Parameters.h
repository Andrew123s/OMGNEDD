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
        const char* name;
        PType       type;
        float       min, max, def;
        float       skewCentre;   // 0 = linear
        const char* unit;         // may be nullptr
        const char* choices;      // '|' separated, Choice only
        const char* tip;          // one sentence, shown in the tooltip
    };

    const ParamDesc*  params();
    int               numParams();
    const ParamDesc*  findParam (juce::StringRef id);
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Engine modes, in the order the mode selector shows them. */
    enum EngineMode { Underwater = 0, Distortion = 1, Saturation = 2 };

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
