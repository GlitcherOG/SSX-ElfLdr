/**
 * SSX-Elfldr
 *
 * (C) 2021-2022 Lily/modeco80 <lily.modeco80@protonmail.ch>
 * under the terms of the MIT license.
 */

#ifndef ELFLDR_GAMEVERSION_H
#define ELFLDR_GAMEVERSION_H

#include <runtime/String.h>
#include <stdint.h>

namespace elfldr {

	/**
	 * Supported games.
	 */
	enum class Game : uint8_t {
		/**
		 * Invalid game.
		 */
		Invalid,

		/**
		 * SSX OG.
		 */
		SSXOG,

		/**
		 * SSX Tricky.
		 */
		SSXDVD,

		/**
		 * SSX 3.
		 */
		SSX3
	};

	enum class GameRegion : uint8_t {
		NTSC,
		PAL,
		NTSCJ,
		/**
		 * Used if a region is not applicable,
		 * for e.g. game coverdiscs.
		 */
		NotApplicable
	};

	enum class GameVersion : uint8_t {
		/**
		 * SSX OG Release
		 */
		SSXOG_10,

		// This space for Demo/earlier builds

		/**
		 * SSX Tricky Release
		 */
		SSXDVD_10 = 10,

		/**
		 * SSX Tricky Jampack demo.
		 * Built September 2001.
		 */
		SSXDVD_JAMPACK_DEMO,

		// This space for rent

		/**
		 * SSX 3 Release
		 */
		SSX3_10,

		/**
		 * SSX3 OPSM2 Demo.
		 * Built early June 2003, from a codebase from May 2003.
		 */
		SSX3_OPSM2_DEMO = 20, // unsupported but I'm giving it space here

		/**
		 * SSX3 Korean demo.
		 * Built June 2003.
		 */
		SSX3_KR_DEMO
	};

	// THIS TYPE IS PASSED IN THE ERL ABI,
	// ADD FIELDS AFTER THE EXISTING ONES!!!

	/**
	 * A package of all game version data.
	 */
	struct GameVersionData {
		/**
		 * What game.
		 */
		Game game;

		/**
		 * What region.
		 */
		GameRegion region;

		/**
		 * Version.
		 */
		GameVersion version;

		/**
		 * Get the game binary filename
		 */
		StringView GetGameBinary() const;

		/**
		 * Converts this version data to a string applicable
		 * to use in configuration.
		 * Not reentrant; copy data if you need to call
		 * this function multiple times.
		 */
		StringView ToString() const;

		// LoadFromString(), which parses each component to load the game/region/ver?
	};

	/**
	 * Probe the game version data.
	 */
	void ProbeVersion();

	const GameVersionData& GetGameVersionData();

} // namespace elfldr

#endif // ELFLDR_GAMEVERSION_H