// PresetDump: generates the Factory/*.xml data files from the deterministic
// recipes. Usage: PresetDump <output-dir>. Run whenever recipes change:
//   ./PresetDump Factory
// The plugin itself never compiles recipes in — it only reads data files.
#include <cstdio>
#include "PresetRecipes.h"

int main(int argc, char** argv)
{
    if (argc < 2) { printf("usage: PresetDump <output-dir>\n"); return 2; }
    juce::File outDir(argv[1]);
    if (!outDir.isDirectory() && !outDir.createDirectory())
    {
        printf("FAIL: cannot create %s\n", argv[1]);
        return 1;
    }
    auto all = buildRecipePresets();
    int n = 0;
    for (auto& pr : all)
    {
        juce::XmlElement root("MONO");
        root.setAttribute("version", 1);
        root.setAttribute("name", juce::String(pr.fileName));
        for (auto& kv : pr.values)
        {
            auto* c = new juce::XmlElement("PARAM");
            c->setAttribute("id", juce::String(kv.first));
            c->setAttribute("value", juce::String(kv.second, 8));
            root.addChildElement(c);
        }
        juce::File f = outDir.getChildFile(juce::String(pr.fileName) + ".xml");
        if (!root.writeTo(f))
        {
            printf("FAIL: cannot write %s\n", f.getFullPathName().toRawUTF8());
            return 1;
        }
        ++n;
    }
    printf("wrote %d presets to %s\n", n, outDir.getFullPathName().toRawUTF8());
    return 0;
}
