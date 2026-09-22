#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <chrono>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    OmgnedProcessor p;
    const double sr = 48000.0; const int block = 256;
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);

    juce::AudioBuffer<float> b (2, block);
    juce::MidiBuffer m;
    juce::Random r (7);

    auto setP = [&p] (const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); };

    struct Case { const char* name; int engine; int os; int mod; int mb; };
    const Case cases[] = {
        { "underwater, no oversampling", 0, 0, 0, 0 },
        { "underwater, 2x",              0, 1, 0, 0 },
        { "distortion, 2x",              1, 1, 0, 0 },
        { "distortion, 4x",              1, 2, 0, 0 },
        { "distortion, 8x",              1, 3, 0, 0 },
        { "saturation, 2x",              2, 1, 0, 0 },
        { "distortion, 4x + mod + mb",   1, 2, 1, 1 },
        { "everything on, 8x",           1, 3, 1, 1 },
    };

    for (const auto& c : cases)
    {
        setP (omg::pid::engine, (float) c.engine);
        setP (omg::pid::oversampling, (float) c.os);
        setP (omg::pid::modOn, (float) c.mod);
        setP (omg::pid::mbOn, (float) c.mb);
        setP (omg::pid::mbLow, c.mb ? -6.0f : 0.0f);
        setP (omg::pid::mbMid, c.mb ? 6.0f : 0.0f);
        setP (omg::pid::character, 80.0f);
        setP (omg::pid::trOn, (float) c.mod);
        setP (omg::pid::trAttack, c.mod ? 50.0f : 0.0f);

        for (int n = 0; n < 40; ++n) { for (int ch=0; ch<2; ++ch) for (int i=0;i<block;++i) b.setSample(ch,i,(r.nextFloat()*2-1)*0.3f); p.processBlock (b, m); }

        const int iterations = 4000;
        auto t0 = std::chrono::steady_clock::now();
        for (int n = 0; n < iterations; ++n)
        {
            for (int ch=0; ch<2; ++ch) for (int i=0;i<block;++i) b.setSample(ch,i,(r.nextFloat()*2-1)*0.3f);
            p.processBlock (b, m);
        }
        auto t1 = std::chrono::steady_clock::now();

        const double seconds = std::chrono::duration<double> (t1 - t0).count();
        const double audioSeconds = iterations * block / sr;
        printf ("%-30s  %6.2f %% of one core\n", c.name, 100.0 * seconds / audioSeconds);
    }
    return 0;
}
