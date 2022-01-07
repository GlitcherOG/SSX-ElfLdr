#include <erl/ErlLoader.h>

#include "elf.h"

#include "utils/Allocator.h"
#include "HashTable.h"

#include <utils/Expected.h>
#include <utils/FioFile.h>
#include <ElfLoader.h>

//#define DEBUG

#define ERL_RELEASE_PRINTF(format, ...) elfldr::util::DebugOut("[LibERL] " format, ##__VA_ARGS__)

// if building debug, then we can afford to be a bit more chatty.
#ifdef DEBUG
	#define ERL_DEBUG_PRINTF(format, ...) elfldr::util::DebugOut("[LibERL] " format, ##__VA_ARGS__)
#else
	#define ERL_DEBUG_PRINTF(format, ...)
#endif

namespace elfldr::erl {

	// all possible ERL load errors
	enum class ErlLoadError {
		FileNotFound,	/** File does not exist on disk */
		NotElf,			/** Not a ELF file */
		NotMips,		/** ELF machine type is not MIPS R5900 */
		SizeMismatch,	/** Some data structure size didn't match up our structures */
		OomHit,			/** Could not allocate additional resources */
		NotRelocatable, /** ELF is not relocatable */
		NoSymbols,		/** No symbols */
		RelocationError /** Internal error relocating symbol */
	};

	/**
	 * Convert a ErlLoadError to string.
	 */
	static StringView LoadErrorToString(ErlLoadError e) {
		constexpr static const char* tab[] {
			"ERL file not found",
			"Not ELF file",
			"Not MIPS",
			"Critical structure size mismatch",
			"Out of memory (could not allocate structure)",
			"Not a relocatable ELF",
			"No symbols",
			"Internal error relocating symbol :("
		};
		return tab[static_cast<int>(e)];
	}

	template <class T>
	using ErlResult = util::Expected<T, ErlLoadError>;

	inline std::uint32_t Align(std::uint32_t alignment_value, int align) {
		align--;
		if(alignment_value & align) {
			alignment_value |= align;
			alignment_value++;
		}
		return alignment_value;
	}

	static int ApplyMipsReloc(std::uint8_t* reloc, int type, std::uint32_t addr) {
		std::uint32_t u_current_data;
		std::int32_t s_current_data;
		std::uint32_t newstate;

		if(((std::uintptr_t)reloc) & 0x3) {
			// printf("Unaligned reloc (%p) type=%d!\n", reloc, type);
			ERL_DEBUG_PRINTF("Unaligned relocation (%p), type %d ! Kinda sussy :)", reloc, type);
		}

		memcpy(&u_current_data, reloc, sizeof(std::uintptr_t));
		memcpy(&s_current_data, reloc, 4);

		switch(type) {
			case R_MIPS_32:
				newstate = s_current_data + addr;
				break;
			case R_MIPS_26:
				newstate = (u_current_data & 0xfc000000) | (((u_current_data & 0x03ffffff) + (addr >> 2)) & 0x3ffffff);
				break;
			case R_MIPS_HI16:
				newstate = (u_current_data & 0xffff0000) | ((((s_current_data << 16) >> 16) + (addr >> 16) + ((addr & 0xffff) >= 0x8000 ? 1 : 0)) & 0xffff);
				break;
			case R_MIPS_LO16:
				newstate = (u_current_data & 0xffff0000) | ((((s_current_data << 16) >> 16) + (addr & 0xffff)) & 0xffff);
				break;
			default:
				return -1;
		}

		memcpy(reloc, &newstate, sizeof(std::uint32_t));

		// dprintf("Changed data at %08X from %08X to %08X.\n", reloc, u_current_data, newstate);
		ERL_DEBUG_PRINTF("Changed %08X data from %08X to %08X.", reloc, u_current_data, newstate);
		return 0;
	}

	/**
	 * This class is actually what we allocate
	 * when we give an Image* to people.
	 * It includes all the private members we might need.
	 *
	 * Also provides the implementation of Image::ResolveSymbol and image loading to begin with..
	 */
	struct ImageImpl : public Image {
		~ImageImpl() override {
			ERL_DEBUG_PRINTF("~ImageImpl()");

			// destroy code and everything
			// This should only be done if the ERL isn't needed

			if(bytes)
				delete[] bytes;

			if(symtab)
				delete[] symtab;
		}

		ErlResult<void> Load(const char* path) {
			// Stage 1: open the file.
			// If this doesn't work, then the user specified an invalid path.
			util::FioFile file;

			file.Open(path, FIO_O_RDONLY);

			if(!file)
				return ErlLoadError::FileNotFound;

			// Stage 2: Read the ELF header.
			elf_header_t header {};
			file.Read(&header, sizeof(header));

			// Check ELF signature to make sure this is actually an ELF file.
			if(header.e_ident.cook.ei_magic[0] != '\77' && header.e_ident.cook.ei_magic[1] != 'E' && header.e_ident.cook.ei_magic[2] != 'L' && header.e_ident.cook.ei_magic[3] != 'F')
				return ErlLoadError::NotElf;

			// Check if this is a relocatable ELF.
			if(header.e_type != REL_TYPE)
				return ErlLoadError::NotRelocatable;

			// FIXME: Guard for r5900 mips ELF's.
			//  ORIGINAL ERL LIBRARY DOES NOT DO THIS!!!!!

			if(sizeof(elf_section_t) != header.e_shentsize)
				return ErlLoadError::SizeMismatch;

			std::uint32_t fullsize {};

			// Let's read the section table:
			auto* sections = new elf_section_t[header.e_shnum];

			// FIXME: These should probably be elf_section_t*?
			int strtab_index {};
			int symtab_index {};
			int linked_strtab_index {};

			// If allocation failed, report so.
			if(!sections)
				return ErlLoadError::OomHit;

			file.Seek(header.e_shoff, FIO_SEEK_SET);
			file.Read(sections, header.e_shnum * sizeof(elf_section_t));

			// Let's read .shstrtab:
			String shstrtab;
			shstrtab.Resize(sections[header.e_shstrndx].sh_size);

			file.Seek(sections[header.e_shstrndx].sh_offset, FIO_SEEK_SET);
			file.Read(shstrtab.data(), shstrtab.length());

			for(int i = 1; i < header.e_shnum; ++i) {
				auto& section = sections[i];
				StringView sectionName(&shstrtab[section.sh_name]);

				if(sectionName == ".symtab") {
					symtab_index = i;
					linked_strtab_index = section.sh_link;
				} else if(sectionName == ".strtab") {
					strtab_index = i;
				}

				if(section.sh_type == PROGBITS || section.sh_type == NOBITS) {
					fullsize = Align(fullsize, section.sh_addralign);
					section.sh_addr = fullsize;
					fullsize += section.sh_size;
				}

				ERL_DEBUG_PRINTF("Section %s: Offset %08X Size %08X Type %5i Link %5i Info %5i Align %5i EntSize %5i LoadAddr %08X", sectionName.CStr(), section.sh_offset, section.sh_size, section.sh_type, section.sh_link, section.sh_info, section.sh_addralign, section.sh_entsize, section.sh_addr);
			}

			// Sanity checks

			if(!symtab_index)
				return ErlLoadError::NotRelocatable;

			if(!strtab_index)
				return ErlLoadError::NotRelocatable;

			if(strtab_index != linked_strtab_index)
				return ErlLoadError::NotRelocatable;

			if(sizeof(elf_symbol_t) != sections[symtab_index].sh_entsize)
				return ErlLoadError::SizeMismatch;

			// Allocate byte buffer for the ERL data
			this->fullsize = fullsize;
			bytes = new std::uint8_t[fullsize];

			if(!bytes)
				return ErlLoadError::OomHit;

			// Initalize the symbol hash table, so we can
			symbol_table.Init(64);

			ERL_RELEASE_PRINTF("ERL Base Address: %08X", bytes);

			// Load in sections initially
			// PROGBITS and NOBITS
			for(int i = 1; i < header.e_shnum; ++i) {
				auto& section = sections[i];
				StringView sectionName(&shstrtab[section.sh_name]);

				switch(section.sh_type) {
					case PROGBITS:
						ERL_DEBUG_PRINTF("Reading section %s from ELF file", sectionName.CStr());
						file.Seek(section.sh_offset, FIO_SEEK_SET);
						file.Read(&bytes[section.sh_addr], section.sh_size);
						break;
					case NOBITS:
						ERL_DEBUG_PRINTF("Zeroing section %s", sectionName.CStr());
						memset(&bytes[section.sh_addr], 0x0, section.sh_size);
						break;
#ifdef DEBUG
					default:
						ERL_DEBUG_PRINTF("No action for section %s (type %d)", sectionName.CStr(), section.sh_type);
						break;
#endif
				}

				ERL_DEBUG_PRINTF("Processed section %s (at %08X)", sectionName.CStr(), &bytes[section.sh_addr]);
			}

			// Load .strtab
			strtab_names.Resize(sections[strtab_index].sh_size);

			file.Seek(sections[strtab_index].sh_offset, FIO_SEEK_SET);
			file.Read(strtab_names.data(), strtab_names.length());

			// Load .symtab
			symtab = new elf_symbol_t[sections[symtab_index].sh_size / sizeof(elf_symbol_t)];
			if(!symtab)
				return ErlLoadError::OomHit;

			file.Seek(sections[symtab_index].sh_offset, FIO_SEEK_SET);
			file.Read(symtab, sections[symtab_index].sh_size);

			// Load and parse the relocation section(s).

			for(int i = 0; i < header.e_shnum; ++i) {
				auto& section = sections[i];
				StringView sectionName(&shstrtab[section.sh_name]);

				if(section.sh_type != RELA)
					continue;

				// the section we'd be relocating.
				auto& relocating_section = sections[section.sh_info];

				ERL_DEBUG_PRINTF("Section %d (%s) contains rela reloc for %d (%s)", i, sectionName.CStr(), section.sh_info, StringView(&shstrtab[relocating_section.sh_name]));

				if(section.sh_entsize != sizeof(elf_reloca_t))
					return ErlLoadError::SizeMismatch;

				auto* reloc = new elf_reloca_t[(section.sh_size / section.sh_entsize)];
				if(!reloc)
					return ErlLoadError::OomHit;

				// read this reloc entry into data
				file.Seek(section.sh_offset, FIO_SEEK_SET);
				file.Read(reloc, section.sh_size);

				for(int j = 0; j < (section.sh_size / section.sh_entsize); ++j) {
					auto& r = reloc[j];
					int symbol_number = r.r_info >> 8;
					auto& sym = symtab[symbol_number];

					ERL_DEBUG_PRINTF("RelaEntry %3i: %08X %d Addend: %d sym: %d (%s): ", j, r.r_offset, r.r_info & 255, r.r_addend, symbol_number, StringView(&strtab_names[symtab[symbol_number].st_name]).CStr());

					switch(sym.st_info & 15) {
						case NOTYPE:
							ERL_DEBUG_PRINTF("Not handling NOTYPE for now cause it seems to be a dependent symbol thingy, and we're not doing that :)");
							break;
						case SECTION: {
							ERL_DEBUG_PRINTF("Internal reloc to section %d strndx %d (%s)", sym, sections[sym.st_shndx].sh_name, StringView(shstrtab.data() + sections[sym.st_shndx].sh_name).CStr());
							auto offset = relocating_section.sh_addr + r.r_offset;
							auto addr = reinterpret_cast<std::uintptr_t>(&bytes[sections[sym.st_shndx].sh_addr]);

							if(ApplyMipsReloc(&bytes[offset], r.r_info & 255, addr) < 0) {
								ERL_DEBUG_PRINTF("Error relocating");
								// cleanup
								delete[] reloc;
								return ErlLoadError::RelocationError;
							}
						} break;
						case OBJECT:
						case FUNC: {
							// TODO: Should probably implement dedupe, but whateverrrrrrrrrr

							ERL_DEBUG_PRINTF("Internal symbol relocation to %s", StringView(strtab_names.data() + sym.st_name).CStr());
							auto offset = relocating_section.sh_addr + sym.st_value;
							auto addr = reinterpret_cast<std::uintptr_t>(bytes + offset);
							ERL_DEBUG_PRINTF("Relocating at address %08X", addr);
							if(ApplyMipsReloc(&bytes[offset], r.r_info & 255, addr) < 0) {
								ERL_DEBUG_PRINTF("Error relocating");
								// cleanup
								delete[] reloc;
								return ErlLoadError::RelocationError;
							}
						} break;
						default:
							ERL_DEBUG_PRINTF("unknown relocation.");
							break;
					}
				}

				delete[] reloc;
			}

			// Let's export all symbols which should be exported.

			for(int i = 0; i < (sections[symtab_index].sh_size / sections[symtab_index].sh_entsize); ++i) {
				if(((symtab[i].st_info >> 4) == GLOBAL) || ((symtab[i].st_info >> 4) == WEAK)) {
					if((symtab[i].st_info & 15) != NOTYPE) {
						// get stuff
						String name(&strtab_names[symtab[i].st_name]);
						auto offset = sections[symtab[i].st_shndx].sh_addr + symtab[i].st_value;
						auto addr = reinterpret_cast<std::uintptr_t>(bytes + offset);

						ERL_RELEASE_PRINTF("Exporting symbol %s @ %08X", name.c_str(), addr);
						symbol_table.Set(name, addr);
					}
				}
			}

			for(int i = 0; i < 4; ++i)
				elfldr::FlushCaches();

			// Let's call _start
			auto start = reinterpret_cast<int (*)()>(ResolveSymbol("_start"));
			int res = start();

			ERL_DEBUG_PRINTF("erl's _start() returned %d", res);

			// No error occurred!
			return util::NO_ERROR<ErlLoadError>;
		}

		Symbol ResolveSymbol(const char* symbolName) override {
			// potential FIXME: Does this need to construct a temporary string *EVERY* time it gets called?
			//              that's a new[], memcpy(), and delete[] just to look up a symbol
			if(auto sym = symbol_table.MaybeGet(String(symbolName)); sym != nullptr) {
				return *sym;
			}

			return -1;
		}

		// IMPLEMENTATION DATA!!!!!!!!!!!!!!!!!!!!

		HashTable<String, Symbol> symbol_table;

		// TODO: Array<std::uint8_t>?
		std::uint8_t* bytes {};
		std::uint32_t fullsize {};

		String strtab_names;
		elf_symbol_t* symtab {};
	};

	// TODO: put this in utils? it might be ok to have round

	template <class ScopeExitT>
	struct ScopeExitGuard {
		constexpr ScopeExitGuard(ScopeExitT se)
			: scope_exited(util::Move(se)) {
		}

		// call the exit functor if we should
		constexpr ~ScopeExitGuard() {
			if(should_call)
				scope_exited();
		}

		/**
		 * Make this ScopeExitGuard not call the attached
		 * function on exit. This can be done for instance,
		 * if a function is successfully returning some heap
		 * value and doesn't need to free it anymore.
		 */
		void DontCall() {
			should_call = false;
		}

	   private:
		ScopeExitT scope_exited;
		bool should_call { true };
	};

	Image* LoadErl(const char* path) {
		auto* image = new ImageImpl();

		// This guard frees the image in case of a load error.
		ScopeExitGuard guard([&]() {
			delete image;
		});

		ERL_DEBUG_PRINTF("Attempting to load ERL \"%s\"", path);

		auto res = image->Load(path);
		if(res.HasError()) {
			// We know there's an error
			ERL_RELEASE_PRINTF("Error %d loading ERL \"%s\" (%s)", res.Error(), path, LoadErrorToString(res.Error()).CStr());
			return nullptr;
		}

		// OK!
		// We've loaded everything, and it's seemed to have worked!
		// Woo-hoo!
		guard.DontCall();
		return image;
	}

	void DestroyErl(Image* theImage) {
		delete theImage;
	}

} // namespace elfldr::erl