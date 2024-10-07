/**
 * SSX-Elfldr
 *
 * (C) 2021-2022 Lily/modeco80 <lily.modeco80@protonmail.ch>
 * under the terms of the MIT license.
 */

// HostFS patch - allows game files to be loose
// on the filesystem, so it's easier to tinker with them.
//
// This patch also provides support for patching the game in such a way
// where world data can be plain files on disc, making them a LOT easier to modify.
//
// This currently works/has supported code paths for:
//	- SSX OG
//	- SSX Tricky (dubbed SSXDVD in code)
//  - SSX 3 KR Demo
//  - SSX 3 Retail
//

#include <mlstd/Assert.h>
#include <utils/CodeUtils.h>
#include <utils/GameVersion.h>
#include <utils/Utils.h>
#include <cstdio>

#include "../ElfPatch.h"

namespace elfldr {

	struct HostFsPatch : public ElfPatch {
		const char* GetName() const override {
			return "HostFS";
		}

		const char* GetIdentifier() const override {
			return "hostfs";
		}

		void Apply_SSXOG(const util::GameVersionData& data) {
			// ASYNCFILE_init usually gets "cd:".
			// We replace this with a string which will match "host",
			// after we..
			util::ReplaceString(util::Ptr(0x002c4e70), "host");

			// replace the strncmp length param constant in ASYNCFILE_init
			// from 6 to 4, so we can just use "host".
			util::MemRefTo<uint8_t>(util::Ptr(0x00238550)) = 0x4;

			// Write a new string in some slack space.

			util::WriteString(util::Ptr(0x002c5cc4), "host:");

			// Overwrite the pointer that the path "beautification" function uses to strcat()
			// "host0:" pointing it to our HostFS path instead.
			util::MemRefTo<uint32_t>(util::Ptr(0x002c59c8)) = 0x002c5cc4;

			// Write new IOP module paths
			util::WriteString(util::Ptr(0x002b3ab0), "host:data/modules/ioprp16.img");
			util::WriteString(util::Ptr(0x002b3b08), "host:data/modules/sio2man.irx");
			util::WriteString(util::Ptr(0x002b3b48), "host:data/modules/padman.irx");
			util::WriteString(util::Ptr(0x002b3b88), "host:data/modules/libsd.irx");
			util::WriteString(util::Ptr(0x002b3bc8), "host:data/modules/sdrdrv.irx");
			util::WriteString(util::Ptr(0x002b3c08), "host:data/modules/snddrv.irx"); // eac custom!!!
			util::WriteString(util::Ptr(0x002b3c48), "host:data/modules/mcman.irx");
			util::WriteString(util::Ptr(0x002b3c88), "host:data/modules/mcserv.irx");

			// This will completely disable loading worlds from BIG files.
			// Only enable this if you've extracted everything!!!
			util::NopFill<3>(util::Ptr(0x00187704)); // nop TheApp.MountWorld(...) in cGame::cGame()
			util::NopFill<2>(util::Ptr(0x001879f4)); // nop TheApp.UnmountWorld() in cGame::~cGame()

			// you know what? fuck you
			// *unbigs your files*
			// (i could patch bxMain() but cApplication::Run() never returns in release.)
			// util::NopFill<36>(util::Ptr(0x00183b68));

			// replace beq with bne, i hope this works LUL
			// util::MemRefTo<std::uint32_t>(util::Ptr(0x00238800)) = 0x14400017;

			// replace li 0x2 with 0x0
			// util::MemRefTo<std::uint32_t>(util::Ptr(0x00238770)) = 0x24120000;

			// Rewrite most of the cWorld path strings to remove the |.
			// This allows world files to either be loose or inside of the venue BIG files
			// (as long as the above code is not enabled).

			// I don't think this is ever used cause the game mounts the big
			// before calling cWorld::Load(). Maybe older versions of the function
			// mounted the BIG file from this path itself? We may never know
			// (unless said older builds leak of course..)
			util::WriteString(util::Ptr(0x002bdfc0), "data/models/%s.big");

			// Actually used paths.
			util::WriteString(util::Ptr(0x002bdfd8), "data/models/%s.wdx");
			util::WriteString(util::Ptr(0x002bdff0), "data/models/%s.wdf");
			util::WriteString(util::Ptr(0x002be008), "data/models/%s.wdr");
			util::WriteString(util::Ptr(0x002be020), "data/models/%s.wdv");
			util::WriteString(util::Ptr(0x002be038), "data/models/%s.wds");
			util::WriteString(util::Ptr(0x002be050), "data/models/%s.wfx");
			util::WriteString(util::Ptr(0x002be068), "data/models/%s.aip");
			util::WriteString(util::Ptr(0x002be080), "data/models/%s.ssh");
			util::WriteString(util::Ptr(0x002be098), "data/models/%sl.ssh");
			util::WriteString(util::Ptr(0x002b6d10), "data/models/%s_sky");
		}

		void Apply_SSXDVD(const util::GameVersionData& data) {
			switch(data.version) {
				case util::GameVersion::SSXDVD_10:
					switch(data.region) {
						case util::GameRegion::NTSC:
							// The new REAL library version introduced here onwards
							// doesn't hardcode the length of the host0 string, and trying to hardcode
							// the length results in crashing.
							// So we admit defeat and just give it what it wants, to a point.
							util::ReplaceString(util::Ptr(0x00387468), "host0:");
							util::WriteString(util::Ptr(0x003b9130), "host:");

							// Write new IOP module paths
							util::WriteString(util::Ptr(0x00387258), "host:data/modules/ioprp224.img");
							util::WriteString(util::Ptr(0x003872b0), "host:data/modules/sio2man.irx");
							util::WriteString(util::Ptr(0x003872f0), "host:data/modules/padman.irx");
							util::WriteString(util::Ptr(0x00387330), "host:data/modules/libsd.irx");
							util::WriteString(util::Ptr(0x00387370), "host:data/modules/snddrv.irx");
							util::WriteString(util::Ptr(0x003873b0), "host:data/modules/mcman.irx");
							util::WriteString(util::Ptr(0x003873f0), "host:data/modules/mcserv.irx");

							//Bigless Characters
							util::WriteString(util::Ptr(0x0039B420), "data/char/eddie_body.mpf");
							util::WriteString(util::Ptr(0x0039B440), "data/char/kaori_body.mpf");
							util::WriteString(util::Ptr(0x0039B460), "data/char/luther_body.mpf");
							util::WriteString(util::Ptr(0x0039B480), "data/char/mac_body.mpf");
							util::WriteString(util::Ptr(0x0039B498), "data/char/moby_body.mpf");
							util::WriteString(util::Ptr(0x0039B4B8), "data/char/zoe_body.mpf");
							util::WriteString(util::Ptr(0x0039B4D0), "data/char/jp_body.mpf");
							util::WriteString(util::Ptr(0x0039B4E8), "data/char/elise_body.mpf");
							util::WriteString(util::Ptr(0x0039B508), "data/char/psymon_body.mpf");
							util::WriteString(util::Ptr(0x0039B528), "data/char/seeiah_body.mpf");
							util::WriteString(util::Ptr(0x0039B548), "data/char/brodi_body.mpf");
							util::WriteString(util::Ptr(0x0039B568), "data/char/marisol_body.mpf");
							util::WriteString(util::Ptr(0x0039B588), "data/char/zz_mmm_body.mpf");
							util::WriteString(util::Ptr(0x0039B5A8), "data/char/eddie_head.mpf");
							util::WriteString(util::Ptr(0x0039B5C8), "data/char/kaori_head.mpf");
							util::WriteString(util::Ptr(0x0039B5E8), "data/char/luther_head.mpf");
							util::WriteString(util::Ptr(0x0039B608), "data/char/mac_head.mpf");
							util::WriteString(util::Ptr(0x0039B620), "data/char/moby_head.mpf");
							util::WriteString(util::Ptr(0x0039B640), "data/char/zoe_head.mpf");
							util::WriteString(util::Ptr(0x0039B658), "data/char/jp_head.mpf");
							util::WriteString(util::Ptr(0x0039B670), "data/char/elise_head.mpf");
							util::WriteString(util::Ptr(0x0039B690), "data/char/psymon_head.mpf");
							util::WriteString(util::Ptr(0x0039B6B0), "data/char/seeiah_head.mpf");
							util::WriteString(util::Ptr(0x0039B6D0), "data/char/brodi_head.mpf");
							util::WriteString(util::Ptr(0x0039B6F0), "data/char/marisol_head.mpf");
							util::WriteString(util::Ptr(0x0039B710), "data/char/zz_mmm_head.mpf");
							////util::WriteString(util::Ptr(0x0039B440), "data/char/board.mpf");
							
							for (int i = 1; i < 7; i++) {
								static char charPath[32] {};
								snprintf(&charPath[0], 32, "data/char/eddie%s_suit.ssh", i);
								util::WriteString(util::Ptr(0x0039B730 + (i-1) * 64), charPath);
								snprintf(&charPath[0], 32, "data/char/eddie%s_boot.ssh", i);
								util::WriteString(util::Ptr(0x0039B750+ 32 + (i-1) * 64), charPath);
							}

							
							// BIGless worlds
							// You'll need bigfile's bigextract to extract the world archives,
							// since they're c0fb BIG archives.

							// It seems they got a little mad at the mound of paths and made paths composed
							// via sprintf(), so this is actually quite a bit easier to do than OG.
							util::ReplaceString(util::Ptr(0x003a7bb8), "data/models/%s%s");

							// NOP world BIG file mounts, both for hardcoded SSXFE and the world's mounting
							util::NopFill<4>(util::Ptr(0x001862dc));
							util::MemRefTo<uint32_t>(util::Ptr(0x00263e1c)) = 0x00000000;
							break;

						default:
							break;
					}
					break;

				case util::GameVersion::SSXDVD_JAMPACK_DEMO:
					util::NopFill<84>(util::Ptr(0x001803ec));

					util::ReplaceString(util::Ptr(0x00381db8), "");
					util::ReplaceString(util::Ptr(0x00381dc0), "host:");

					util::ReplaceString(util::Ptr(0x00381b10), "host:data/modules/ioprp224.img");

					util::WriteString(util::Ptr(0x00381bd0), "host:data/modules/sio2man.irx");

					// util::WriteString(util::Ptr(0x00381bd0), "");
					util::WriteString(util::Ptr(0x00381c00), "host:data/modules/padman.irx");
					util::WriteString(util::Ptr(0x00381c90), "host:data/modules/libsd.irx");
					util::WriteString(util::Ptr(0x00381ca8), "host:data/modules/snddrv.irx");
					util::WriteString(util::Ptr(0x00381ca8), "host:data/modules/mcman.irx");
					util::WriteString(util::Ptr(0x00381d38), "host:data/modules/mcserv.irx");

					// MLSTD_VERIFY(false && "sorry, this doesnt work atm. please give me at least 5 minutes of research time");
					break;

				default:
					break;
			}
		}

		void Apply_SSX3(const util::GameVersionData& data) {
			// TODO: Bigless
			// (maybe as an ERL, we can patch loading chunks)

			// TODO: The game still passes some cdrom0: paths,
			// but it's only to some network module garbage,
			// so it's probably fine. if not we can fix it later!

			switch(data.version) {
				case util::GameVersion::SSX3_10:
					switch(data.region) {
						case util::GameRegion::NTSC:
							util::ReplaceString(util::Ptr(0x004a3ed8), "host0:");
							util::ReplaceString(util::Ptr(0x0048d9c8), "host:");

							// null terminate the ';1' so it isn't concatenated
							// to paths (HostFS doesn't need it)
							util::MemRefTo<uint8_t>(util::Ptr(0x004a3ea0)) = 0x0;

							util::WriteString(util::Ptr(0x00495828), "host:");
							break;

						default:
							break;
					}
					break;
					// doesn't work yet :( idk why
				case util::GameVersion::SSX3_KR_DEMO:
					util::ReplaceString(util::Ptr(0x004be1e8), "host0:");
					util::ReplaceString(util::Ptr(0x004b1580), "host:");
					util::ReplaceString(util::Ptr(0x004b0f88), "host:");
					util::ReplaceString(util::Ptr(0x0049efb8), "host:");

					util::MemRefTo<uint8_t>(util::Ptr(0x004be190)) = 0x0;

					// 0049efc8
					util::ReplaceString(util::Ptr(0x0049efc8), "%sdata/modules/");
					break;

				default:
					break;
			}
		}

		void Apply() override {
			const auto& data = util::GetGameVersionData();

			// TODO: it seems like sceCd* init hangs up on something, I suspect media type
			// 	(Older PCSX2 versions don't emulate the CD block as well and don't care)
			//		I'd like for the game to run with no disk in the drive though, so that will probs take work

			switch(data.game) {
				case util::Game::SSXOG:
					Apply_SSXOG(data);
					break;
				case util::Game::SSXDVD:
					Apply_SSXDVD(data);
					break;
				case util::Game::SSX3:
					Apply_SSX3(data);
					break;

				default:
					break;
			}
		}
	};

	// Register the patch into the patch system
	static PatchRegistrar<HostFsPatch, 0x01> registrar;
} // namespace elfldr
