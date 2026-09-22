#include "FactoryPresets.h"

namespace omg
{
    /*  Every preset is a complete state: the manager resets to defaults and then
        applies the string, so nothing carries over from whatever was loaded
        before. Anything not named here is at its default.

        These are real settings, not decoration. Each one was written against
        the DSP it drives - the underwater bank moves DEPTH, WATER, MURK and
        the filter slope together, the distortion bank pairs DRIVE with SMOOTH
        and the post filter so the result stays usable on a vocal, and the
        saturation bank leans on the model rather than on raw drive.
    */
    static const FactoryPreset kPresets[] =
    {
        { "Init", "VOCAL",
          "engine=0;character=50;uwDepth=20;uwWater=25;uwMix=0;mix=100" },

        // ------------------------------------------------------------ UNDERWATER
        { "Deep Dive", "UNDERWATER",
          "engine=0;character=72;uwDepth=70;uwWater=66;uwMurk=48;uwPressure=52;uwRipple=30;uwWave=40;uwBubble=28;"
          "uwSlope=2;uwResonance=1.9;compThresh=-20;compRatio=3.5;macDepth=60;macSpace=40;"
          "modOn=1;modMode=3;modRate=22;modDepth=28;modMix=24;mix=100" },

        { "Submerged", "UNDERWATER",
          "engine=0;character=58;uwDepth=52;uwWater=48;uwMurk=30;uwPressure=45;uwRipple=18;uwWave=26;uwBubble=16;"
          "uwMix=85;macBody=62;compMode=1;modOn=1;modMode=0;modRate=18;modDepth=22;modMix=18;mix=92" },

        { "Ocean Voice", "UNDERWATER",
          "engine=0;character=66;uwDepth=60;uwWater=72;uwMurk=34;uwPressure=38;uwRipple=44;uwWave=62;uwBubble=40;"
          "uwSlope=1;uwModShape=0;uwModPhase=140;macMotion=55;macSpace=58;stWidth=128;"
          "modOn=1;modMode=0;modRate=26;modDepth=40;modWidth=78;modMix=32;mix=100" },

        { "Drowned Radio", "UNDERWATER",
          "engine=0;character=78;uwDepth=74;uwWater=70;uwMurk=66;uwPressure=60;uwRipple=26;uwWave=22;uwBubble=14;"
          "uwSlope=3;eq1Freq=260;eq7Freq=3400;eq3Gain=3.5;eq4Gain=4;macDepth=55;"
          "modOn=1;modMode=4;modRate=52;modDepth=30;modMix=26;mix=100" },

        { "Dark Water", "UNDERWATER",
          "engine=0;character=70;uwDepth=76;uwWater=58;uwMurk=78;uwPressure=48;uwRipple=12;uwWave=30;uwBubble=8;"
          "uwSlope=2;uwResonance=0.9;macColor=26;macBody=64;eq2Gain=2.5;mix=100" },

        { "Pressure", "UNDERWATER",
          "engine=0;character=84;uwDepth=82;uwWater=74;uwMurk=62;uwPressure=88;uwPressResp=78;uwRipple=12;uwWave=18;"
          "uwSlope=3;compThresh=-26;compRatio=6;compMode=2;sigPunch=1;trOn=1;trAttack=34;mix=100" },

        { "Abyss", "UNDERWATER",
          "engine=0;character=94;uwDepth=94;uwWater=90;uwMurk=72;uwPressure=78;uwRipple=20;uwWave=48;uwBubble=22;"
          "uwSlope=3;uwResonance=2.6;macDepth=80;macSpace=55;oversampling=2;"
          "modOn=1;modMode=3;modRate=14;modDepth=46;modMotion=60;modMix=34;mix=100" },

        { "Liquid Vocal", "UNDERWATER",
          "engine=0;character=54;uwDepth=42;uwWater=54;uwMurk=22;uwPressure=36;uwRipple=52;uwWave=44;uwBubble=46;"
          "uwMix=76;uwModShape=1;macMotion=50;modOn=1;modMode=1;modDetune=42;modMix=30;mix=88" },

        // ------------------------------------------------------------ DISTORTION
        { "Destroyed", "DISTORTION",
          "engine=1;character=92;dsDrive=88;dsBite=78;dsBody=32;dsCrush=64;dsEdge=84;dsSmooth=26;dsType=4;"
          "dsPostFilter=6500;macDamage=88;compMode=4;compRatio=10;oversampling=3;mix=92" },

        { "Radio Damage", "DISTORTION",
          "engine=1;character=70;dsDrive=62;dsBite=70;dsBody=30;dsCrush=38;dsEdge=60;dsSmooth=30;dsType=6;"
          "dsPostFilter=5200;eq1Freq=380;eq7Freq=3800;macDamage=55;mix=100" },

        { "Aggressive", "DISTORTION",
          "engine=1;character=76;dsDrive=70;dsBite=72;dsBody=48;dsCrush=8;dsEdge=66;dsSmooth=38;dsType=1;"
          "compMode=4;compThresh=-24;compRatio=6;sigPunch=1;trOn=1;trAttack=42;mix=96" },

        { "Digital Rage", "DISTORTION",
          "engine=1;character=86;dsDrive=64;dsBite=66;dsBody=34;dsCrush=82;dsEdge=74;dsSmooth=18;dsType=9;"
          "dsPostFilter=8500;macDamage=72;sigChaos=1;oversampling=3;mix=88" },

        { "Broken Speaker", "DISTORTION",
          "engine=1;character=80;dsDrive=74;dsBite=48;dsBody=44;dsCrush=22;dsEdge=72;dsSmooth=34;dsType=5;"
          "dsAsym=36;dsBias=-24;eq7Freq=5600;macDamage=70;compMode=4;mix=88" },

        { "Dirty Vocal", "DISTORTION",
          "engine=1;character=58;dsDrive=44;dsBite=52;dsBody=58;dsCrush=0;dsEdge=34;dsSmooth=44;dsType=10;"
          "dsPreEmph=34;compMode=1;compThresh=-20;deAmount=55;mix=62" },

        { "Fuzz Voice", "DISTORTION",
          "engine=1;character=82;dsDrive=78;dsBite=64;dsBody=40;dsCrush=6;dsEdge=80;dsSmooth=40;dsType=4;"
          "dsPostFilter=7200;macDamage=66;mbOn=1;mbLow=-6;mbMid=4;mbHigh=-3;mix=84" },

        { "Industrial", "DISTORTION",
          "engine=1;character=88;dsDrive=80;dsBite=74;dsBody=36;dsCrush=52;dsEdge=78;dsSmooth=22;dsType=8;"
          "macDamage=80;compMode=4;compRatio=8;sigChaos=1;modOn=1;modMode=4;modRate=64;modDepth=22;modMix=18;"
          "oversampling=3;mix=90" },

        // ------------------------------------------------------------ SATURATION
        { "Warm Tape", "SATURATION",
          "engine=2;character=54;satDrive=40;satWarmth=58;satHarmonics=44;satThickness=52;satTone=-12;"
          "satSoftClip=38;satModel=0;compMode=3;compRatio=2.5;mix=100" },

        { "Analog Vocal", "SATURATION",
          "engine=2;character=52;satDrive=36;satWarmth=50;satHarmonics=42;satThickness=44;satTone=-4;"
          "satModel=1;satDensity=48;compMode=1;compThresh=-20;deAmount=48;mix=100" },

        { "Tube Glow", "SATURATION",
          "engine=2;character=64;satDrive=56;satWarmth=62;satHarmonics=64;satThickness=48;satTone=4;"
          "satSoftClip=44;satModel=1;satDensity=54;eq5Gain=2;sigAir=1;mix=100" },

        { "Vintage", "SATURATION",
          "engine=2;character=58;satDrive=46;satWarmth=68;satHarmonics=50;satThickness=58;satTone=-18;"
          "satModel=3;satDensity=56;eq2Gain=2;eq6Gain=-2;compMode=5;mix=100" },

        { "Thick Vocal", "SATURATION",
          "engine=2;character=64;satDrive=52;satWarmth=64;satHarmonics=56;satThickness=76;satTone=-8;"
          "satModel=3;satDensity=66;macBody=72;eq2Gain=2.5;trOn=1;trBody=26;mix=100" },

        { "Console", "SATURATION",
          "engine=2;character=48;satDrive=32;satWarmth=52;satHarmonics=36;satThickness=40;satTone=6;"
          "satModel=2;satDensity=46;macBody=60;compMode=0;mix=100" },

        { "Warm Presence", "SATURATION",
          "engine=2;character=50;satDrive=34;satWarmth=48;satHarmonics=46;satThickness=38;satTone=22;"
          "satModel=4;eq5Gain=3;eq6Gain=2.5;deAmount=58;sigAir=1;mix=100" },

        { "Final Polish", "SATURATION",
          "engine=2;character=42;satDrive=22;satWarmth=36;satHarmonics=28;satThickness=26;satSoftClip=46;"
          "satModel=5;satDensity=38;compMode=1;compThresh=-18;compRatio=2.5;compAutoRel=1;"
          "limiter=1;ceiling=-0.5;mix=100" },

        // ---------------------------------------------------------------- VOCAL
        { "Lead Vocal Polish", "VOCAL",
          "engine=2;character=44;satDrive=24;satWarmth=40;satModel=4;compMode=1;compThresh=-22;compRatio=3;"
          "compAttack=8;compRelease=120;compAutoRel=1;deAmount=55;eq5Gain=2.5;eq6Gain=2;sigAir=1;"
          "trOn=1;trAttack=18;mix=100" },

        { "Close And Dry", "VOCAL",
          "engine=2;character=38;satDrive=18;satModel=1;compMode=2;compThresh=-24;compRatio=4;compAttack=20;"
          "compRelease=90;eq1Freq=110;eq3Gain=-3;deAmount=50;trOn=1;trAttack=28;trBody=-14;mix=100" },

        { "Ad-Lib Doubler", "VOCAL",
          "engine=2;character=46;satDrive=28;satModel=0;modOn=1;modMode=1;modDetune=58;modWidth=82;modMix=42;"
          "stWidth=148;macSpace=62;deAmount=52;mix=100" },

        { "Trap Ad-Lib", "VOCAL",
          "engine=1;character=68;dsDrive=54;dsBite=66;dsBody=36;dsCrush=26;dsEdge=58;dsSmooth=34;dsType=2;"
          "compMode=4;compThresh=-26;compRatio=6;stWidth=136;macDamage=48;macSpace=50;mix=78" },

        // ---------------------------------------------------------------- HYBRID
        { "Half Sunk Grit", "HYBRID",
          "engine=0;character=66;uwDepth=56;uwWater=50;uwMurk=42;uwPressure=48;uwMix=70;macDamage=45;"
          "macMotion=40;macSpace=45;compMode=2;sigPunch=1;sigAir=1;mbOn=1;mbLow=-4;mbMid=5;mbHigh=-2;mix=94" },

        { "Wide Whisper", "HYBRID",
          "engine=2;character=42;satDrive=22;satModel=4;macSpace=70;macMotion=35;stWidth=140;deAmount=60;"
          "eq6Gain=3;sigAir=1;modOn=1;modMode=0;modRate=16;modDepth=24;modWidth=88;modMix=26;mix=96" },

        { "Drowned Machine", "HYBRID",
          "engine=0;character=94;uwDepth=92;uwWater=88;uwMurk=78;uwPressure=86;uwRipple=62;uwWave=58;uwBubble=52;"
          "uwSlope=3;uwModShape=2;macDamage=60;macMotion=75;sigChaos=1;modOn=1;modMode=4;modRate=70;modDepth=34;"
          "modMotion=72;modMix=30;oversampling=2;mix=100" },

        { "Parallel Damage", "HYBRID",
          "engine=1;character=90;dsDrive=86;dsBite=76;dsBody=30;dsCrush=56;dsEdge=80;dsSmooth=24;dsType=3;"
          "macDamage=82;compPlace=1;compMode=4;compRatio=8;dryLevel=0;wetLevel=-2;oversampling=3;mix=46" },

        { "Underwater Tube", "HYBRID",
          "engine=0;character=62;uwDepth=54;uwWater=60;uwMurk=36;uwPressure=56;uwMix=82;satModel=1;"
          "mbOn=1;mbLow=3;mbMid=-4;mbHigh=6;macBody=64;macDepth=52;trOn=1;trBody=22;mix=90" },

        { "Total Collapse", "EXTREME",
          "engine=1;character=96;dsDrive=92;dsBite=82;dsBody=24;dsCrush=72;dsEdge=88;dsSmooth=12;dsType=4;"
          "macDamage=95;compMode=4;compRatio=12;sigChaos=1;oversampling=3;mbOn=1;mbLow=-8;mbMid=8;mbHigh=-5;"
          "mix=100" }
    };

    const FactoryPreset* factoryPresets() { return kPresets; }
    int numFactoryPresets() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }
}
