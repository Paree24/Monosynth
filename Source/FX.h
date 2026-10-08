#pragma once
// Monosynth master effects: chorus, stereo delay, modulated Schroeder reverb.
// Adapted from our own ERSA engine (same author/project family): identical
// algorithms and smoothing behavior, renamed into the MonoFX namespace.
// All processing is linear, allocation-free and finite-safe.
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace MonoFX
{

class ChorusFX
{
public:
    void prepare(double sr, int maxBlock)
    {
        sampleRate = sr;
        int maxLen = (int)(sr * 0.05) + 16;
        bufL.assign(maxLen, 0.0f); bufR.assign(maxLen, 0.0f);
        len = maxLen; pos = 0;
        lfoPhase = 0.0;
        wlpL = wlpR = 0.0f;
        smBase = 12.0f; smDepth = 3.0f; smRate = 0.6f;
        juce::ignoreUnused(maxBlock);
    }
    void process(float* L, float* R, int n, float rate, float depth, float mix)
    {
        if (mix <= 0.001f) return;
        float baseMs = 8.0f, depthMs = 4.0f * depth; // subtle, minimal comb color
        // glide delay parameters (~30 ms): knob jumps morph instead of zapping
        float k = 1.0f - std::exp(-1.0f / (0.030f * (float)sampleRate));
        // gentle BBD-style darkening on the wet path only
        float wg = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 7000.0f / (float)sampleRate);
        for (int i = 0; i < n; ++i)
        {
            smBase += (baseMs - smBase) * k;
            smDepth += (depthMs - smDepth) * k;
            smRate += (rate - smRate) * k;
            lfoPhase += smRate / sampleRate; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            float ph = (float)lfoPhase * 2.0f * (float)juce::MathConstants<double>::pi;
            float lfoL = std::sin(ph);
            float lfoR = -lfoL; // antiphase keeps it classic and mono-safe
            bufL[pos] = L[i]; bufR[pos] = R[i];
            float wetL = readDelay(bufL, smBase + smDepth * lfoL);
            float wetR = readDelay(bufR, smBase + smDepth * lfoR);
            wlpL += wg * (wetL - wlpL); wlpR += wg * (wetR - wlpR);
            L[i] = L[i] * (1.0f - mix * 0.5f) + wlpL * mix * 0.5f;
            R[i] = R[i] * (1.0f - mix * 0.5f) + wlpR * mix * 0.5f;
            pos = (pos + 1) % len;
        }
    }
    void clear()
    {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        wlpL = wlpR = 0.0f;
    }
private:
    float readDelay(const std::vector<float>& buf, float ms)
    {
        float samples = (float)(ms * sampleRate / 1000.0);
        float rp = (float)pos - samples;
        while (rp < 0) rp += len;
        int i0 = (int)rp; float frac = rp - i0;
        int i1 = (i0 + 1) % len;
        return buf[(size_t)i0] * (1 - frac) + buf[(size_t)i1] * frac;
    }
    double sampleRate = 44100.0; std::vector<float> bufL, bufR; int len = 1024, pos = 0; double lfoPhase = 0.0;
    float wlpL = 0.0f, wlpR = 0.0f;
    float smBase = 12.0f, smDepth = 3.0f, smRate = 0.6f;
};

// Modulated Schroeder reverb: 8 modulated comb filters (stereo pairs) into
// 2+2 allpasses. Slow LFO on each comb's delay time smears resonant modes.
class ModVerb
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        const float combMs[4] = { 47.0f, 53.0f, 44.0f, 58.0f };
        for (int i = 0; i < 4; ++i)
        {
            int maxLen = (int)(sr * 0.13) + 64;
            cl[i].buf.assign((size_t)maxLen, 0.0f);
            cr[i].buf.assign((size_t)maxLen, 0.0f);
            cl[i].len = maxLen; cr[i].len = maxLen;
            cl[i].base = combMs[i] * (float)sr / 1000.0f;
            cr[i].base = (combMs[i] + 1.1f) * (float)sr / 1000.0f;
            cl[i].pos = cr[i].pos = 0; cl[i].lp = cr[i].lp = 0.0f;
        }
        const float apMs[4] = { 5.0f, 1.7f, 3.4f, 2.6f };
        const float apG[4] = { 0.7f, 0.6f, 0.65f, 0.6f };
        for (int i = 0; i < 4; ++i)
        {
            int maxLen = (int)(sr * 0.01) + 16;
            al[i].buf.assign((size_t)maxLen, 0.0f);
            ar[i].buf.assign((size_t)maxLen, 0.0f);
            al[i].len = ar[i].len = maxLen;
            al[i].d = apMs[i] * (float)sr / 1000.0f;
            ar[i].d = (apMs[i] + 0.4f) * (float)sr / 1000.0f;
            al[i].pos = ar[i].pos = 0; al[i].g = ar[i].g = apG[i];
        }
        pd.buf.assign((size_t)((int)(sr * 0.06) + 16), 0.0f);
        pd.len = (int)pd.buf.size(); pd.pos = 0;
    }

    void process(float* L, float* R, int n, float size, float damp, float mix)
    {
        if (mix <= 0.001f) return;
        size = juce::jlimit(0.0f, 1.0f, size);
        float fb = 0.70f + size * 0.25f;
        float lpC = 0.55f * (1.0f - juce::jlimit(0.0f, 1.0f, damp)) + 0.05f;
        float lenScale = 0.75f + size * 0.6f;
        float kLen = 1.0f - std::exp(-1.0f / (0.050f * (float)sampleRate));
        float modAmp = (float)((0.0006 + size * 0.002) * sampleRate);
        const float rates[4] = { 0.11f, 0.19f, 0.27f, 0.23f };
        float wetG = (0.5f + size * 0.7f);
        float preD = size * 0.025f * (float)sampleRate;

        for (int i = 0; i < n; ++i)
        {
            curLenScale += (lenScale - curLenScale) * kLen;
            float dry = (L[i] + R[i]) * 0.5f;
            pd.buf[(size_t)pd.pos] = dry;
            float rp = (float)pd.pos - preD;
            while (rp < 0.0f) rp += (float)pd.len;
            int pi0 = (int)rp % pd.len; if (pi0 < 0) pi0 += pd.len;
            int pi1 = (pi0 + 1) % pd.len;
            float pf = rp - std::floor(rp);
            float in = pd.buf[(size_t)pi0] * (1.0f - pf) + pd.buf[(size_t)pi1] * pf;
            pd.pos = (pd.pos + 1) % pd.len;
            float accL = 0.0f, accR = 0.0f;
            for (int c = 0; c < 4; ++c)
            {
                mlfo[c] += rates[c] / sampleRate;
                if (mlfo[c] >= 1.0) mlfo[c] -= 1.0;
                float mod = std::sin(2.0f * juce::MathConstants<float>::pi * (float)mlfo[c]) * modAmp;
                accL += runComb(cl[c], in, fb, lpC, mod, curLenScale);
                accR += runComb(cr[c], in, fb, lpC, -mod, curLenScale);
            }
            accL *= 0.25f; accR *= 0.25f;
            float wL = accL, wR = accR;
            for (int a = 0; a < 4; ++a) { wL = runAP(wL, al[a]); wR = runAP(wR, ar[a]); }
            L[i] = L[i] * (1.0f - mix) + wL * wetG * mix;
            R[i] = R[i] * (1.0f - mix) + wR * wetG * mix;
        }
    }
    void clear()
    {
        for (auto* c : { cl, cr }) for (int i = 0; i < 4; ++i)
        { std::fill(c[i].buf.begin(), c[i].buf.end(), 0.0f); c[i].lp = 0.0f; }
        for (auto* a : { al, ar }) for (int i = 0; i < 4; ++i)
            std::fill(a[i].buf.begin(), a[i].buf.end(), 0.0f);
        std::fill(pd.buf.begin(), pd.buf.end(), 0.0f);
    }

private:
    struct Comb { std::vector<float> buf; int len = 1, pos = 0; float base = 0.0f, lp = 0.0f; };
    struct AP { std::vector<float> buf; int len = 1, pos = 0; float d = 0.0f, g = 0.65f; };
    struct PD { std::vector<float> buf; int len = 1, pos = 0; };
    Comb cl[4], cr[4];
    AP al[4], ar[4];
    PD pd;
    float curLenScale = 1.0f;
    double sampleRate = 44100.0, mlfo[4] = { 0.0, 0.25, 0.5, 0.75 };

    inline float readInterp(const std::vector<float>& buf, int len, float delaySamp, int writePos)
    {
        float rp = (float)writePos - delaySamp;
        while (rp < 0.0f) rp += (float)len;
        int i0 = (int)rp % len; if (i0 < 0) i0 += len;
        int i1 = (i0 + 1) % len;
        float frac = rp - std::floor(rp);
        return buf[(size_t)i0] * (1.0f - frac) + buf[(size_t)i1] * frac;
    }
    inline float runComb(Comb& c, float in, float fb, float lpC, float mod, float lenScale)
    {
        float d = juce::jlimit(4.0f, (float)c.len - 2.0f, c.base * lenScale + mod);
        float y = readInterp(c.buf, c.len, d, c.pos);
        c.lp += lpC * (y - c.lp);
        c.buf[(size_t)c.pos] = in + c.lp * fb;
        c.pos = (c.pos + 1) % c.len;
        return y;
    }
    inline float runAP(float x, AP& a)
    {
        float g = a.g;
        int rp = a.pos - (int)a.d;
        while (rp < 0) rp += a.len;
        rp %= a.len;
        float bufOut = a.buf[(size_t)rp];
        float y = -g * x + bufOut;
        a.buf[(size_t)a.pos] = x + g * y;
        a.pos = (a.pos + 1) % a.len;
        return y;
    }
};

class DelayFX
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        int maxLen = (int)(sr * 2.0) + 16;
        bufL.assign(maxLen, 0.0f); bufR.assign(maxLen, 0.0f);
        len = maxLen; pos = 0; lpL = lpR = 0.0f;
    }
    void process(float* L, float* R, int n, float timeMs, float fb, float mix)
    {
        if (mix <= 0.001f) return;
        timeMs = juce::jlimit(20.0f, 1500.0f, timeMs);
        float samples = (float)(timeMs * sampleRate / 1000.0);
        for (int i = 0; i < n; ++i)
        {
            float rp = (float)pos - samples;
            while (rp < 0) rp += len;
            int i0 = (int)rp; float frac = rp - i0; int i1 = (i0 + 1) % len;
            float dL = bufL[(size_t)i0] * (1 - frac) + bufL[(size_t)i1] * frac;
            float dR = bufR[(size_t)i0] * (1 - frac) + bufR[(size_t)i1] * frac;
            lpL += 0.25f * (dL - lpL); lpR += 0.25f * (dR - lpR); // damping
            bufL[pos] = L[i] + lpL * fb;
            bufR[pos] = R[i] + lpR * fb;
            L[i] = L[i] * (1 - mix) + dL * mix;
            R[i] = R[i] * (1 - mix) + dR * mix;
            pos = (pos + 1) % len;
        }
    }
    void clear() { std::fill(bufL.begin(), bufL.end(), 0.0f); std::fill(bufR.begin(), bufR.end(), 0.0f); }
private:
    double sampleRate = 44100.0; std::vector<float> bufL, bufR; int len = 1024, pos = 0; float lpL = 0, lpR = 0;
};

} // namespace MonoFX
