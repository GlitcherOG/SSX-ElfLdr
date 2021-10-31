// MemClr patch - relatively useless,
// disables memory clearing done by the game.

#include "patch.h"

#include <utils.h>
#include <codeutils.h>

#include "GameVersion.h"

using namespace elfldr;

struct MemclrPatch : public Patch {
	const char* GetName() const override {
		return "MemoryClear";
	}

	const char* GetIdentifier() const override {
		return "memclr";
	}

	bool IsCompatiable() const override {
		const auto& data = GetGameVersionData();

		if(data.region != GameRegion::NTSC)
			return false;

		if(data.game == Game::SSXOG || data.game == Game::SSXDVD)
			return true;

		return false;
	}

	void Apply() override {
		const auto& data = GetGameVersionData();

		switch(data.game) {
			case Game::SSXOG: {
				// NOP fill the direct memory clearing loop in bxPreInit()
				util::NopFill<10>(util::Ptr(0x0018a6d8));

				// NOP fill the memory clearing logic in CMN initheapdebug(),
				// as simply NOP filling the call causes the game to crash.
				util::NopFill<32>(util::Ptr(0x0018a294));

#ifdef EXPERIMENTAL
				util::DebugOut("Special case for Exp - killing MEM_init and initheapdebug");
				util::NopFill<6>(util::Ptr(0x0018a704));
#endif
			} break;

			// gaming
			case Game::SSXDVD: {
				// bxPreInit
				util::MemRefTo<std::uint32_t>(util::Ptr(0x00182b08)) = 0x00000000;

				// initheapdebug()
				// nopping out the writes themselves seems to be the best here.
				util::MemRefTo<std::uint32_t>(util::Ptr(0x001826c8)) = 0x00000000;
				util::MemRefTo<std::uint32_t>(util::Ptr(0x00182700)) = 0x00000000;
			} break;

				// SSX3 release does not actually clear the memory,
				// so patch data for it isn't needed!
				// go EA
		}
	}
};

// Register the patch into the patch system
static PatchRegistrar<MemclrPatch, 0x00> registrar;