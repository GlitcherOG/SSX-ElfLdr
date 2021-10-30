// Patching ELF loader for (right now) SSX OG
// 2021 modeco80

#include <utils.h>

#include "patch.h"
#include <ElfLoader.h>

// is this include needed?
#include <codeutils.h>

// stuff.
#include <erl/ErlLoader.h>
#include <GameApi.h>

#include "GameVersion.h"

const char* gHostFsPath = "host:";
elfldr::ElfLoader gLoader;

void DoLoadElf(const elfldr::GameVersionData& gdata) {
	char elfPath[elfldr::util::MaxPath] {};
	strncpy(&elfPath[0], gHostFsPath, elfldr::util::MaxPath * sizeof(char));
	strcat(elfPath, gdata.GetGameBinary());

	ELFLDR_VERIFY(gLoader.LoadElf(elfPath));
}

void ApplyPatch(elfldr::Patch* patch) {
	ELFLDR_VERIFY(patch);

	elfldr::util::DebugOut("Applying patch \"%s\"...", patch->GetName());
	if(!patch->IsCompatiable()) {
		elfldr::util::DebugOut("Patch \"%s\" is incompatiable with the current game.", patch->GetName());
		return;
	}
	patch->Apply();
	elfldr::util::DebugOut("Finished applying patch \"%s\"...", patch->GetName());
}

int main() {
	elfldr::util::DebugOut("SSX-ElfLdr");

	// Init loader services.
	elfldr::InitLoader();

	elfldr::util::DebugOut("Probing game version...");
	elfldr::ProbeVersion();

	const auto& gdata = elfldr::GetGameVersionData();

	if(gdata.game == elfldr::Game::Invalid) {
		elfldr::util::DebugOut("No game that is supported could be detected alongside ElfLdr.");
		elfldr::util::DebugOut("Make sure elfldr is in the proper spot.");
		ELFLDR_VERIFY(false);
	}

	DoLoadElf(gdata);

	ApplyPatch(elfldr::GetPatchById(0x00));
	ApplyPatch(elfldr::GetPatchById(0x01));

#ifdef EXPERIMENTAL
	ApplyPatch(elfldr::GetPatchById(0xE0));
#endif

	char* argv[1];
	argv[0] = elfldr::util::UBCast<char*>(gHostFsPath); // I hate this

	// Execute the elf
	gLoader.ExecElf(sizeof(argv) / sizeof(argv[0]), argv);

	return 0;
}
