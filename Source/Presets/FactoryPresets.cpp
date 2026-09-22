#include "FactoryPresets.h"

namespace omg
{
    /*  Every preset is a complete state: the manager resets every parameter to
        its default and then applies the string. Anything not named is at its
        default, and every default is neutral: no FX, no reverb, no delay, no
        pitch layer, macros centred or at zero.

        UNDERWATER's WATER knob brings its own dark ducked reverb and ping-pong
        delay wash with it, so the underwater presets do not need the SPACE
        page switched on to sound submerged. Where a preset does switch SPACE
        on, WATER adds to that instead.

        The build loads every one of these, runs a vocal through it and fails
        if the result is not finite or leaves the safety ceiling.
    */
    static const FactoryPreset kPresets[] =
    {
        { "Init", "VOCAL",
          "engine=0;uwMix=0" },

        // ----------------------------------------------------------- UNDERGROUND
        // the rage / underground vocal: sunk, warbling, squashed, washed out
        { "Rage Sub", "UNDERGROUND",
          "engine=0;character=58;uwDepth=46;uwWater=62;uwMurk=38;uwPressure=58;uwRipple=42;uwWave=24;uwBubble=12;"
          "uwResonance=1.6;uwPressResp=70;macDamage=32;pitShift=-12;pitMix=18;compMode=4;compThresh=-24;compRatio=5;"
          "deAmount=60;stWidth=125;mix=100" },

        { "Vamp Haze", "UNDERGROUND",
          "engine=0;character=60;uwDepth=54;uwWater=76;uwMurk=52;uwPressure=48;uwRipple=34;uwWave=36;uwBubble=10;"
          "uwModPhase=140;modOn=1;modMode=0;modRate=22;modDepth=38;modWidth=85;modMix=30;stWidth=140;mix=100" },

        { "Night Swim", "UNDERGROUND",
          "engine=0;character=62;uwDepth=63;uwWater=66;uwMurk=44;uwPressure=40;uwRipple=26;uwWave=48;uwBubble=34;"
          "uwModShape=1;uwModPhase=120;fxOn=1;fxMode=4;fxSync=0;fxRate=0.3;fxFreq=300;fxDepth=60;fxReso=55;fxStereo=90;fxMix=45;mix=100" },

        { "Baby Voice", "UNDERGROUND",
          "engine=0;character=52;uwDepth=34;uwWater=48;uwMurk=20;uwPressure=45;uwRipple=22;uwWave=15;"
          "pitShift=5;pitMix=78;deAmount=65;eq6Gain=2;mix=100" },

        { "Demon Layer", "UNDERGROUND",
          "engine=0;character=64;uwDepth=52;uwWater=58;uwMurk=55;uwPressure=66;uwRipple=30;uwWave=20;"
          "macDamage=45;pitShift=-12;pitMix=46;compMode=4;compThresh=-26;mix=100" },

        { "Blown Out Water", "UNDERGROUND",
          "engine=0;character=70;uwDepth=44;uwWater=52;uwMurk=40;uwPressure=80;uwPressResp=85;uwRipple=30;uwWave=18;"
          "macDamage=75;compMode=4;compThresh=-22;compRatio=6;oversampling=2;mix=100" },

        { "Sunk Adlibs", "UNDERGROUND",
          "engine=0;character=66;uwDepth=60;uwWater=90;uwMurk=35;uwPressure=50;uwRipple=52;uwWave=34;uwBubble=22;"
          "uwModPhase=160;stWidth=155;pitShift=-12;pitMix=22;mix=100" },

        { "Warble Lead", "UNDERGROUND",
          "engine=0;character=55;uwDepth=38;uwWater=45;uwMurk=30;uwPressure=50;uwRipple=78;uwWave=12;"
          "modOn=1;modMode=3;modRate=35;modDepth=40;modMotion=55;modMix=35;mix=100" },

        // ------------------------------------------------------------- UNDERWATER
        { "Deep Dive", "UNDERWATER",
          "engine=0;character=58;uwDepth=62;uwWater=70;uwMurk=45;uwPressure=45;uwRipple=35;uwWave=30;uwBubble=15;"
          "uwResonance=1.4;compMode=1;compThresh=-20;mix=100" },

        { "Submerged", "UNDERWATER",
          "engine=0;character=52;uwDepth=48;uwWater=50;uwMurk=30;uwPressure=35;uwRipple=20;uwWave=22;uwBubble=10;"
          "uwMix=90;compMode=1;mix=100" },

        { "Ocean Voice", "UNDERWATER",
          "engine=0;character=56;uwDepth=50;uwWater=60;uwMurk=30;uwPressure=38;uwRipple=40;uwWave=55;uwBubble=20;"
          "uwModPhase=150;modOn=1;modMode=0;modRate=20;modDepth=35;modWidth=90;modMix=30;stWidth=140;mix=100" },

        { "Drowned Radio", "UNDERWATER",
          "engine=0;character=60;uwDepth=58;uwWater=45;uwMurk=55;uwPressure=60;uwRipple=18;uwWave=15;"
          "eq1Freq=320;eq1Slope=2;eq4Gain=4;modOn=1;modMode=4;modRate=55;modDepth=35;modMix=30;mix=100" },

        { "Dark Water", "UNDERWATER",
          "engine=0;character=60;uwDepth=70;uwWater=55;uwMurk=80;uwPressure=42;uwRipple=15;uwWave=18;"
          "uwResonance=0.9;macColor=25;macBody=64;mix=100" },

        { "Pressure", "UNDERWATER",
          "engine=0;character=64;uwDepth=55;uwWater=45;uwMurk=48;uwPressure=86;uwPressResp=80;uwRipple=14;uwWave=16;"
          "sigPunch=1;compMode=2;compThresh=-24;mix=100" },

        { "Abyss", "UNDERWATER",
          "engine=0;character=66;uwDepth=85;uwWater=88;uwMurk=60;uwPressure=50;uwRipple=30;uwWave=40;uwBubble=18;"
          "uwResonance=2.5;uwSlope=3;pitShift=-12;pitMix=35;macDepth=50;oversampling=2;mix=100" },

        { "Liquid Vocal", "UNDERWATER",
          "engine=0;character=54;uwDepth=40;uwWater=50;uwMurk=22;uwPressure=36;uwRipple=45;uwWave=28;uwBubble=55;"
          "uwModShape=1;modOn=1;modMode=1;modDetune=45;modMix=30;mix=100" },

        // --------------------------------------------------------- WOBBLE / WAH
        { "Auto Wah Funk", "WOBBLE",
          "engine=2;satMix=35;fxOn=1;fxMode=0;fxFreq=350;fxDepth=70;fxReso=68;fxSens=62;fxMix=100;compMode=1;mix=100" },

        { "Wobble 1/4", "WOBBLE",
          "engine=2;satMix=30;fxOn=1;fxMode=1;fxSync=1;fxDiv=4;fxFreq=250;fxDepth=78;fxReso=72;fxDrive=30;fxShape=0;"
          "fxMix=100;mix=100" },

        { "Wobble 1/8 Grit", "WOBBLE",
          "engine=1;dsDrive=28;dsType=0;dsMix=60;fxOn=1;fxMode=1;fxSync=1;fxDiv=7;fxFreq=280;fxDepth=72;fxReso=78;"
          "fxDrive=60;fxMix=100;mix=100" },

        { "Triplet Wub", "WOBBLE",
          "engine=2;satMix=30;fxOn=1;fxMode=1;fxSync=1;fxDiv=9;fxFreq=220;fxDepth=80;fxReso=82;fxDrive=35;fxShape=1;"
          "fxStereo=40;fxMix=100;mix=100" },

        { "Square Chop", "WOBBLE",
          "engine=2;satMix=25;fxOn=1;fxMode=1;fxSync=1;fxDiv=10;fxFreq=400;fxDepth=60;fxReso=60;fxShape=4;fxMix=100;mix=100" },

        { "LFO Wah Sweep", "WOBBLE",
          "engine=2;satMix=30;fxOn=1;fxMode=2;fxSync=1;fxDiv=1;fxFreq=420;fxDepth=65;fxReso=70;fxShape=1;fxMix=100;mix=100" },

        { "Wah Wah Adlib", "WOBBLE",
          "engine=2;satMix=30;fxOn=1;fxMode=2;fxSync=1;fxDiv=7;fxFreq=500;fxDepth=60;fxReso=66;fxStereo=60;fxMix=100;"
          "dlyOn=1;dlyMix=24;dlyDiv=8;dlyFeedback=40;dlyTone=35;dlyPing=1;dlyDuck=55;mix=100" },

        { "Talkbox Yoi", "WOBBLE",
          "engine=2;satMix=35;fxOn=1;fxMode=3;fxSync=1;fxDiv=4;fxFreq=700;fxDepth=90;fxReso=55;fxShape=0;fxMix=100;mix=100" },

        { "Robot Talk", "WOBBLE",
          "engine=1;dsDrive=20;dsType=9;dsMix=40;fxOn=1;fxMode=3;fxSync=1;fxDiv=10;fxFreq=760;fxDepth=100;fxReso=60;"
          "fxShape=5;fxMix=100;mix=100" },

        { "Phaser Drift", "WOBBLE",
          "engine=2;satMix=30;fxOn=1;fxMode=4;fxSync=0;fxRate=0.35;fxFreq=260;fxDepth=75;fxReso=65;fxStereo=90;fxMix=100;"
          "revOn=1;revMix=18;revDecay=1.6;revDamp=55;mix=100" },

        { "Underwater Wobble", "WOBBLE",
          "engine=0;character=55;uwDepth=45;uwWater=55;uwMurk=35;uwPressure=45;uwRipple=30;uwWave=10;"
          "fxOn=1;fxMode=1;fxSync=1;fxDiv=4;fxFreq=260;fxDepth=70;fxReso=70;fxDrive=25;fxMix=85;mix=100" },

        // ------------------------------------------------------------- DISTORTION
        { "Destroyed", "DISTORTION",
          "engine=1;character=70;dsDrive=80;dsBite=70;dsBody=40;dsCrush=55;dsEdge=80;dsSmooth=30;dsType=4;"
          "dsPostFilter=7000;macDamage=60;compMode=4;compRatio=6;oversampling=3;mix=92" },

        { "Radio Damage", "DISTORTION",
          "engine=1;character=62;dsDrive=58;dsBite=72;dsBody=30;dsCrush=30;dsEdge=60;dsSmooth=35;dsType=6;"
          "eq1Freq=380;eq1Slope=2;eq7Freq=3800;eq7Slope=2;macDamage=40;mix=100" },

        { "Aggressive", "DISTORTION",
          "engine=1;character=66;dsDrive=64;dsBite=72;dsBody=55;dsCrush=6;dsEdge=62;dsSmooth=38;dsType=1;"
          "compMode=4;compThresh=-24;compRatio=5;sigPunch=1;trOn=1;trAttack=40;mix=96" },

        { "Digital Rage", "DISTORTION",
          "engine=1;character=70;dsDrive=60;dsBite=62;dsBody=40;dsCrush=75;dsEdge=70;dsSmooth=22;dsType=9;"
          "dsPostFilter=9000;macDamage=55;sigChaos=1;oversampling=3;mix=88" },

        { "Broken Speaker", "DISTORTION",
          "engine=1;character=66;dsDrive=70;dsBite=48;dsBody=45;dsCrush=20;dsEdge=70;dsSmooth=34;dsType=5;"
          "dsAsym=36;dsBias=-24;eq7Freq=5600;macDamage=50;compMode=4;mix=88" },

        { "Dirty Vocal", "DISTORTION",
          "engine=1;character=55;dsDrive=42;dsBite=52;dsBody=58;dsEdge=34;dsSmooth=44;dsType=10;"
          "dsPreEmph=34;compMode=1;compThresh=-20;deAmount=55;mix=62" },

        { "Fuzz Voice", "DISTORTION",
          "engine=1;character=66;dsDrive=72;dsBite=62;dsBody=42;dsCrush=6;dsEdge=76;dsSmooth=40;dsType=4;"
          "dsPostFilter=7200;macDamage=50;mbOn=1;mbLow=-6;mbMid=5;mbHigh=-3;mix=84" },

        { "Industrial", "DISTORTION",
          "engine=1;character=70;dsDrive=74;dsBite=72;dsBody=38;dsCrush=48;dsEdge=76;dsSmooth=24;dsType=8;"
          "macDamage=60;compMode=4;compRatio=6;sigChaos=1;modOn=1;modMode=4;modRate=60;modDepth=30;modMix=25;"
          "oversampling=3;mix=90" },

        // ------------------------------------------------------------- SATURATION
        { "Warm Tape", "SATURATION",
          "engine=2;character=54;satDrive=40;satWarmth=58;satHarmonics=44;satThickness=52;satTone=-12;"
          "satSoftClip=38;satModel=0;compMode=3;compRatio=2.5;mix=100" },

        { "Analog Vocal", "SATURATION",
          "engine=2;character=52;satDrive=36;satWarmth=50;satHarmonics=42;satThickness=44;satTone=-4;"
          "satModel=1;satDensity=48;compMode=1;compThresh=-20;deAmount=48;mix=100" },

        { "Tube Glow", "SATURATION",
          "engine=2;character=60;satDrive=52;satWarmth=55;satHarmonics=64;satThickness=46;satTone=8;"
          "satSoftClip=44;satModel=1;satDensity=54;eq5Gain=2;sigAir=1;mix=100" },

        { "Vintage", "SATURATION",
          "engine=2;character=56;satDrive=46;satWarmth=68;satHarmonics=50;satThickness=58;satTone=-20;"
          "satModel=3;satDensity=56;eq2Gain=2;eq6Gain=-2;compMode=5;mix=100" },

        { "Thick Vocal", "SATURATION",
          "engine=2;character=60;satDrive=50;satWarmth=64;satHarmonics=56;satThickness=76;satTone=-8;"
          "satModel=3;satDensity=66;macBody=70;trOn=1;trBody=26;mix=100" },

        { "Console", "SATURATION",
          "engine=2;character=50;satDrive=32;satWarmth=50;satHarmonics=36;satThickness=40;satTone=6;"
          "satModel=2;satDensity=46;compMode=0;mix=100" },

        { "Warm Presence", "SATURATION",
          "engine=2;character=50;satDrive=34;satWarmth=46;satHarmonics=46;satThickness=38;satTone=24;"
          "satModel=4;eq5Gain=3;eq6Gain=2.5;deAmount=60;sigAir=1;mix=100" },

        { "Final Polish", "SATURATION",
          "engine=2;character=45;satDrive=22;satWarmth=36;satHarmonics=28;satThickness=26;satSoftClip=46;"
          "satModel=5;satDensity=38;compMode=1;compThresh=-18;compRatio=2.5;compAutoRel=1;ceiling=-0.5;mix=100" },

        // ------------------------------------------------------------------ VOCAL
        { "Lead Vocal Polish", "VOCAL",
          "engine=2;character=45;satDrive=24;satWarmth=40;satModel=4;compMode=1;compThresh=-22;compRatio=3;"
          "compAttack=8;compRelease=120;compAutoRel=1;deAmount=55;eq5Gain=2.5;eq6Gain=2;sigAir=1;"
          "trOn=1;trAttack=18;revOn=1;revMix=14;revSize=40;revDecay=1.3;revDamp=45;revDuck=40;mix=100" },

        { "Close And Dry", "VOCAL",
          "engine=2;character=40;satDrive=18;satModel=1;compMode=2;compThresh=-24;compRatio=4;compAttack=20;"
          "compRelease=90;eq1Freq=110;eq3Gain=-3;deAmount=50;trOn=1;trAttack=28;trBody=-14;mix=100" },

        { "Ad-Lib Doubler", "VOCAL",
          "engine=2;character=46;satDrive=28;satModel=0;modOn=1;modMode=1;modDetune=58;modWidth=85;modMix=45;"
          "stWidth=150;deAmount=52;dlyOn=1;dlyMix=18;dlyDiv=7;dlyFeedback=30;dlyTone=40;dlyPing=1;dlyDuck=50;mix=100" },

        { "Trap Ad-Lib", "VOCAL",
          "engine=1;character=58;dsDrive=48;dsBite=66;dsBody=40;dsCrush=20;dsEdge=55;dsSmooth=34;dsType=2;"
          "compMode=4;compThresh=-26;compRatio=5;stWidth=136;dlyOn=1;dlyMix=22;dlyDiv=8;dlyFeedback=38;dlyTone=35;"
          "dlyPing=1;dlyDuck=55;mix=85" },

        { "Clean Space", "VOCAL",
          "engine=2;character=42;satDrive=20;satModel=5;compMode=1;compThresh=-20;deAmount=55;"
          "revOn=1;revMix=24;revSize=60;revDecay=2.2;revDamp=50;revPre=30;revDuck=45;"
          "dlyOn=1;dlyMix=16;dlyDiv=4;dlyFeedback=32;dlyTone=45;dlyDuck=55;mix=100" },

        // ----------------------------------------------------------------- HYBRID
        { "Half Sunk Grit", "HYBRID",
          "engine=0;character=58;uwDepth=52;uwWater=50;uwMurk=42;uwPressure=55;uwMix=75;macDamage=45;"
          "macMotion=35;compMode=2;sigPunch=1;mbOn=1;mbLow=-4;mbMid=5;mbHigh=-2;mix=94" },

        { "Wide Whisper", "HYBRID",
          "engine=2;character=44;satDrive=22;satModel=4;macSpace=60;macMotion=30;stWidth=140;deAmount=60;"
          "eq6Gain=3;sigAir=1;modOn=1;modMode=0;modRate=16;modDepth=26;modWidth=88;modMix=26;mix=96" },

        { "Drowned Machine", "HYBRID",
          "engine=0;character=66;uwDepth=74;uwWater=82;uwMurk=65;uwPressure=70;uwRipple=50;uwWave=40;uwBubble=45;"
          "uwSlope=3;uwModShape=2;macDamage=50;sigChaos=1;modOn=1;modMode=4;modRate=60;modDepth=35;"
          "modMotion=60;modMix=30;oversampling=2;mix=100" },

        { "Parallel Damage", "HYBRID",
          "engine=1;character=70;dsDrive=82;dsBite=74;dsBody=34;dsCrush=50;dsEdge=78;dsSmooth=26;dsType=3;"
          "macDamage=65;compPlace=1;compMode=4;compRatio=6;wetLevel=-2;oversampling=3;mix=46" },

        { "Total Collapse", "EXTREME",
          "engine=1;character=85;dsDrive=90;dsBite=80;dsBody=28;dsCrush=70;dsEdge=86;dsSmooth=14;dsType=4;"
          "macDamage=85;compMode=4;compRatio=10;sigChaos=1;oversampling=3;mbOn=1;mbLow=-8;mbMid=8;mbHigh=-5;"
          "fxOn=1;fxMode=1;fxSync=1;fxDiv=10;fxDepth=50;fxReso=60;fxMix=50;mix=100" }
    };

    const FactoryPreset* factoryPresets() { return kPresets; }
    int numFactoryPresets() { return (int) (sizeof (kPresets) / sizeof (kPresets[0])); }
}
