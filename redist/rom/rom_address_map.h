#pragma once

#include "types/rom_ptr.h"

#include <filesystem>
#include <string>

namespace spintool::rom
{
	// ROM addresses hard-coded in the tools are the USA ROM's. For another version
	// they are relocated through that version's tables, which map USA addresses to
	// its own. Each group has its own file set in metadata/:
	//   Common:     metadata.json / metadata_<version>.json  ("rom_addresses")
	//   BonusStage: bonus_stage.json / bonus_stage_<version>.json
	//   Frontend:   frontend.json / frontend_<version>.json (cutscenes, menus, title)
	// With no table for a group (the USA ROM), addresses are used as they are.
	enum class AddressGroup
	{
		Common,
		BonusStage,
		Frontend,
		Count
	};

	[[nodiscard]] std::string AddressFilename(AddressGroup group, const std::string& version_id);

	// Drops every table: addresses are then used as they are.
	void ClearAddressRelocations();
	// Loads the "rom_addresses" object of a file (hex USA address -> hex address).
	// Returns false, leaving the group's table empty, if the file has none.
	bool LoadAddressRelocations(AddressGroup group, const std::filesystem::path& file);

	[[nodiscard]] Uint32 Relocate(AddressGroup group, Uint32 usa_address);
	// The reverse: the USA address of one of this version's (itself if not listed).
	// For comparing addresses read from the ROM with USA constants.
	[[nodiscard]] Uint32 UsaAddressOf(AddressGroup group, Uint32 address);

	// Marks a USA ROM address constant that the code compares against UsaAddressOf()
	// (it stays a plain constant, so it works in constexpr and case labels). The
	// metadata generator collects the addresses of these, UsaAddress entries and the
	// RomAddress/BonusAddress/FrontendAddress calls.
#define SPINTOOL_USA_ADDRESS(group, address) (address)

	// A USA ROM address that can sit in a table built before a ROM is loaded,
	// relocated when it is read.
	struct UsaAddress
	{
		AddressGroup group;
		Uint32 usa_address;
		[[nodiscard]] Uint32 Get() const { return Relocate(group, usa_address); }
	};

	[[nodiscard]] inline Uint32 RomAddress(Uint32 usa_address) { return Relocate(AddressGroup::Common, usa_address); }
	[[nodiscard]] inline Uint32 BonusAddress(Uint32 usa_address) { return Relocate(AddressGroup::BonusStage, usa_address); }
	[[nodiscard]] inline Uint32 FrontendAddress(Uint32 usa_address) { return Relocate(AddressGroup::Frontend, usa_address); }
}
