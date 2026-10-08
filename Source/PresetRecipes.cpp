#include "PresetRecipes.h"
#include "Parameters.h"

// Recipe builders for the 256 factory data files. See Presets.h for the
// musical house rules. Dumped to Factory/*.xml by the PresetDump tool;
// the plugin itself never compiles these in.
struct Builder
{
    std::map<std::string, float> v;
    void set(const char* id, float x) { v[id] = x; }
    void set(const juce::String& id, float x) { v[id.toStdString()] = x; }
    void route(int slot, int src, float amt, int dst, int type)
    {
        v[P::modSrcId(slot).toStdString()] = (float)src;
        v[P::modAmtId(slot).toStdString()] = amt;
        v[P::modDstId(slot).toStdString()] = (float)dst;
        v[P::modTypeId(slot).toStdString()] = (float)type;
    }
    void noPitch(int slot) { route(slot, 0, 0.0f, 0, 0); }
    void base()
    {
        set(P::VCO1_WAVE, 0); set(P::VCO1_OCT, 2); set(P::VCO1_PITCH, 0); set(P::VCO1_TUNE, 0); set(P::VCO1_PW, 0.5f);
        set(P::VCO2_WAVE, 2); set(P::VCO2_OCT, 2); set(P::VCO2_PITCH, 0); set(P::VCO2_TUNE, 0); set(P::VCO2_PW, 0.5f);
        set(P::MIX_VCO1, 0.9f); set(P::MIX_VCO2, 0.7f); set(P::MIX_RING, 0.0f); set(P::MIX_NOISE, 0.0f);
        set(P::THICK, 0.3f);
        set(P::NOISE_TYPE, 0);
        set(P::HPF_CUT, 25.0f); set(P::HPF_PEAK, 0.1f); set(P::LPF_CUT, 2800.0f);
        set(P::LPF_RES, 0.25f); set(P::DRIVE, 0.2f); set(P::FMODEL, 0);
        set(P::SAT_AMT, 0.12f); set(P::SAT_TYPE, 0);
        set(P::VCA_LEVEL, 0.85f); set(P::VOLUME, 0.5f); set(P::PORTA, 0.0f);
        set(P::PORTA_ON, 0.0f); set(P::GLIDE_MODE, 0.0f);
        set(P::LIMITER, 1.0f);
        set(P::CH_ON, 0.0f); set(P::CH_RATE, 0.6f); set(P::CH_DEPTH, 0.4f);
        set(P::DL_ON, 0.0f); set(P::DL_TIME, 350.0f); set(P::DL_FB, 0.35f); set(P::DL_MIX, 0.25f);
        set(P::RV_ON, 0.0f); set(P::RV_SIZE, 0.5f); set(P::RV_MIX, 0.25f);
        set(P::MG_WAVE, 0); set(P::MG_RATE, 4.0f); set(P::MG_SYNC, 0.0f);
        set(P::EG1_A, 0.005f); set(P::EG1_D, 0.35f); set(P::EG1_S, 0.2f); set(P::EG1_R, 0.25f);
        set(P::EG2_A, 0.002f); set(P::EG2_D, 0.15f); set(P::EG2_S, 0.85f); set(P::EG2_R, 0.15f);
        set(P::BEND_RANGE, 2.0f); set(P::VELSENS, 0.5f);
        route(0, 1, 0.12f, 6, 0);
        route(1, 2, 0.55f, 6, 0);
        route(2, 2, 0.20f, 5, 0);
        route(3, 3, 1.00f, 9, 1);
        route(4, 4, 0.50f, 9, 1);
        route(5, 0, 0.00f, 0, 0);
        route(6, 6, 0.30f, 6, 0);
        route(7, 4, 0.35f, 6, 0);
    }
};

static float stackIv(juce::Random& rng, int style)
{
    static const float opts[] = { 0.0f, 0.0f, -12.0f, 7.0f, 12.0f, -7.0f };
    if (style == 0) return 0.0f;
    return opts[(int)(rng.nextFloat() * 6.0f)];
}

std::vector<RecipePreset> buildRecipePresets()
{
    std::vector<RecipePreset> out;
    juce::Random rng(1234);
    auto add = [&](const std::string& name, Builder b) {
        RecipePreset p; p.fileName = name; p.values = b.v; out.push_back(p);
    };
    auto rf = [&](float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto ri = [&](int lo, int hi) { return lo + (int)(rng.nextFloat() * (float)(hi - lo + 1)); };
    auto nm3 = [&](int n, const char* base, const char* extra, const char* bank) {
        juce::String s = juce::String(n).paddedLeft('0', 3) + " " + juce::String(base) + juce::String(extra)
            + " [" + juce::String(bank) + "]";
        return s.toStdString();
    };

    const char* bassN[] = { "Deep Sub", "Solid Bottom", "Round Low", "Tube Bass", "Velvet Under", "Concrete", "Night Drive", "Low Authority", "Foundation", "Heavy Root", "Soft Hammer", "Dark Pillar", "Analog Heart", "Warm Weight", "Iron String", "Bottom Feeder" };
    const char* sqN[] = { "Acid Line", "Squelch Box", "Rubber Band", "Wet Circuit", "Reso Burner", "Liquid Silver", "Bubble Trouble", "Snapback", "Voltage Chew", "Squelch Puppy", "Elastic", "Goo Step" };
    const char* leadN[] = { "Mono Cry", "Saw Prayer", "Night Howl", "Silver Fang", "Lead Pipe", "Neon Throat", "Sharp Visitor", "High Voltage", "Signal Fire", "Lonely Horn", "Searing", "Sky Wire" };
    const char* keyN[] = { "Dusty Keys", "Soft Clav", "Muted Pluck", "Wooden Key", "Paper Piano", "Glass Touch", "Felt Hammer", "Old School" };
    const char* drumN[] = { "Kick", "Snare", "CHat", "OHat", "LoTom", "HiTom", "Clap", "Zap" };
    const char* fxN[] = { "Alien Choir", "Riser Beam", "Falling Star", "Robot Talk", "Storm Front", "Laser Hall", "Ghost Wire", "Deep Space" };

    // ---- 0..63 Analog bass ----
    for (int i = 0; i < 64; ++i)
    {
        Builder b; b.base();
        int wmode = i % 4;
        b.set(P::VCO1_WAVE, (float)(wmode == 0 ? 0 : (wmode == 1 ? 2 : (wmode == 2 ? 3 : 0))));
        b.set(P::VCO2_WAVE, (float)(wmode == 0 ? 2 : (wmode == 1 ? 0 : (wmode == 2 ? 3 : 4))));
        b.set(P::VCO1_OCT, (float)ri(0, 2));
        b.set(P::VCO2_OCT, (float)ri(1, 2));
        b.set(P::VCO2_PITCH, stackIv(rng, i % 3 == 0 ? 1 : 0));
        b.set(P::VCO1_PW, rf(0.15f, 0.85f)); b.set(P::VCO2_PW, rf(0.15f, 0.85f));
        b.set(P::MIX_VCO1, rf(0.8f, 1.0f)); b.set(P::MIX_VCO2, rf(0.4f, 0.9f));
        b.set(P::MIX_RING, i % 6 == 0 ? rf(0.2f, 0.5f) : 0.0f);
        b.set(P::MIX_NOISE, i % 8 == 0 ? rf(0.05f, 0.2f) : 0.0f);
        bool bandGrowl = (i % 5 == 4);
        b.set(P::HPF_CUT, bandGrowl ? rf(250.0f, 900.0f) : rf(10.0f, 90.0f));
        b.set(P::HPF_PEAK, bandGrowl ? rf(0.4f, 0.8f) : rf(0.0f, 0.2f));
        b.set(P::LPF_CUT, rf(250.0f, 1600.0f)); b.set(P::LPF_RES, rf(0.05f, i % 4 == 3 ? 0.7f : 0.45f));
        b.set(P::DRIVE, rf(0.15f, 0.45f));
        b.set(P::SAT_AMT, rf(0.1f, 0.35f));
        b.set(P::SAT_TYPE, (float)(i % 9 == 0 ? 3 : (i % 3 == 0 ? 1 : 0)));
        b.set(P::THICK, rf(0.3f, 0.55f));
        b.set(P::FMODEL, (float)(i % 2));
        bool pluck = (i % 2 == 0);
        b.set(P::EG1_A, rf(0.001f, 0.01f));
        b.set(P::EG1_D, pluck ? rf(0.08f, 0.3f) : rf(0.3f, 0.7f));
        b.set(P::EG1_S, pluck ? rf(0.0f, 0.25f) : rf(0.4f, 0.8f));
        b.set(P::EG1_R, rf(0.06f, 0.3f));
        b.set(P::EG2_A, rf(0.001f, 0.008f)); b.set(P::EG2_D, rf(0.05f, 0.3f));
        b.set(P::EG2_S, pluck ? rf(0.2f, 0.7f) : rf(0.7f, 1.0f)); b.set(P::EG2_R, rf(0.06f, 0.3f));
        b.set(P::MG_WAVE, i % 5); b.set(P::MG_RATE, rf(0.1f, 2.0f));
        b.set(P::PORTA, i % 4 == 0 ? rf(0.08f, 0.3f) : 0.0f);
        b.set(P::PORTA_ON, i % 4 == 0 ? 1.0f : 0.0f);
        b.route(0, 1, i % 3 == 0 ? rf(0.1f, 0.3f) : 0.0f, 3 + (i % 2), 0);
        b.route(1, 2, rf(0.35f, 0.7f), 6, 0);
        b.route(2, 2, bandGrowl ? rf(0.2f, 0.5f) : 0.0f, 5, 0);
        b.route(4, 4, rf(0.3f, 0.7f), 9, 1);
        b.route(7, 4, rf(0.2f, 0.55f), 6, 0);
        add(nm3(i, bassN[i % 16], i >= 16 ? (" " + juce::String(i / 16 + 1)).toStdString().c_str() : "", "Bass"), b);
    }
    // ---- 64..111 Acid: single-osc punch, low HPF, Tube/Tape only ----
    for (int i = 0; i < 48; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)(i % 2 == 0 ? 0 : 3));
        b.set(P::VCO2_WAVE, 0);
        b.set(P::VCO1_OCT, 2); b.set(P::VCO2_OCT, 2);
        b.set(P::VCO2_PITCH, 0.0f);
        b.set(P::VCO1_PW, rf(0.4f, 0.6f)); b.set(P::VCO2_PW, 0.5f);
        b.set(P::MIX_VCO1, rf(0.85f, 1.0f)); b.set(P::MIX_VCO2, rf(0.15f, 0.35f));
        b.set(P::MIX_RING, 0.0f); b.set(P::MIX_NOISE, 0.0f);
        b.set(P::HPF_CUT, rf(10.0f, 60.0f)); b.set(P::HPF_PEAK, rf(0.0f, 0.15f));
        b.set(P::LPF_CUT, rf(500.0f, 1500.0f)); b.set(P::LPF_RES, rf(0.65f, 0.9f));
        b.set(P::DRIVE, rf(0.2f, 0.45f));
        b.set(P::SAT_AMT, rf(0.15f, 0.35f)); b.set(P::SAT_TYPE, (float)(i % 3 == 0 ? 0 : 1));
        b.set(P::THICK, rf(0.25f, 0.4f));
        b.set(P::FMODEL, (float)(i % 2));
        b.set(P::EG1_A, rf(0.001f, 0.003f)); b.set(P::EG1_D, rf(0.1f, 0.3f));
        b.set(P::EG1_S, rf(0.0f, 0.15f)); b.set(P::EG1_R, rf(0.05f, 0.15f));
        b.set(P::EG2_A, rf(0.001f, 0.003f)); b.set(P::EG2_D, rf(0.1f, 0.25f));
        b.set(P::EG2_S, rf(0.7f, 1.0f)); b.set(P::EG2_R, rf(0.05f, 0.15f));
        b.set(P::MG_WAVE, 0); b.set(P::MG_RATE, 4.0f);
        b.set(P::PORTA, i % 2 == 0 ? rf(0.08f, 0.25f) : 0.0f);
        b.set(P::PORTA_ON, i % 2 == 0 ? 1.0f : 0.0f);
        b.set(P::GLIDE_MODE, 0.0f);
        b.set(P::VELSENS, rf(0.5f, 0.8f));
        b.noPitch(0);
        b.route(1, 2, rf(0.7f, 1.0f), 6, 0);
        b.noPitch(2);
        b.route(4, 4, rf(0.5f, 0.9f), 9, 1);
        b.noPitch(5);
        b.route(6, 6, rf(0.2f, 0.4f), 6, 0);
        b.route(7, 4, rf(0.4f, 0.7f), 6, 0);
        add(nm3(64 + i, sqN[i % 12], i >= 12 ? (" " + juce::String(i / 12 + 1)).toStdString().c_str() : "", "Acid"), b);
    }
    // ---- 112..159 Mono leads ----
    for (int i = 0; i < 48; ++i)
    {
        Builder b; b.base();
        int wmode = i % 5;
        b.set(P::VCO1_WAVE, (float)(wmode == 4 ? 4 : wmode));
        b.set(P::VCO2_WAVE, (float)((wmode + 2) % 5));
        b.set(P::VCO1_OCT, (float)ri(2, 3)); b.set(P::VCO2_OCT, (float)ri(2, 3));
        b.set(P::VCO2_PITCH, i % 2 == 0 ? (i % 4 == 0 ? 7.0f : 12.0f) : 0.0f);
        b.set(P::VCO1_PW, rf(0.2f, 0.8f)); b.set(P::VCO2_PW, rf(0.2f, 0.8f));
        b.set(P::MIX_VCO1, rf(0.8f, 1.0f)); b.set(P::MIX_VCO2, rf(0.5f, 1.0f));
        b.set(P::MIX_RING, i % 5 == 0 ? rf(0.2f, 0.5f) : 0.0f);
        b.set(P::HPF_CUT, rf(20.0f, 200.0f)); b.set(P::HPF_PEAK, rf(0.0f, 0.25f));
        b.set(P::LPF_CUT, rf(1800.0f, 12000.0f)); b.set(P::LPF_RES, rf(0.1f, i % 3 == 2 ? 0.6f : 0.4f));
        b.set(P::DRIVE, rf(0.15f, 0.4f));
        b.set(P::SAT_AMT, rf(0.1f, 0.3f)); b.set(P::SAT_TYPE, (float)(i % 2));
        b.set(P::THICK, rf(0.25f, 0.45f));
        b.set(P::EG1_A, rf(0.002f, 0.04f)); b.set(P::EG1_D, rf(0.15f, 0.6f));
        b.set(P::EG1_S, rf(0.35f, 0.85f)); b.set(P::EG1_R, rf(0.1f, 0.4f));
        b.set(P::EG2_A, rf(0.002f, 0.04f)); b.set(P::EG2_D, rf(0.1f, 0.4f));
        b.set(P::EG2_S, rf(0.6f, 1.0f)); b.set(P::EG2_R, rf(0.1f, 0.4f));
        b.set(P::MG_WAVE, i % 5); b.set(P::MG_RATE, rf(0.2f, 7.0f));
        b.set(P::PORTA, i % 3 != 0 ? rf(0.05f, 0.3f) : 0.0f);
        b.set(P::PORTA_ON, i % 3 != 0 ? 1.0f : 0.0f);
        b.set(P::BEND_RANGE, (float)(i % 3 == 0 ? 12 : (i % 3 == 1 ? 7 : 2)));
        if (i % 3 == 0) { b.set(P::CH_ON, 1.0f); b.set(P::CH_RATE, rf(0.3f, 0.9f)); b.set(P::CH_DEPTH, rf(0.3f, 0.6f)); }
        if (i % 5 == 4) { b.set(P::DL_ON, 1.0f); b.set(P::DL_TIME, rf(250.0f, 450.0f)); b.set(P::DL_FB, rf(0.25f, 0.45f)); b.set(P::DL_MIX, rf(0.2f, 0.35f)); }
        b.route(0, 1, rf(0.05f, 0.2f), 6, 0);
        b.route(1, 2, rf(0.1f, 0.4f), 6, 0);
        b.route(5, 1, rf(0.1f, 0.3f), (i % 2) + 3, 0);
        b.route(6, 6, rf(0.1f, 0.35f), 6, 0);
        b.route(7, 5, i % 4 == 0 ? 0.4f : 0.0f, 6, 0);
        add(nm3(112 + i, leadN[i % 12], i >= 12 ? (" " + juce::String(i / 12 + 1)).toStdString().c_str() : "", "Lead"), b);
    }
    // ---- 160..191 Keys ----
    for (int i = 0; i < 32; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)(i % 3 == 0 ? 1 : (i % 3 == 1 ? 3 : 4)));
        b.set(P::VCO2_WAVE, (float)(i % 2 == 0 ? 1 : 4));
        b.set(P::VCO1_OCT, 2); b.set(P::VCO2_OCT, (float)ri(2, 3));
        b.set(P::VCO2_PITCH, i % 2 == 0 ? (i % 4 == 0 ? -12.0f : 12.0f) : 0.0f);
        b.set(P::VCO1_PW, rf(0.3f, 0.7f)); b.set(P::VCO2_PW, rf(0.3f, 0.7f));
        b.set(P::MIX_VCO1, rf(0.7f, 0.9f)); b.set(P::MIX_VCO2, rf(0.4f, 0.8f));
        b.set(P::MIX_NOISE, i % 8 == 0 ? rf(0.03f, 0.1f) : 0.0f);
        b.set(P::HPF_CUT, rf(20.0f, 150.0f));
        b.set(P::LPF_CUT, rf(800.0f, 6000.0f)); b.set(P::LPF_RES, rf(0.05f, 0.3f));
        b.set(P::DRIVE, rf(0.05f, 0.25f));
        b.set(P::SAT_AMT, rf(0.05f, 0.2f)); b.set(P::SAT_TYPE, 0);
        b.set(P::THICK, rf(0.15f, 0.3f));
        b.set(P::EG1_A, rf(0.002f, 0.03f)); b.set(P::EG1_D, rf(0.2f, 0.7f));
        b.set(P::EG1_S, rf(0.2f, 0.65f)); b.set(P::EG1_R, rf(0.15f, 0.5f));
        b.set(P::EG2_A, rf(0.002f, 0.03f)); b.set(P::EG2_D, rf(0.2f, 0.6f));
        b.set(P::EG2_S, rf(0.4f, 0.8f)); b.set(P::EG2_R, rf(0.15f, 0.5f));
        b.set(P::MG_WAVE, 0); b.set(P::MG_RATE, rf(0.1f, 1.2f));
        b.route(0, 1, rf(0.1f, 0.3f), 3 + (i % 2), 0);
        b.route(1, 2, rf(0.15f, 0.4f), 6, 0);
        b.route(4, 4, rf(0.4f, 0.8f), 9, 1);
        b.noPitch(5);
        if (i % 2 == 0) { b.set(P::CH_ON, 1.0f); b.set(P::CH_RATE, rf(0.3f, 0.8f)); b.set(P::CH_DEPTH, rf(0.25f, 0.5f)); }
        add(nm3(160 + i, keyN[i % 8], i >= 8 ? (" " + juce::String(i / 8 + 1)).toStdString().c_str() : "", "Keys"), b);
    }
    // ---- 192..223 Drums: one recipe per drum kind ----
    for (int i = 0; i < 32; ++i)
    {
        Builder b; b.base();
        int kind = i % 8;
        b.set(P::VCO2_WAVE, 4); b.set(P::VCO2_OCT, 2);
        b.set(P::MIX_RING, 0.0f);
        b.set(P::EG1_A, 0.001f); b.set(P::EG1_S, 0.0f); b.set(P::EG1_R, 0.05f);
        b.set(P::EG2_A, 0.001f); b.set(P::EG2_S, 0.0f); b.set(P::EG2_R, 0.05f);
        b.set(P::VELSENS, 0.7f);
        b.route(3, 3, 1.0f, 9, 1);
        b.route(4, 4, 0.6f, 9, 1);
        b.noPitch(5); b.noPitch(6); b.noPitch(7);
        if (kind == 0) // Kick: sine at -1 oct, fast pitch drop, open filter
        {
            b.set(P::VCO1_WAVE, 4); b.set(P::VCO1_OCT, 1);
            b.set(P::MIX_VCO1, 1.0f); b.set(P::MIX_VCO2, 0.0f); b.set(P::MIX_NOISE, 0.0f);
            b.set(P::HPF_CUT, 20.0f); b.set(P::HPF_PEAK, 0.0f);
            b.set(P::LPF_CUT, 12000.0f); b.set(P::LPF_RES, 0.0f);
            b.set(P::DRIVE, 0.05f);
            b.set(P::SAT_AMT, 0.1f); b.set(P::SAT_TYPE, 0); b.set(P::THICK, 0.2f);
            b.set(P::EG1_D, rf(0.08f, 0.15f));
            b.set(P::EG2_D, rf(0.25f, 0.4f));
            b.route(0, 2, rf(0.6f, 0.8f), 1, 0);
            b.noPitch(1); b.noPitch(2);
        }
        else if (kind == 1) // Snare: triangle body + white noise snap
        {
            b.set(P::VCO1_WAVE, 1); b.set(P::VCO1_OCT, 2);
            b.set(P::MIX_VCO1, 0.8f); b.set(P::MIX_VCO2, 0.4f); b.set(P::MIX_NOISE, rf(0.7f, 1.0f));
            b.set(P::NOISE_TYPE, 0);
            b.set(P::HPF_CUT, rf(100.0f, 250.0f)); b.set(P::HPF_PEAK, 0.1f);
            b.set(P::LPF_CUT, rf(4000.0f, 9000.0f)); b.set(P::LPF_RES, rf(0.1f, 0.3f));
            b.set(P::DRIVE, rf(0.2f, 0.4f));
            b.set(P::SAT_AMT, rf(0.15f, 0.3f)); b.set(P::SAT_TYPE, 0); b.set(P::THICK, 0.3f);
            b.set(P::EG1_D, rf(0.06f, 0.12f));
            b.set(P::EG2_D, rf(0.08f, 0.18f));
            b.route(0, 2, rf(0.4f, 0.7f), 6, 0);
            b.route(1, 2, rf(0.2f, 0.4f), 5, 0);
        }
        else if (kind == 2 || kind == 3) // Hats: high squares + noise, very short
        {
            bool open = (kind == 3);
            b.set(P::VCO1_WAVE, 2); b.set(P::VCO1_OCT, 4);
            b.set(P::VCO2_WAVE, 2); b.set(P::VCO2_OCT, 4);
            b.set(P::MIX_VCO1, 0.6f); b.set(P::MIX_VCO2, 0.4f); b.set(P::MIX_NOISE, rf(0.8f, 1.0f));
            b.set(P::NOISE_TYPE, 0);
            b.set(P::HPF_CUT, open ? rf(5000.0f, 8000.0f) : rf(6000.0f, 9000.0f));
            b.set(P::HPF_PEAK, rf(0.1f, 0.3f));
            b.set(P::LPF_CUT, 15000.0f); b.set(P::LPF_RES, 0.0f);
            b.set(P::DRIVE, rf(0.1f, 0.25f));
            b.set(P::SAT_AMT, 0.1f); b.set(P::SAT_TYPE, 0); b.set(P::THICK, 0.2f);
            b.set(P::EG1_D, 0.03f);
            b.set(P::EG2_D, open ? rf(0.2f, 0.45f) : rf(0.03f, 0.06f));
            b.set(P::EG2_R, 0.03f);
            b.route(0, 2, 0.3f, 5, 0);
            b.noPitch(1); b.noPitch(2);
        }
        else if (kind == 4 || kind == 5) // Toms: sine body with pitch drop
        {
            bool hi = (kind == 5);
            b.set(P::VCO1_WAVE, 4); b.set(P::VCO1_OCT, hi ? 2 : 1);
            b.set(P::MIX_VCO1, 1.0f); b.set(P::MIX_VCO2, 0.0f); b.set(P::MIX_NOISE, 0.0f);
            b.set(P::HPF_CUT, 20.0f); b.set(P::HPF_PEAK, 0.0f);
            b.set(P::LPF_CUT, hi ? rf(1200.0f, 3000.0f) : rf(800.0f, 2000.0f));
            b.set(P::LPF_RES, rf(0.2f, 0.4f));
            b.set(P::DRIVE, rf(0.1f, 0.25f));
            b.set(P::SAT_AMT, rf(0.1f, 0.2f)); b.set(P::SAT_TYPE, 0); b.set(P::THICK, 0.25f);
            b.set(P::EG1_D, rf(0.12f, 0.25f));
            b.set(P::EG2_D, rf(0.15f, 0.3f));
            b.route(0, 2, rf(0.4f, 0.6f), 1, 0);
            b.route(1, 2, rf(0.3f, 0.5f), 6, 0);
            b.noPitch(2);
        }
        else if (kind == 6) // Clap: bandpassed noise grit
        {
            b.set(P::VCO1_WAVE, 3); b.set(P::VCO1_OCT, 2);
            b.set(P::MIX_VCO1, 0.5f); b.set(P::MIX_VCO2, 0.0f); b.set(P::MIX_NOISE, rf(0.7f, 1.0f));
            b.set(P::NOISE_TYPE, 0);
            b.set(P::HPF_CUT, rf(800.0f, 1800.0f)); b.set(P::HPF_PEAK, rf(0.2f, 0.5f));
            b.set(P::LPF_CUT, rf(3500.0f, 7000.0f)); b.set(P::LPF_RES, rf(0.2f, 0.4f));
            b.set(P::DRIVE, rf(0.25f, 0.45f));
            b.set(P::SAT_AMT, rf(0.2f, 0.35f)); b.set(P::SAT_TYPE, 1); b.set(P::THICK, 0.3f);
            b.set(P::EG1_D, rf(0.08f, 0.15f));
            b.set(P::EG2_D, rf(0.12f, 0.25f));
            b.route(0, 2, rf(0.5f, 0.8f), 6, 0);
            b.route(1, 2, rf(0.2f, 0.4f), 5, 0);
        }
        else // Zap: resonant sweep with pitch drop
        {
            b.set(P::VCO1_WAVE, 0); b.set(P::VCO1_OCT, 2);
            b.set(P::MIX_VCO1, 0.9f); b.set(P::MIX_VCO2, 0.0f); b.set(P::MIX_NOISE, 0.0f);
            b.set(P::HPF_CUT, 30.0f); b.set(P::HPF_PEAK, 0.1f);
            b.set(P::LPF_CUT, rf(1000.0f, 4000.0f)); b.set(P::LPF_RES, rf(0.6f, 0.95f));
            b.set(P::DRIVE, rf(0.2f, 0.4f));
            b.set(P::SAT_AMT, rf(0.15f, 0.3f)); b.set(P::SAT_TYPE, (float)(i % 2 ? 2 : 1));
            b.set(P::THICK, 0.3f);
            b.set(P::EG1_D, rf(0.1f, 0.25f));
            b.set(P::EG2_D, rf(0.1f, 0.2f));
            b.route(0, 2, 0.6f, 1, 0);
            b.route(1, 2, rf(0.5f, 0.8f), 6, 0);
            b.noPitch(2);
        }
        juce::String dn = juce::String(drumN[kind]) + " " + juce::String(i / 8 + 1);
        add(nm3(192 + i, dn.toStdString().c_str(), "", "Drums"), b);
    }
    // ---- 224..255 FX ----
    for (int i = 0; i < 32; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)ri(0, 4));
        b.set(P::VCO2_WAVE, (float)ri(0, 4));
        b.set(P::VCO1_OCT, (float)ri(0, 4)); b.set(P::VCO2_OCT, (float)ri(0, 4));
        b.set(P::VCO2_PITCH, i % 3 == 0 ? (i % 2 ? 7.0f : -5.0f) : 0.0f);
        b.set(P::VCO1_PW, rf(0.1f, 0.9f)); b.set(P::VCO2_PW, rf(0.1f, 0.9f));
        b.set(P::MIX_VCO1, rf(0.5f, 1.0f)); b.set(P::MIX_VCO2, rf(0.5f, 1.0f));
        b.set(P::MIX_RING, i % 2 == 0 ? rf(0.3f, 0.9f) : rf(0.0f, 0.2f));
        b.set(P::MIX_NOISE, i % 3 == 0 ? rf(0.3f, 0.8f) : rf(0.0f, 0.2f));
        b.set(P::NOISE_TYPE, (float)(i % 2));
        b.set(P::HPF_CUT, rf(30.0f, 4000.0f)); b.set(P::HPF_PEAK, rf(0.2f, 0.9f));
        b.set(P::LPF_CUT, rf(300.0f, 9000.0f)); b.set(P::LPF_RES, rf(0.4f, 1.0f));
        b.set(P::DRIVE, rf(0.2f, 0.8f));
        b.set(P::SAT_AMT, rf(0.15f, 0.45f)); b.set(P::SAT_TYPE, (float)(i % 4));
        b.set(P::THICK, rf(0.2f, 0.5f));
        b.set(P::FMODEL, (float)(i % 2));
        b.set(P::MG_WAVE, i % 2 == 0 ? 4 : ri(0, 3)); b.set(P::MG_RATE, rf(0.1f, 11.0f));
        b.set(P::EG1_A, rf(0.01f, 0.8f)); b.set(P::EG1_D, rf(0.2f, 1.8f));
        b.set(P::EG1_S, rf(0.0f, 0.8f)); b.set(P::EG1_R, rf(0.3f, 1.8f));
        b.set(P::EG2_A, rf(0.01f, 0.6f)); b.set(P::EG2_D, rf(0.2f, 1.4f));
        b.set(P::EG2_S, rf(0.1f, 0.9f)); b.set(P::EG2_R, rf(0.3f, 1.8f));
        b.route(0, 1, rf(0.3f, 0.9f), 6, 0);
        b.route(1, 2, rf(0.2f, 0.8f), i % 2 ? 6 : 5, 0);
        b.route(2, 1, rf(0.1f, 0.5f), 8, 0);
        b.noPitch(5);
        b.route(6, 4, rf(0.0f, 0.6f), 8, 0);
        b.route(7, 9, rf(0.2f, 0.6f), 6, 0);
        if (i % 3 == 0) { b.set(P::DL_ON, 1.0f); b.set(P::DL_TIME, rf(200.0f, 500.0f)); b.set(P::DL_FB, rf(0.25f, 0.5f)); b.set(P::DL_MIX, rf(0.2f, 0.4f)); }
        if (i % 3 == 1) { b.set(P::RV_ON, 1.0f); b.set(P::RV_SIZE, rf(0.5f, 0.85f)); b.set(P::RV_MIX, rf(0.2f, 0.35f)); }
        add(nm3(224 + i, fxN[i % 8], i >= 8 ? (" " + juce::String(i / 8 + 1)).toStdString().c_str() : "", "FX"), b);
    }
    // ---- 256..285 Space Bass: deep basses with chorus + reverb ----
    const char* spcN[] = { "Space Bass", "Deep Field", "Submerged", "Low Orbit", "Trench", "Pressure", "Abyss", "Undertow", "Bass Nebula", "Dark Matter" };
    for (int i = 0; i < 30; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)(i % 2 == 0 ? 0 : 2));
        b.set(P::VCO2_WAVE, (float)(i % 2 == 0 ? 2 : 0));
        b.set(P::VCO1_OCT, (float)ri(0, 1));
        b.set(P::VCO2_OCT, (float)ri(1, 2));
        b.set(P::VCO2_PITCH, stackIv(rng, i % 4 == 0 ? 1 : 0));
        b.set(P::VCO1_PW, rf(0.2f, 0.8f)); b.set(P::VCO2_PW, rf(0.2f, 0.8f));
        b.set(P::MIX_VCO1, rf(0.8f, 1.0f)); b.set(P::MIX_VCO2, rf(0.4f, 0.8f));
        b.set(P::HPF_CUT, rf(10.0f, 60.0f)); b.set(P::HPF_PEAK, rf(0.0f, 0.1f));
        b.set(P::LPF_CUT, rf(200.0f, 900.0f)); b.set(P::LPF_RES, rf(0.2f, 0.5f));
        b.set(P::DRIVE, rf(0.2f, 0.5f));
        b.set(P::SAT_AMT, rf(0.15f, 0.4f)); b.set(P::SAT_TYPE, (float)(i % 3 == 0 ? 1 : 0));
        b.set(P::THICK, rf(0.35f, 0.55f));
        b.set(P::FMODEL, (float)(i % 2));
        b.set(P::EG1_A, rf(0.001f, 0.008f)); b.set(P::EG1_D, rf(0.1f, 0.4f));
        b.set(P::EG1_S, rf(0.1f, 0.5f)); b.set(P::EG1_R, rf(0.08f, 0.3f));
        b.set(P::EG2_A, rf(0.001f, 0.005f)); b.set(P::EG2_D, rf(0.08f, 0.3f));
        b.set(P::EG2_S, rf(0.5f, 1.0f)); b.set(P::EG2_R, rf(0.08f, 0.3f));
        b.set(P::PORTA, i % 5 == 0 ? rf(0.08f, 0.25f) : 0.0f);
        b.set(P::PORTA_ON, i % 5 == 0 ? 1.0f : 0.0f);
        b.set(P::VELSENS, rf(0.4f, 0.7f));
        b.set(P::CH_ON, 1.0f); b.set(P::CH_RATE, rf(0.2f, 0.5f)); b.set(P::CH_DEPTH, rf(0.3f, 0.6f));
        b.set(P::RV_ON, 1.0f); b.set(P::RV_SIZE, rf(0.4f, 0.7f)); b.set(P::RV_MIX, rf(0.15f, 0.3f));
        b.route(0, 1, rf(0.0f, 0.15f), 6, 0);
        b.route(1, 2, rf(0.4f, 0.7f), 6, 0);
        b.route(4, 4, rf(0.3f, 0.7f), 9, 1);
        b.route(7, 4, rf(0.25f, 0.55f), 6, 0);
        add(nm3(256 + i, spcN[i % 10], i >= 10 ? (" " + juce::String(i / 10 + 1)).toStdString().c_str() : "", "Bass"), b);
    }
    // ---- 286..305 Acid Wash: acid lines with delay ----
    const char* washN[] = { "Acid Wash", "Echo Chamber", "Wet Signal", "Dub Line", "Feedback Loop", "Rolling Echo", "Squelch Dub", "Deep Wash", "Acid Rain", "Trailing Edge" };
    for (int i = 0; i < 20; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)(i % 2 == 0 ? 0 : 3));
        b.set(P::VCO2_WAVE, 0);
        b.set(P::VCO1_OCT, 2); b.set(P::VCO2_OCT, 2);
        b.set(P::VCO1_PW, rf(0.4f, 0.6f));
        b.set(P::MIX_VCO1, rf(0.85f, 1.0f)); b.set(P::MIX_VCO2, rf(0.15f, 0.35f));
        b.set(P::HPF_CUT, rf(10.0f, 60.0f)); b.set(P::HPF_PEAK, rf(0.0f, 0.15f));
        b.set(P::LPF_CUT, rf(500.0f, 1600.0f)); b.set(P::LPF_RES, rf(0.65f, 0.9f));
        b.set(P::DRIVE, rf(0.2f, 0.45f));
        b.set(P::SAT_AMT, rf(0.15f, 0.35f)); b.set(P::SAT_TYPE, (float)(i % 3 == 0 ? 0 : 1));
        b.set(P::THICK, rf(0.25f, 0.4f));
        b.set(P::FMODEL, (float)(i % 2));
        b.set(P::EG1_A, rf(0.001f, 0.003f)); b.set(P::EG1_D, rf(0.08f, 0.28f));
        b.set(P::EG1_S, rf(0.0f, 0.15f)); b.set(P::EG1_R, rf(0.05f, 0.15f));
        b.set(P::EG2_A, rf(0.001f, 0.003f)); b.set(P::EG2_D, rf(0.1f, 0.25f));
        b.set(P::EG2_S, rf(0.7f, 1.0f)); b.set(P::EG2_R, rf(0.05f, 0.15f));
        b.set(P::PORTA, i % 2 == 0 ? rf(0.08f, 0.25f) : 0.0f);
        b.set(P::PORTA_ON, i % 2 == 0 ? 1.0f : 0.0f);
        b.set(P::GLIDE_MODE, 0.0f);
        b.set(P::VELSENS, rf(0.5f, 0.8f));
        b.set(P::DL_ON, 1.0f); b.set(P::DL_TIME, rf(240.0f, 520.0f));
        b.set(P::DL_FB, rf(0.3f, 0.5f)); b.set(P::DL_MIX, rf(0.25f, 0.4f));
        if (i % 4 == 0) { b.set(P::RV_ON, 1.0f); b.set(P::RV_SIZE, rf(0.3f, 0.5f)); b.set(P::RV_MIX, rf(0.1f, 0.2f)); }
        b.noPitch(0);
        b.route(1, 2, rf(0.7f, 1.0f), 6, 0);
        b.noPitch(2);
        b.route(4, 4, rf(0.5f, 0.9f), 9, 1);
        b.noPitch(5);
        b.route(6, 6, rf(0.2f, 0.4f), 6, 0);
        b.route(7, 4, rf(0.4f, 0.7f), 6, 0);
        add(nm3(286 + i, washN[i % 10], i >= 10 ? (" " + juce::String(i / 10 + 1)).toStdString().c_str() : "", "Acid"), b);
    }
    // ---- 306..325 Ethereal Lead: spacious leads, delay + reverb ----
    const char* ethN[] = { "Ethereal Lead", "Sky Temple", "Distant Cry", "Halo Lead", "Weightless", "High Air", "Glass Voice", "Far Horizon", "Cloud Singer", "Silver Mist" };
    for (int i = 0; i < 20; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)ri(0, 4));
        b.set(P::VCO2_WAVE, (float)ri(0, 4));
        b.set(P::VCO1_OCT, (float)ri(2, 3)); b.set(P::VCO2_OCT, (float)ri(2, 3));
        b.set(P::VCO2_PITCH, i % 2 == 0 ? 7.0f : 0.0f);
        b.set(P::VCO1_PW, rf(0.2f, 0.8f)); b.set(P::VCO2_PW, rf(0.2f, 0.8f));
        b.set(P::MIX_VCO1, rf(0.8f, 1.0f)); b.set(P::MIX_VCO2, rf(0.5f, 0.9f));
        b.set(P::HPF_CUT, rf(20.0f, 150.0f)); b.set(P::HPF_PEAK, rf(0.0f, 0.2f));
        b.set(P::LPF_CUT, rf(2000.0f, 9000.0f)); b.set(P::LPF_RES, rf(0.15f, 0.5f));
        b.set(P::DRIVE, rf(0.15f, 0.4f));
        b.set(P::SAT_AMT, rf(0.1f, 0.3f)); b.set(P::SAT_TYPE, 0);
        b.set(P::THICK, rf(0.25f, 0.45f));
        b.set(P::EG1_A, rf(0.005f, 0.05f)); b.set(P::EG1_D, rf(0.2f, 0.6f));
        b.set(P::EG1_S, rf(0.5f, 0.9f)); b.set(P::EG1_R, rf(0.15f, 0.45f));
        b.set(P::EG2_A, rf(0.005f, 0.04f)); b.set(P::EG2_D, rf(0.15f, 0.4f));
        b.set(P::EG2_S, rf(0.6f, 1.0f)); b.set(P::EG2_R, rf(0.15f, 0.45f));
        b.set(P::MG_WAVE, i % 5); b.set(P::MG_RATE, rf(0.2f, 5.0f));
        b.set(P::PORTA, i % 3 != 0 ? rf(0.05f, 0.3f) : 0.0f);
        b.set(P::PORTA_ON, i % 3 != 0 ? 1.0f : 0.0f);
        b.set(P::BEND_RANGE, (float)(i % 2 == 0 ? 7 : 2));
        if (i % 2 == 0) { b.set(P::CH_ON, 1.0f); b.set(P::CH_RATE, rf(0.4f, 0.8f)); b.set(P::CH_DEPTH, rf(0.3f, 0.5f)); }
        b.set(P::DL_ON, 1.0f); b.set(P::DL_TIME, rf(300.0f, 550.0f));
        b.set(P::DL_FB, rf(0.3f, 0.5f)); b.set(P::DL_MIX, rf(0.25f, 0.4f));
        b.set(P::RV_ON, 1.0f); b.set(P::RV_SIZE, rf(0.55f, 0.85f)); b.set(P::RV_MIX, rf(0.2f, 0.35f));
        b.route(0, 1, rf(0.05f, 0.2f), 6, 0);
        b.route(1, 2, rf(0.15f, 0.4f), 6, 0);
        b.route(5, 1, rf(0.1f, 0.25f), (i % 2) + 3, 0);
        b.route(6, 6, rf(0.1f, 0.3f), 6, 0);
        b.route(7, 5, i % 4 == 0 ? 0.4f : 0.0f, 6, 0);
        add(nm3(306 + i, ethN[i % 10], i >= 10 ? (" " + juce::String(i / 10 + 1)).toStdString().c_str() : "", "Lead"), b);
    }
    // ---- 326..340 Dream Keys: chorused + reverbed keys ----
    const char* drmN[] = { "Dream Keys", "Soft Focus", "Hazy Morning", "Reverie", "Daydream", "Lucid", "Slow Bloom", "Memory Foam" };
    for (int i = 0; i < 15; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)(i % 3 == 0 ? 1 : (i % 3 == 1 ? 3 : 4)));
        b.set(P::VCO2_WAVE, (float)(i % 2 == 0 ? 1 : 4));
        b.set(P::VCO1_OCT, 2); b.set(P::VCO2_OCT, (float)ri(2, 3));
        b.set(P::VCO2_PITCH, i % 2 == 0 ? 12.0f : 0.0f);
        b.set(P::VCO1_PW, rf(0.3f, 0.7f)); b.set(P::VCO2_PW, rf(0.3f, 0.7f));
        b.set(P::MIX_VCO1, rf(0.7f, 0.9f)); b.set(P::MIX_VCO2, rf(0.4f, 0.8f));
        b.set(P::HPF_CUT, rf(20.0f, 120.0f));
        b.set(P::LPF_CUT, rf(1000.0f, 5500.0f)); b.set(P::LPF_RES, rf(0.05f, 0.3f));
        b.set(P::DRIVE, rf(0.05f, 0.25f));
        b.set(P::SAT_AMT, rf(0.05f, 0.2f)); b.set(P::SAT_TYPE, 0);
        b.set(P::THICK, rf(0.15f, 0.3f));
        b.set(P::EG1_A, rf(0.005f, 0.05f)); b.set(P::EG1_D, rf(0.25f, 0.7f));
        b.set(P::EG1_S, rf(0.25f, 0.65f)); b.set(P::EG1_R, rf(0.2f, 0.5f));
        b.set(P::EG2_A, rf(0.005f, 0.03f)); b.set(P::EG2_D, rf(0.2f, 0.6f));
        b.set(P::EG2_S, rf(0.4f, 0.8f)); b.set(P::EG2_R, rf(0.2f, 0.5f));
        b.set(P::MG_WAVE, 0); b.set(P::MG_RATE, rf(0.1f, 1.0f));
        b.set(P::CH_ON, 1.0f); b.set(P::CH_RATE, rf(0.3f, 0.7f)); b.set(P::CH_DEPTH, rf(0.3f, 0.55f));
        b.set(P::RV_ON, 1.0f); b.set(P::RV_SIZE, rf(0.5f, 0.8f)); b.set(P::RV_MIX, rf(0.2f, 0.35f));
        b.route(0, 1, rf(0.1f, 0.25f), 3 + (i % 2), 0);
        b.route(1, 2, rf(0.15f, 0.35f), 6, 0);
        b.route(4, 4, rf(0.4f, 0.8f), 9, 1);
        b.noPitch(5);
        add(nm3(326 + i, drmN[i % 8], i >= 8 ? (" " + juce::String(i / 8 + 1)).toStdString().c_str() : "", "Keys"), b);
    }
    // ---- 341..355 Deep FX: full-FX evolving beds ----
    const char* dfxN[] = { "Far Field", "Event Horizon", "Slow Signal", "Night Transmission", "Ion Trail", "Dark Energy", "Silent Running", "Void Call" };
    for (int i = 0; i < 15; ++i)
    {
        Builder b; b.base();
        b.set(P::VCO1_WAVE, (float)ri(0, 4));
        b.set(P::VCO2_WAVE, (float)ri(0, 4));
        b.set(P::VCO1_OCT, (float)ri(1, 3)); b.set(P::VCO2_OCT, (float)ri(1, 3));
        b.set(P::VCO1_PW, rf(0.15f, 0.85f)); b.set(P::VCO2_PW, rf(0.15f, 0.85f));
        b.set(P::MIX_VCO1, rf(0.6f, 1.0f)); b.set(P::MIX_VCO2, rf(0.5f, 0.9f));
        b.set(P::MIX_RING, i % 2 == 0 ? rf(0.3f, 0.7f) : 0.0f);
        b.set(P::MIX_NOISE, i % 3 == 0 ? rf(0.2f, 0.5f) : 0.0f);
        b.set(P::HPF_CUT, rf(30.0f, 1500.0f)); b.set(P::HPF_PEAK, rf(0.2f, 0.7f));
        b.set(P::LPF_CUT, rf(400.0f, 6000.0f)); b.set(P::LPF_RES, rf(0.4f, 0.9f));
        b.set(P::DRIVE, rf(0.2f, 0.6f));
        b.set(P::SAT_AMT, rf(0.15f, 0.4f)); b.set(P::SAT_TYPE, (float)(i % 4));
        b.set(P::THICK, rf(0.25f, 0.5f));
        b.set(P::FMODEL, (float)(i % 2));
        b.set(P::MG_WAVE, 4); b.set(P::MG_RATE, rf(0.1f, 6.0f));
        b.set(P::EG1_A, rf(0.05f, 0.8f)); b.set(P::EG1_D, rf(0.4f, 1.8f));
        b.set(P::EG1_S, rf(0.1f, 0.8f)); b.set(P::EG1_R, rf(0.4f, 1.8f));
        b.set(P::EG2_A, rf(0.05f, 0.6f)); b.set(P::EG2_D, rf(0.4f, 1.4f));
        b.set(P::EG2_S, rf(0.2f, 0.9f)); b.set(P::EG2_R, rf(0.4f, 1.8f));
        b.set(P::CH_ON, 1.0f); b.set(P::CH_RATE, rf(0.2f, 0.8f)); b.set(P::CH_DEPTH, rf(0.3f, 0.6f));
        b.set(P::DL_ON, 1.0f); b.set(P::DL_TIME, rf(200.0f, 600.0f));
        b.set(P::DL_FB, rf(0.25f, 0.55f)); b.set(P::DL_MIX, rf(0.2f, 0.4f));
        b.set(P::RV_ON, 1.0f); b.set(P::RV_SIZE, rf(0.5f, 0.9f)); b.set(P::RV_MIX, rf(0.2f, 0.4f));
        b.route(0, 1, rf(0.3f, 0.8f), 6, 0);
        b.route(1, 2, rf(0.2f, 0.7f), 6, 0);
        b.route(2, 1, rf(0.1f, 0.4f), 8, 0);
        b.noPitch(5);
        b.route(7, 9, rf(0.2f, 0.6f), 6, 0);
        add(nm3(341 + i, dfxN[i % 8], i >= 8 ? (" " + juce::String(i / 8 + 1)).toStdString().c_str() : "", "FX"), b);
    }
    return out;
}
