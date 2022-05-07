/**
 * SSX-Elfldr
 *
 * (C) 2021-2022 Lily/modeco80 <lily.modeco80@protonmail.ch>
 * under the terms of the MIT license.
 */

// MemClr patch - relatively useless,
// disables memory clearing done by the game.

#include <utils/CodeUtils.h>
#include <utils/GameVersion.h>
#include <utils/Utils.h>

#include "../Patch.h"

namespace elfldr {

	struct MemclrPatch : public Patch {
		const char* GetName() const override {
			return "MemoryClear";
		}

		const char* GetIdentifier() const override {
			return "memclr";
		}

		void Apply_SSXOG(const util::GameVersionData& versionData) {
			switch(versionData.version) {
				case util::GameVersion::SSXOG_10:
					switch(versionData.region) {
						case util::GameRegion::NTSC:
							// NOP fill the direct memory clearing loop in bxPreInit()
							util::NopFill<10>(util::Ptr(0x0018a6d8));

							util::MemRefTo<uint32_t>(util::Ptr(0x0018a2a0)) = 0x10000016; // b to the jr ra in initheapdebug() once the needed logic is done.
							util::MemRefTo<uint32_t>(util::Ptr(0x0018a2a4)) = 0x00000000; // nop out the new delay slot

							util::DebugOut("Disabling MEM_init and initheapdebug");
							util::NopFill<6>(util::Ptr(0x0018a704));
							break;

						default:
							break;
					}
					break;

				default:
					break;
			}
		}

		void Apply_SSXDVD(const util::GameVersionData& versionData) {
			switch(versionData.version) {
				case util::GameVersion::SSXDVD_10:
					switch(versionData.region) {
						case util::GameRegion::NTSC:
							// bxPreInit
							util::MemRefTo<uint32_t>(util::Ptr(0x00182b08)) = 0x00000000;

							// initheapdebug()
							// nopping out the writes themselves seems to be the best here.
							util::MemRefTo<uint32_t>(util::Ptr(0x001826c8)) = 0x00000000;
							util::MemRefTo<uint32_t>(util::Ptr(0x00182700)) = 0x00000000;
							break;
						default:
							break;
					}
					break;

				case util::GameVersion::SSXDVD_JAMPACK_DEMO:
					//ELFLDR_VERIFY(false && "doesnt work for now, please bug me later.");
					break;

				default:
					break;
			}
		}

		void Apply() override {
			const auto& data = util::GetGameVersionData();

			switch(data.game) {
				case util::Game::SSXOG:
					Apply_SSXOG(data);
					break;

				// gaming
				case util::Game::SSXDVD:
					Apply_SSXDVD(data);
					break;

					// SSX3 release does not actually clear the memory,
					// so patch data for it isn't needed!
					// go EA


				default:
					break;
			}
		}
	};

	// Register the patch into the patch system
	static PatchRegistrar<MemclrPatch, 0x00> registrar;

} // namespace elfldr