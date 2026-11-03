#pragma once

#include <algorithm>
#include <optional>

namespace ObjectPreview
{
struct Layout
{
	int width, height;
	int detailsX, detailsY, detailsWidth, detailsHeight;
};

// Never shrink or clip the native popup to make room for supplemental information.
inline std::optional<Layout> layout(int screenWidth, int screenHeight, int nativeWidth, int nativeHeight)
{
	const int width = screenWidth - 40;
	const int height = screenHeight - 60;
	constexpr int gap = 20;
	if(width < 240 || height < 140 || nativeWidth <= 0 || nativeHeight <= 0 || nativeWidth > width || nativeHeight > height)
		return {};
	if(width - nativeWidth - gap >= 240)
	{
		const int detailsWidth = std::min(420, width - nativeWidth - gap);
		const int detailsHeight = std::min(520, height);
		return Layout{nativeWidth + gap + detailsWidth, std::max(nativeHeight, detailsHeight),
			nativeWidth + gap, 0, detailsWidth, detailsHeight};
	}
	if(height - nativeHeight - gap >= 140)
	{
		const int detailsWidth = std::max(nativeWidth, std::min(520, width));
		const int detailsHeight = std::min(320, height - nativeHeight - gap);
		return Layout{detailsWidth, nativeHeight + gap + detailsHeight,
			0, nativeHeight + gap, detailsWidth, detailsHeight};
	}
	return {};
}
}
