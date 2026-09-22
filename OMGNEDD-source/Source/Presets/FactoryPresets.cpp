#include "FactoryPresets.h"

namespace omg
{
    static const FactoryPreset kPresets[] =
    {
        { "Init", "VOCAL",
          "engine=0;character=50;uwDepth=20;uwWater=25;uwMix=0;mix=100" },

        { "Deep Dive", "UNDERWATER",
          "engine=0;character=72;uwDepth=70;uwWater=66;uwMurk=48;uwPressure=52;uwRipple=30;uwWave=40;uwBubble=28;"
          "uwSlope=2;uwResonance=1.9;compThresh=-20;compRatio=3.5;macDepth=60;macSpace=40;mix=100" },

        { "Submerged Lead", "UNDERWATER",
          "engine=0;character=58;uwDepth=52;uwWater=48;uwMurk=30;uwPressure=45;uwRipple=18;uwWave=26;uwBubble=16;"
          "uwMix=85;macBody=62;compMode=1;mix=92" },

        { "Pressure Chamber", "UNDERWATER",
          "engine=0;character=84;uwDepth=82;uwWater=74;uwMurk=62;uwPressure=80;uwPressResp=78;uwRipple=12;uwWave=18;"
          "uwSlope=3;compThresh=-26;compRatio=6;sigPunch=1;mix=100" },

        { "Broken Radio", "DISTORTION",
          "engine=1;character=70;dsDrive=62;dsBite=70;dsBody=30;dsCrush=38;dsEdge=60;dsSmooth=20;dsType=6;"
          "eq1Freq=220;eq7Freq=4200;macDamage=55;mix=100" },

        { "Fold Damage", "DISTORTION",
          "engine=1;character=80;dsDrive=74;dsBite=48;dsBody=44;dsCrush=18;dsEdge=72;dsSmooth=34;dsType=5;"
          "dsAsym=32;macDamage=70;compMode=4;mix=88" },

        { "Tube Shout", "DISTORTION",
          "engine=1;character=62;dsDrive=54;dsBite=58;dsBody=62;dsCrush=0;dsEdge=40;dsSmooth=40;dsType=2;"
          "dsBias=22;compMode=2;sigPunch=1;mix=100" },

        { "Tape Glue", "SATURATION",
          "engine=2;character=54;satDrive=40;satWarmth=58;satHarmonics=44;satThickness=52;satTone=-12;"
          "satSoftClip=38;satModel=0;compMode=3;compRatio=2.5;mix=100" },

        { "Console Warmth", "SATURATION",
          "engine=2;character=48;satDrive=32;satWarmth=52;satHarmonics=36;satThickness=40;satTone=6;"
          "satModel=2;satDensity=46;macBody=60;mix=100" },

        { "Transformer Weight", "SATURATION",
          "engine=2;character=64;satDrive=52;satWarmth=64;satHarmonics=56;satThickness=68;satTone=-8;"
          "satModel=3;satDensity=62;eq2Gain=2.5;mix=100" },

        { "Lead Vocal Polish", "VOCAL",
          "engine=2;character=44;satDrive=24;satWarmth=40;satModel=4;compMode=1;compThresh=-22;compRatio=3;"
          "compAttack=8;compRelease=120;deAmount=55;eq5Gain=2.5;eq6Gain=2;sigAir=1;mix=100" },

        { "Close And Dry", "VOCAL",
          "engine=2;character=38;satDrive=18;satModel=1;compMode=2;compThresh=-24;compRatio=4;compAttack=20;"
          "compRelease=90;eq1Freq=110;eq3Gain=-3;deAmount=50;mix=100" },

        { "Total Collapse", "EXTREME",
          "engine=1;character=96;dsDrive=92;dsBite=82;dsBody=24;dsCrush=72;dsEdge=88;dsSmooth=12;dsType=4;"
          "macDamage=95;compMode=4;compRatio=12;sigChaos=1;oversampling=3;mix=100" },

        { "Drowned Machine", "EXTREME",
          "engine=0;character=94;uwDepth=92;uwWater=88;uwMurk=78;uwPressure=86;uwRipple=62;uwWave=58;uwBubble=52;"
          "uwSlope=3;uwModShape=2;macDamage=60;macMotion=75;sigChaos=1;mix=100" },

        { "Half Sunk Grit", "HYBRID",
          "engine=0;character=66;uwDepth=56;uwWater=50;uwMurk=42;uwPressure=48;uwMix=70;macDamage=45;"
          "macMotion=40;macSpace=45;compMode=2;sigPunch=1;sigAir=1;mix=94" },

        { "Wide Whisper", "HYBRID",
          "engine=2;character=42;satDrive=22;satModel=4;macSpace=70;macMotion=35;stWidth=140;deAmount=60;"
          "eq6Gain=3;sigAir=1;mix=96" }
    };

    const FactoryPreset* factoryPresets() { return kPresets; }
    int numFactoryPresets() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }
}
