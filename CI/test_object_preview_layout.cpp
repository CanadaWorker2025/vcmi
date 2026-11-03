#include "../client/windows/ObjectPreviewLayout.h"

#include <stdexcept>
#include <initializer_list>

int main()
{
	int tested = 0;
	for(int sw : {40, 200, 320, 640, 800, 1024, 1280, 1920})
		for(int sh : {60, 120, 240, 480, 600, 720, 1080})
			for(int nw : {1, 100, 250, 480, 700, 1800})
				for(int nh : {1, 100, 300, 500, 1000})
				{
					const auto result = ObjectPreview::layout(sw, sh, nw, nh);
					if(!result)
						continue;
					const auto & r = *result;
					if(r.width > sw - 40 || r.height > sh - 60 || r.width < nw || r.height < nh
						|| r.detailsWidth < 240 || r.detailsHeight < 140
						|| r.detailsX < 0 || r.detailsY < 0
						|| r.detailsX + r.detailsWidth > r.width || r.detailsY + r.detailsHeight > r.height
						|| !(r.detailsX >= nw + 20 || r.detailsY >= nh + 20))
						throw std::runtime_error("Preview clips native content, overlaps, or exceeds the screen");
					++tested;
				}
	if(!tested || ObjectPreview::layout(800, 600, 740, 500)
		|| ObjectPreview::layout(0, 0, 100, 100) || ObjectPreview::layout(800, 600, 0, 100))
		throw std::runtime_error("Native-only fallback is broken");
	return 0;
}
