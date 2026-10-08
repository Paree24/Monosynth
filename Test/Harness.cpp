// Monosynth headless tests: real processor + editor, 356 presets, DSP checks.
// Exit code 0 = all OK.
#include <cstdio>
#include <cmath>
#include <vector>
#include <map>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } \
    else { printf("ok: %s\n", msg); } } while (0)

static bool isFiniteBuffer(const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite(b.getSample(ch, i))) return false;
    return true;
}
static float peakAbs(const juce::AudioBuffer<float>& b)
{
    float p = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            p = juce::jmax(p, std::abs(b.getSample(ch, i)));
    return p;
}
static float rmsOf(const std::vector<float>& v)
{
    double s = 0.0; for (auto x : v) s += (double) x * x;
    return (float) std::sqrt(s / (double) juce::jmax<size_t>(1, v.size()));
}

int main(int, char**)
{
    {
        juce::File repoFactory;
        const char* cands[] = { "Factory", "../Factory", "../../Factory",
                                "../../../Factory", "Monosynth/Factory" };
        for (auto c : cands)
            if (juce::File(c).isDirectory()) { repoFactory = juce::File(c); break; }
        if (! repoFactory.isDirectory())
        {
            printf("FAIL: Factory presets not found (run from repo root or build dir)\n");
            return 1;
        }
        juce::File tmpOver = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("mono-harness-factory-override");
        tmpOver.deleteRecursively();
        setFactoryDirsForTest(repoFactory, tmpOver);
        juce::File tmpUser = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("mono-harness-user");
        tmpUser.deleteRecursively();
        setUserPresetDirForTest(tmpUser);
        printf("factory dir: %s\n", repoFactory.getFullPathName().toRawUTF8());
    }
    printf("--- init patch is neutral and static ---\n");
    {
        std::unique_ptr<MonoProcessor> pi(new MonoProcessor());
        pi->setRateAndBufferSizeDetails(44100, 512);
        pi->prepareToPlay(44100.0, 512);
        pi->loadInit();
        CHECK(pi->showInit, "init flag set");
        for (int r = 0; r < 8; ++r)
        {
            auto* s = pi->apvts.getRawParameterValue(P::modSrcId(r));
            auto* d = pi->apvts.getRawParameterValue(P::modDstId(r));
            auto* a = pi->apvts.getRawParameterValue(P::modAmtId(r));
            if (s == nullptr || d == nullptr || a == nullptr
                || (int)std::round(s->load()) != 0
                || (int)std::round(d->load()) != 0
                || std::abs(a->load()) > 1e-6f)
            { printf("FAIL: init route %d not empty\n", r); return 1; }
        }
        CHECK(std::abs(pi->apvts.getRawParameterValue("vco2tune")->load()) < 1e-6f,
              "init has no detune");
        // static render: sustained level must not wander (no hidden wobble)
        juce::AudioBuffer<float> bi(2, 512); juce::MidiBuffer mi;
        std::vector<float> sus;
        for (int k = 0; k < 172; ++k)
        {
            mi.clear();
            if (k == 0) mi.addEvent(juce::MidiMessage::noteOn(1, 45, 0.9f), 0);
            bi.clear(); pi->processBlock(bi, mi);
            if (! isFiniteBuffer(bi)) { printf("FAIL: non-finite init\n"); return 1; }
            if (k > 44) for (int i = 0; i < 512; ++i)
                sus.push_back(0.5f * (bi.getSample(0, i) + bi.getSample(1, i)));
        }
        size_t half = sus.size() / 2;
        double r1 = 0.0, r2 = 0.0;
        for (size_t i = 0; i < half; ++i) r1 += (double)sus[i] * sus[i];
        for (size_t i = half; i < sus.size(); ++i) r2 += (double)sus[i] * sus[i];
        r1 = std::sqrt(r1 / half); r2 = std::sqrt(r2 / (sus.size() - half));
        printf("init sustain rms %.4f -> %.4f\n", r1, r2);
        CHECK(r1 > 0.02, "init sounds");
        CHECK(std::abs(r1 - r2) / juce::jmax(1e-6, r1) < 0.02, "init is static (no wobble)");
    }

    printf("--- creating processor ---\n");
    std::unique_ptr<MonoProcessor> proc(new MonoProcessor());
    CHECK(proc->getNumPrograms() == 356, "356 programs");
    for (int i = 0; i < 356; ++i)
    {
        proc->setCurrentProgram(i);
        if (proc->getProgramName(i).isEmpty()) { printf("FAIL: preset %d empty name\n", i); return 1; }
    }
    printf("ok: all 356 presets apply + named\n");

    printf("--- determinism: reset-then-apply ---\n");
    proc->setCurrentProgram(193); // snare (noise high)
    float nzHigh = proc->apvts.getRawParameterValue("mixnoise")->load();
    printf("snare mixnoise = %f\n", nzHigh);
    CHECK(nzHigh > 0.5f, "drum preset sets noise");
    proc->setCurrentProgram(65); // acid (no noise)
    float nz0 = proc->apvts.getRawParameterValue("mixnoise")->load();
    CHECK(std::abs(nz0) < 1e-6f, "acid preset resets noise (no leak)");

    printf("--- prepare + render ---\n");
    const double sr = 44100.0; const int block = 512;
    proc->setRateAndBufferSizeDetails((int) sr, block);
    proc->prepareToPlay(sr, block);
    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;
    float maxPeak = 0.0f;
    for (int b = 0; b < 220; ++b)
    {
        midi.clear();
        if (b == 2) midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
        if (b == 100) midi.addEvent(juce::MidiMessage::noteOn(1, 52, 0.9f), 0);
        if (b == 180) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        buf.clear();
        proc->processBlock(buf, midi);
        if (! isFiniteBuffer(buf)) { printf("FAIL: non-finite at block %d\n", b); return 1; }
        if (b > 4 && b < 170) maxPeak = juce::jmax(maxPeak, peakAbs(buf));
    }
    printf("ok: render finite, peak=%.3f\n", maxPeak);
    CHECK(maxPeak > 0.02f, "synth produces sound");

    printf("--- staccato re-articulation ---\n");
    {
        std::unique_ptr<MonoProcessor> p5(new MonoProcessor());
        p5->setRateAndBufferSizeDetails(44100, 512);
        p5->prepareToPlay(44100.0, 512);
        p5->setCurrentProgram(64); // acid
        juce::AudioBuffer<float> b5(2, 512); juce::MidiBuffer m5;
        m5.addEvent(juce::MidiMessage::noteOn(1, 40, 0.9f), 0);
        p5->processBlock(b5, m5);
        float secondPeak = 0.0f;
        for (int k = 0; k < 30; ++k)
        {
            m5.clear();
            if (k == 2) m5.addEvent(juce::MidiMessage::noteOff(1, 40), 0);
            if (k == 4) m5.addEvent(juce::MidiMessage::noteOn(1, 45, 0.9f), 0);
            if (k == 20) m5.addEvent(juce::MidiMessage::noteOff(1, 45), 0);
            b5.clear(); p5->processBlock(b5, m5);
            if (! isFiniteBuffer(b5)) { printf("FAIL: non-finite staccato\n"); return 1; }
            if (k >= 4 && k < 12) secondPeak = juce::jmax(secondPeak, peakAbs(b5));
        }
        printf("second note peak = %f\n", secondPeak);
        CHECK(secondPeak > 0.05f, "staccato re-articulates");
    }

    printf("--- oscillator waveshape (scope test) ---\n");
    {
        // Dry solo path: init is already single-osc, open filter, no drive/
        // thick/sat, full sustain. Render A4, slice cycles at the edge,
        // average 100+ cycles, compare against the ideal shape.
        std::unique_ptr<MonoProcessor> po(new MonoProcessor());
        po->setRateAndBufferSizeDetails(44100, 512);
        po->prepareToPlay(44100.0, 512);
        auto setF = [&](const char* id, float v) {
            if (auto* prm = po->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        auto setC = [&](const char* id, float idx) {
            if (auto* prm = po->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(idx));
        };
        auto avgCycle = [&](int wave, float pw) {
            po->loadInit();
            setC("vco1wave", (float)wave);
            setF("vco1pw", pw);
            juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
            std::vector<float> mono;
            for (int k = 0; k < 86; ++k)
            {
                m.clear();
                if (k == 0) m.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
                b.clear(); po->processBlock(b, m);
                if (k >= 43) for (int i = 0; i < 512; ++i)
                    mono.push_back(0.5f * (b.getSample(0, i) + b.getSample(1, i)));
            }
            float peak = 0.0f;
            for (auto x : mono) peak = juce::jmax(peak, std::abs(x));
            std::vector<int> edges;
            bool sharp = (wave != 1 && wave != 4); // saw/square/pulse have edges
            for (size_t i = 1; i < mono.size(); ++i)
            {
                if (sharp) { if (mono[i] - mono[i - 1] < -0.25f * peak) edges.push_back((int)i); }
                else if (mono[i - 1] < 0.0f && mono[i] >= 0.0f) edges.push_back((int)i);
            }
            std::vector<float> avg(128, 0.0f);
            int used = 0;
            for (size_t e = 0; e + 1 < edges.size() && used < 120; ++e)
            {
                int a = edges[e], len = edges[e + 1] - a;
                if (len < 40 || len > 400) continue;
                for (int j = 0; j < 128; ++j)
                {
                    float pos = (float)a + (float)j * (float)len / 128.0f;
                    int i0 = (int)pos;
                    float fr = pos - (float)i0;
                    float s = mono[(size_t)i0] * (1.0f - fr) + mono[(size_t)i0 + 1] * fr;
                    avg[(size_t)j] += s;
                }
                ++used;
            }
            if (used < 20) return std::vector<float>();
            float mx = 1e-9f;
            for (auto& x : avg) { x /= (float)used; mx = juce::jmax(mx, std::abs(x)); }
            for (auto& x : avg) x /= mx;
            return avg;
        };
        auto saw = avgCycle(0, 0.5f);
        CHECK(!saw.empty(), "saw cycles found");
        {
            double dev = 0.0;
            for (int j = 16; j <= 112; ++j)
                dev = juce::jmax(dev, (double)std::abs(saw[(size_t)j] - (-1.0f + 2.0f * (float)j / 127.0f)));
            printf("saw ramp deviation = %.4f\n", dev);
            // edges sharp, body straight: residual is filter/DC-block phase
            CHECK(dev < 0.09, "saw is a saw");
        }
        auto sqr = avgCycle(2, 0.5f);
        CHECK(!sqr.empty(), "square cycles found");
        {
            double top = 0.0, bot = 0.0, ft = 0.0, fb = 0.0;
            for (int j = 72; j <= 120; ++j) { top += sqr[(size_t)j]; ++ft; }
            for (int j = 8; j <= 56; ++j) { bot += sqr[(size_t)j]; ++fb; }
            top /= ft; bot /= fb;
            double vt = 0.0, vb = 0.0;
            for (int j = 72; j <= 120; ++j) vt += (sqr[(size_t)j] - top) * (sqr[(size_t)j] - top);
            for (int j = 8; j <= 56; ++j) vb += (sqr[(size_t)j] - bot) * (sqr[(size_t)j] - bot);
            vt = std::sqrt(vt / ft); vb = std::sqrt(vb / fb);
            printf("square top=%.3f bot=%.3f flat=%.4f/%.4f\n", top, bot, vt, vb);
            CHECK(top - bot > 1.5, "square has two levels");
            CHECK(std::abs(top + bot) < 0.1, "square is symmetric");
            CHECK(vt < 0.06 && vb < 0.06, "square is flat");
        }
        auto pls = avgCycle(3, 0.25f);
        CHECK(!pls.empty(), "pulse cycles found");
        {
            int hi = 0;
            for (auto x : pls) if (x > 0.0f) ++hi;
            double duty = (double)hi / (double)pls.size();
            printf("pulse duty = %.3f\n", duty);
            CHECK(std::abs(duty - 0.25) < 0.04, "pulse width is honest");
        }
        auto tri = avgCycle(1, 0.5f);
        CHECK(!tri.empty(), "triangle cycles found");
        {
            double mx = -1e9, mn = 1e9, num = 0.0, d1 = 0.0, d2 = 0.0;
            for (int j = 0; j < 128; ++j)
            {
                float p = (float)j / 128.0f;
                float ideal = p < 0.25f ? p * 4.0f : (p < 0.75f ? 2.0f - p * 4.0f : p * 4.0f - 4.0f);
                mx = juce::jmax(mx, (double)tri[(size_t)j]);
                mn = juce::jmin(mn, (double)tri[(size_t)j]);
                num += tri[(size_t)j] * ideal; d1 += tri[(size_t)j] * tri[(size_t)j]; d2 += ideal * ideal;
            }
            double corr = num / std::sqrt(d1 * d2);
            printf("triangle symmetry=%.4f corr=%.5f\n", mx + mn, corr);
            CHECK(std::abs(mx + mn) < 0.08, "triangle is symmetric");
            CHECK(corr > 0.998, "triangle is a triangle");
        }
        auto sin = avgCycle(4, 0.5f);
        CHECK(!sin.empty(), "sine cycles found");
        {
            double s2 = 0.0, pk = 0.0;
            for (auto x : sin) { s2 += (double)x * x; pk = juce::jmax(pk, (double)std::abs(x)); }
            double crest = pk / std::sqrt(s2 / (double)sin.size());
            printf("sine crest = %.4f\n", crest);
            CHECK(std::abs(crest - 1.41421) < 0.03, "sine is a sine");
        }
    }

    printf("--- tuning: A4 must read ~440 ---\n");
    {
        std::unique_ptr<MonoProcessor> tp(new MonoProcessor());
        tp->setRateAndBufferSizeDetails(44100, 512);
        tp->prepareToPlay(44100.0, 512);
        tp->loadInit();
        juce::AudioBuffer<float> tb(2, 512); juce::MidiBuffer tm;
        std::vector<float> steady;
        for (int k = 0; k < 86; ++k)
        {
            tm.clear();
            if (k == 0) tm.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
            tb.clear(); tp->processBlock(tb, tm);
            if (! isFiniteBuffer(tb)) { printf("FAIL: non-finite tuning\n"); return 1; }
            if (k >= 30 && k < 80)
                for (int i = 0; i < 512; ++i) steady.push_back(tb.getSample(0, i));
        }
        const int N = 16384;
        juce::dsp::FFT fft(14);
        std::vector<float> fbuf(2 * N, 0.0f);
        for (int i = 0; i < N && i < (int) steady.size(); ++i)
        {
            float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float) i / (float) N);
            fbuf[(size_t) i] = steady[(size_t) i] * w;
        }
        fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
        float expBin = 440.0f * (float) N / 44100.0f;
        int lo = juce::jmax(2, (int) (expBin * 0.97f)), hi = (int) (expBin * 1.03f) + 1;
        int pk = lo; float pv = 0.0f;
        for (int b = lo; b <= hi && b < N / 2; ++b) if (fbuf[(size_t) b] > pv) { pv = fbuf[(size_t) b]; pk = b; }
        float meas = (float) pk * 44100.0f / (float) N;
        float err = std::abs(1200.0f * std::log2(meas / 440.0f));
        printf("A4 reads %.1f Hz (err %.0f cents)\n", meas, err);
        CHECK(err < 20.0f, "concert pitch in tune");
    }

    printf("--- filter: response + stability ---\n");
    {
        MonoDSP::CascadeFilter f;
        f.prepare(44100.0);
        // LP: 100 Hz tone passes, 8 kHz tone rejected (cutoff 2 kHz, low res)
        auto toneGain = [&](bool hp, float cut, float sig) {
            MonoDSP::CascadeFilter ff; ff.prepare(44100.0);
            std::vector<float> out;
            for (int i = 0; i < 44100; ++i)
            {
                float x = std::sin(2.0f * 3.14159265f * sig * (float) i / 44100.0f) * 0.5f;
                float y = hp ? ff.processHP(x, cut, 0.0f) : ff.processLP(x, cut, 0.05f);
                if (i > 22050) out.push_back(y);
            }
            return rmsOf(out);
        };
        float lpLow = toneGain(false, 2000.0f, 100.0f);
        float lpHigh = toneGain(false, 2000.0f, 8000.0f);
        float hpLow = toneGain(true, 2000.0f, 100.0f);
        float hpHigh = toneGain(true, 2000.0f, 8000.0f);
        printf("LP 100Hz=%.3f 8kHz=%.3f | HP 100Hz=%.3f 8kHz=%.3f\n", lpLow, lpHigh, hpLow, hpHigh);
        CHECK(lpLow > lpHigh * 4.0f, "lowpass rejects highs");
        CHECK(hpHigh > hpLow * 4.0f, "highpass rejects lows");
        // signature: resonance peak sings at cutoff...
        auto atCut = [&](int model, float res) {
            MonoDSP::CascadeFilter ff; ff.prepare(44100.0); ff.setModel(model);
            std::vector<float> out;
            for (int i = 0; i < 44100; ++i)
            {
                float x = std::sin(2.0f * 3.14159265f * 1000.0f * (float) i / 44100.0f) * 0.4f;
                float y = ff.process(x, 10.0f, 0.0f, 1000.0f, res, 0.0f);
                if (i > 22050) out.push_back(y);
            }
            return rmsOf(out);
        };
        float pk0 = atCut(0, 0.0f), pk1 = atCut(0, 1.0f);
        printf("res peak @cutoff: res0=%.3f res1=%.3f\n", pk0, pk1);
        CHECK(pk1 > pk0 * 3.0f, "resonance peak screams at cutoff");
        // ...while bass drops out as resonance rises (strong Early, mild Late)
        auto bassAt = [&](int model, float res) {
            MonoDSP::CascadeFilter ff; ff.prepare(44100.0); ff.setModel(model);
            std::vector<float> out;
            for (int i = 0; i < 44100; ++i)
            {
                float x = std::sin(2.0f * 3.14159265f * 100.0f * (float) i / 44100.0f) * 0.4f;
                float y = ff.process(x, 10.0f, 0.0f, 800.0f, res, 0.0f);
                if (i > 22050) out.push_back(y);
            }
            return rmsOf(out);
        };
        float eBass = bassAt(0, 1.0f) / juce::jmax(1e-6f, bassAt(0, 0.0f));
        float lBass = bassAt(1, 1.0f) / juce::jmax(1e-6f, bassAt(1, 0.0f));
        printf("bass retention @max res: Early=%.2f Late=%.2f\n", eBass, lBass);
        CHECK(eBass < 0.75f, "Early loses bass with resonance");
        CHECK(lBass > eBass, "Late keeps bass better than Early");
        // self-oscillation: pinged Early at max res must still whistle 2 s later
        {
            MonoDSP::CascadeFilter ff; ff.prepare(44100.0); ff.setModel(0);
            float peak = 0.0f;
            std::vector<float> tail;
            for (int i = 0; i < 44100 * 2; ++i)
            {
                float x = (i == 100) ? 1.0f : 0.0f;
                float y = ff.process(x, 10.0f, 0.0f, 800.0f, 1.0f, 0.0f);
                if (! std::isfinite(y)) { printf("FAIL: ping non-finite\n"); return 1; }
                peak = juce::jmax(peak, std::abs(y));
                if (i > 44100 * 2 - 11025) tail.push_back(y);
            }
            float tailRms = rmsOf(tail);
            printf("ping: peak=%.3f tail-rms=%.3f\n", peak, tailRms);
            CHECK(peak < 8.0f, "ping bounded");
            CHECK(tailRms > 0.02f, "filter self-oscillates");
        }
        // stability torture: max res + max drive + sweeping cutoff, both models
        for (int m = 0; m < 2; ++m)
        {
            MonoDSP::CascadeFilter ff; ff.prepare(44100.0); ff.setModel(m);
            juce::Random rr(7);
            for (int i = 0; i < 44100 * 2; ++i)
            {
                float cut = 50.0f + 18000.0f * (0.5f + 0.5f * std::sin((float) i * 0.0002f));
                float x = (rr.nextFloat() * 2.0f - 1.0f) * 0.8f;
                float y = ff.process(x, cut, 1.0f, cut, 1.0f, 1.0f);
                if (! std::isfinite(y) || std::abs(y) > 8.0f)
                { printf("FAIL: filter unstable model %d @ %d\n", m, i); return 1; }
            }
        }
        printf("ok: filter stable at extremes, both models\n");
    }

    printf("--- saturation: all modes finite + audible ---\n");
    {
        for (int m = 0; m < 4; ++m)
        {
            MonoDSP::Saturation s;
            float peak = 0.0f;
            juce::Random rr(3);
            for (int i = 0; i < 44100; ++i)
            {
                float x = (rr.nextFloat() * 2.0f - 1.0f) * 1.5f;
                float y = s.process(x, 1.0f, m);
                if (! std::isfinite(y)) { printf("FAIL: sat mode %d non-finite\n", m); return 1; }
                peak = juce::jmax(peak, std::abs(y));
            }
            printf("sat mode %d peak=%.3f\n", m, peak);
            if (peak < 0.1f || peak > 1.5f) { printf("FAIL: sat mode %d level wrong\n", m); return 1; }
        }
        printf("ok: saturation modes behave\n");
    }

    printf("--- no pitch tampering in data files (except kick/toms/zap) ---\n");
    {
        // parse each factory file directly: routes into VCO pitch and any
        // VCO2 detune are forbidden outside the drum pitch-drop recipes
        auto& all = getFactoryPresets();
        CHECK((int)all.size() == 356, "356 data files scanned");
        for (auto& pr : all)
        {
            bool isDrop = pr.index >= 192 && pr.index < 224
                && (pr.index % 8 == 0 || pr.index % 8 == 4
                    || pr.index % 8 == 5 || pr.index % 8 == 7);
            auto xml = juce::parseXML(pr.file);
            if (xml == nullptr) { printf("FAIL: unparsable %d\n", pr.index); return 1; }
            auto vt = juce::ValueTree::fromXml(*xml);
            if (!vt.isValid()) { printf("FAIL: bad tree %d\n", pr.index); return 1; }
            std::map<juce::String, float> m;
            for (int i = 0; i < vt.getNumChildren(); ++i)
            {
                auto c = vt.getChild(i);
                if (c.hasType("PARAM"))
                    m[c.getProperty("id").toString()] = (float)c.getProperty("value", 0.0);
            }
            for (int r = 0; r < 8; ++r)
            {
                juce::String did = "mod" + juce::String(r) + "dst";
                juce::String aid = "mod" + juce::String(r) + "amt";
                if (m.find(did) == m.end() || m.find(aid) == m.end())
                { printf("FAIL: preset %d missing route %d\n", pr.index, r); return 1; }
                int dst = (int)std::round(m[did]);
                if ((dst == 1 || dst == 2) && std::abs(m[aid]) > 1e-6f && !isDrop)
                { printf("FAIL: preset %d route %d targets pitch\n", pr.index, r); return 1; }
            }
            if (m.find("vco2tune") == m.end() || std::abs(m["vco2tune"]) > 1e-6f)
            { printf("FAIL: preset %d detuned\n", pr.index); return 1; }
            if (m.find("thick") == m.end() || m.find("limiter") == m.end())
            { printf("FAIL: preset %d missing thick/limiter\n", pr.index); return 1; }
            if (pr.index >= 256)
            {
                bool fx = m["chon"] > 0.5f || m["dlon"] > 0.5f || m["rvon"] > 0.5f;
                if (!fx) { printf("FAIL: preset %d has no FX\n", pr.index); return 1; }
            }
        }
        printf("ok: pitch untouched everywhere except kick/toms/zap\n");
    }

    printf("--- thickness: transparent at 0, finite across range ---\n");
    {
        std::unique_ptr<MonoProcessor> pt(new MonoProcessor());
        pt->setRateAndBufferSizeDetails(44100, 512);
        pt->prepareToPlay(44100.0, 512);
        pt->loadInit();
        auto setF = [&](const char* id, float v) {
            if (auto* prm = pt->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        juce::AudioBuffer<float> b1(2, 512), b2(2, 512);
        juce::MidiBuffer mm;
        auto renderNote = [&](juce::AudioBuffer<float>& b) {
            float peak = 0.0f;
            for (int k = 0; k < 60; ++k)
            {
                mm.clear();
                if (k == 0) mm.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
                b.clear(); pt->processBlock(b, mm);
                if (! isFiniteBuffer(b)) return -1.0f;
                if (k > 4) peak = juce::jmax(peak, peakAbs(b));
            }
            return peak;
        };
        setF("thick", 0.0f);
        float p0 = renderNote(b1);
        setF("thick", 1.0f);
        float p1 = renderNote(b2);
        printf("thick 0 peak=%.3f thick 1 peak=%.3f\n", p0, p1);
        CHECK(p0 > 0.02f && p1 > 0.02f, "thickness range sounds");
        CHECK(p1 > p0 * 0.5f && p1 < p0 * 3.0f + 0.3f, "thickness stays controlled");
    }

    printf("--- limiter toggle ---\n");
    {
        std::unique_ptr<MonoProcessor> pl(new MonoProcessor());
        pl->setRateAndBufferSizeDetails(44100, 512);
        pl->prepareToPlay(44100.0, 512);
        pl->setCurrentProgram(80); // hot acid
        auto setB = [&](const char* id, bool v) {
            if (auto* prm = pl->apvts.getParameter(id))
                prm->setValueNotifyingHost(v ? 1.0f : 0.0f);
        };
        auto renderPeak = [&]() {
            juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
            float peak = 0.0f;
            for (int k = 0; k < 80; ++k)
            {
                m.clear();
                if (k == 0) m.addEvent(juce::MidiMessage::noteOn(1, 40, 1.0f), 0);
                b.clear(); pl->processBlock(b, m);
                if (! isFiniteBuffer(b)) return -1.0f;
                if (k > 4) peak = juce::jmax(peak, peakAbs(b));
            }
            return peak;
        };
        setB("limiter", true);
        float on = renderPeak();
        setB("limiter", false);
        float off = renderPeak();
        printf("limiter on=%.3f off=%.3f\n", on, off);
        CHECK(on > 0.02f && off > 0.02f, "both limiter states sound");
        CHECK(off + 0.05f >= on, "limiter only contains peaks");
    }

    printf("--- master FX: finite, audible, stereo ---\n");
    {
        std::unique_ptr<MonoProcessor> pf(new MonoProcessor());
        pf->setRateAndBufferSizeDetails(44100, 512);
        pf->prepareToPlay(44100.0, 512);
        pf->loadInit();
        auto setF = [&](const char* id, float v) {
            if (auto* prm = pf->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        auto setB = [&](const char* id, bool v) {
            if (auto* prm = pf->apvts.getParameter(id))
                prm->setValueNotifyingHost(v ? 1.0f : 0.0f);
        };
        setB("chon", true); setF("chrate", 0.8f); setF("chdepth", 0.6f);
        setB("dlon", true); setF("dltime", 300.0f); setF("dlfb", 0.4f); setF("dlmix", 0.35f);
        setB("rvon", true); setF("rvsize", 0.6f); setF("rvmix", 0.3f);
        juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m;
        float peak = 0.0f, diff = 0.0f;
        for (int k = 0; k < 100; ++k)
        {
            m.clear();
            if (k == 0) m.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
            if (k == 70) m.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
            b.clear(); pf->processBlock(b, m);
            if (! isFiniteBuffer(b)) { printf("FAIL: non-finite FX\n"); return 1; }
            if (k > 4)
            {
                peak = juce::jmax(peak, peakAbs(b));
                for (int i = 0; i < 512; ++i)
                    diff = juce::jmax(diff, std::abs(b.getSample(0, i) - b.getSample(1, i)));
            }
        }
        printf("fx peak=%.3f stereo-diff=%.4f\n", peak, diff);
        CHECK(peak > 0.02f, "FX chain sounds");
        CHECK(diff > 1e-4f, "FX chain is stereo");
    }

    printf("--- user preset save/load + factory shadow round-trip ---\n");
    {
        std::unique_ptr<MonoProcessor> pu(new MonoProcessor());
        pu->setRateAndBufferSizeDetails(44100, 512);
        pu->prepareToPlay(44100.0, 512);
        CHECK(scanUserPresets().empty(), "user bank starts empty");
        pu->setCurrentProgram(80);
        CHECK(saveUserPreset("Harness Test!", pu->apvts), "save user preset");
        CHECK(saveUserPreset("Second One", pu->apvts), "save second preset");
        auto found = scanUserPresets();
        CHECK(found.size() == 2, "scan finds both");
        CHECK(found[0].name == "Harness Test", "name sanitised");
        pu->setCurrentProgram(0);
        CHECK(loadUserPreset(found[0].file, pu->apvts), "load user preset");
        // shadow a factory preset, verify, then restore the shipped file
        pu->setCurrentProgram(10);
        CHECK(overwriteFactoryPreset(10, pu->apvts), "overwrite factory (shadow)");
        {
            bool seen = false;
            for (auto& pr : getFactoryPresets())
                if (pr.index == 10) seen = pr.shadowed;
            CHECK(seen, "shadow flagged");
        }
        CHECK(deleteFactoryShadow(10), "delete restores shipped file");
        {
            bool seen = false;
            for (auto& pr : getFactoryPresets())
                if (pr.index == 10) seen = !pr.shadowed;
            CHECK(seen, "shipped file restored");
        }
        CHECK(!loadUserPreset(getUserPresetDir().getChildFile("nope.xml"), pu->apvts),
              "missing file fails clean");
        CHECK(deleteUserPreset(found[0].file), "delete user preset works");
        CHECK(scanUserPresets().size() == 1, "one remains");
        CHECK(!deleteUserPreset(juce::File("/tmp/mono-harness-evil.xml")), "delete confined");
        printf("ok: user preset round-trip\n");
    }

    printf("--- sub-20Hz + DC (C3, the bass playing range) ---\n");
    {
        std::unique_ptr<MonoProcessor> p8(new MonoProcessor());
        p8->setRateAndBufferSizeDetails(44100, 512);
        p8->prepareToPlay(44100.0, 512);
        p8->setCurrentProgram(0);
        juce::AudioBuffer<float> b8(2, 512); juce::MidiBuffer m8;
        std::vector<float> steady;
        for (int k = 0; k < 172; ++k)
        {
            m8.clear();
            if (k == 0) m8.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
            b8.clear(); p8->processBlock(b8, m8);
            if (! isFiniteBuffer(b8)) { printf("FAIL: non-finite sub test\n"); return 1; }
            if (k > 25) for (int i = 0; i < 512; ++i)
                steady.push_back(0.5f * (b8.getSample(0, i) + b8.getSample(1, i)));
        }
        const int N = 16384;
        juce::dsp::FFT fft(14);
        std::vector<float> fbuf(2 * N, 0.0f);
        for (int i = 0; i < N && i < (int) steady.size(); ++i)
        {
            float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float) i / (float) N);
            fbuf[(size_t) i] = steady[(size_t) i] * w;
        }
        fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
        double dc = 0.0; for (auto x : steady) dc += x; dc /= (double) steady.size();
        float fund = 0.0f, rumble = 0.0f;
        for (int b = 0; b < N / 2; ++b)
        {
            float fr = (float) b * 44100.0f / (float) N;
            if (fr > 25.0f && fr < 160.0f) fund = juce::jmax(fund, fbuf[(size_t) b]);
            if (fr >= 8.0f && fr < 20.0f) rumble = juce::jmax(rumble, fbuf[(size_t) b]);
        }
        double rumbleDb = 20.0 * std::log10(rumble / juce::jmax(fund, 1e-6f));
        printf("rumble %.1f dB, DC %.5f\n", rumbleDb, dc);
        CHECK(rumbleDb < -40.0, "no subsonic rumble");
        // DC threshold allows slow inter-osc beating (finite-window mean of
        // beating is nonzero but inaudible/harmless); static offsets read 10x this.
        CHECK(std::abs(dc) < 0.02, "no DC offset");
    }

    printf("--- state round-trip ---\n");
    {
        proc->setCurrentProgram(100);
        juce::MemoryBlock mb;
        proc->getStateInformation(mb);
        proc->setCurrentProgram(0);
        proc->setStateInformation(mb.getData(), (int) mb.getSize());
        CHECK(proc->getCurrentProgram() == 100, "program restored");
    }

    printf("--- all presets render finite (notes + chord) ---\n");
    {
        float worst = 0.0f; int worstIdx = -1;
        int nprog = proc->getNumPrograms();
        for (int pi = 0; pi < nprog; ++pi)
        {
            std::unique_ptr<MonoProcessor> pq(new MonoProcessor());
            pq->setRateAndBufferSizeDetails(44100, 512);
            pq->prepareToPlay(44100.0, 512);
            pq->setCurrentProgram(pi);
            juce::AudioBuffer<float> bq(2, 512); juce::MidiBuffer mq;
            float peak = 0.0f;
            for (int k = 0; k < 100; ++k)
            {
                mq.clear();
                if (k == 0) { mq.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
                              mq.addEvent(juce::MidiMessage::noteOn(1, 55, 0.9f), 5); }
                if (k == 50) mq.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
                bq.clear(); pq->processBlock(bq, mq);
                if (! isFiniteBuffer(bq)) { printf("FAIL: non-finite preset %d\n", pi); return 1; }
                if (k > 2) peak = juce::jmax(peak, peakAbs(bq));
            }
            if (peak > worst) { worst = peak; worstIdx = pi; }
            if (peak > 1.2f) printf("  HOT preset %d peak %.3f\n", pi, peak);
        }
        printf("bank sweep done, worst = preset %d peak %.3f\n", worstIdx, worst);
        CHECK(worst < 1.2f, "limiter contains every preset");
    }

    printf("--- editor create/paint ---\n");
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc->createEditor());
        CHECK(ed != nullptr, "editor created");
        ed->setSize(MonoContent::baseW, MonoContent::baseH);
        juce::Image img(juce::Image::RGB, MonoContent::baseW, MonoContent::baseH, true);
        { juce::Graphics g(img); ed->paintEntireComponent(g, false); }
        juce::File png("/tmp/monosynth-ui.png");
        png.deleteFile();
        { juce::FileOutputStream fos(png); juce::PNGImageFormat fmt; fmt.writeImageToStream(img, fos); }
        printf("screenshot: %s\n", png.getFullPathName().toRawUTF8());
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
