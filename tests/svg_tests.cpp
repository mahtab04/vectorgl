#include <string>
#include <vectorgl/svg.hpp>
#include <vectorgl/svg_cache.hpp>

#include "test_utils.hpp"

int main()
{
    using namespace vectorgl;
    SvgImage image;
    SvgCache cache;
    for (const std::string xml : {"<svg <>", "<svg width=>", "<svg width=10/>", "<svg width='10>", "<svg><g></svg>",
                                  "<svg>", "<svg><!-- broken", "<?xml broken", "<", "<svg ='/>'>", "<svg value='<'/>"})
    {
        expect(!image.loadFromString(xml), "malformed SVG is rejected without hanging");
        expect(cache.loadFromString(xml) == SvgCache::kInvalidHandle, "cache rejects malformed SVG");
        expect(cache.count() == 0, "failed parse does not retain a cache slot");
    }
    std::string nested = "<svg>";
    for (int i = 0; i < 300; ++i)
        nested += "<g>";
    for (int i = 0; i < 300; ++i)
        nested += "</g>";
    nested += "</svg>";
    expect(!image.loadFromString(nested), "excessive nesting is rejected");
    expect(!image.loadFromString(std::string("<svg/>\0<g/>", 12)), "embedded null is rejected");
    expect(!image.loadFromString(std::string(16 * 1024 * 1024 + 1, ' ')), "oversized XML is rejected");
    expect(image.loadFromString("<?xml version='1.0'?><!-- comment --><svg width='20' height='30'>"
                                "<g><rect width='10' height='12'/></g></svg>"),
           "valid nested SVG loads");
    expectNear(image.width(), 20, 0.0001f, "SVG width survives validation");
    expectNear(image.height(), 30, 0.0001f, "SVG height survives validation");
    expect(image.loadFromString("<svg/>"), "valid self-closing SVG loads");
}
