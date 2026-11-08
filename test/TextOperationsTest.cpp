/*
 * TextOperationsTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "../lib/texts/TextOperations.h"

#include <boost/filesystem/path.hpp>

namespace test
{

TEST(TextOperationsTest, SaveNameStemPreservesUtf8)
{
	const std::vector<std::string> names = {
		"Quicksave",
		"\xe6\x9c\xab\xe6\x97\xa5\xe4\xb9\x8b\xe5\x88\x83",
		"Save_\xe6\x9c\xab\xe6\x97\xa5 01",
		"\xe6\x9c\xab\xe6\x97\xa5.backup",
		"caf\xc3\xa9_\xf0\x9f\x90\x89"
	};
	for(const auto & name : names)
	{
		const auto path = TextOperations::Utf8TofilesystemPath("Saves/" + name + ".vsgm1");
		const auto displayedName = TextOperations::filesystemPathToUtf8(path.stem());
		EXPECT_EQ(name, displayedName);
		EXPECT_TRUE(TextOperations::isValidUnicodeString(displayedName));
	}
}

#ifdef VCMI_WINDOWS
TEST(TextOperationsTest, NativeWindowsSaveNameUsesUtf8)
{
	const boost::filesystem::path path(L"Saves/\u672b\u65e5\u4e4b\u5203.vsgm1");
	EXPECT_EQ("\xe6\x9c\xab\xe6\x97\xa5\xe4\xb9\x8b\xe5\x88\x83",
		TextOperations::filesystemPathToUtf8(path.stem()));
}
#endif

}
