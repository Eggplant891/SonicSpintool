#include "rom/rom_address_map.h"

#include "serialisation/editor_serialiser.h"

#include "json.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>

namespace spintool::rom
{
	namespace
	{
		struct GroupTable
		{
			bool loaded = false;
			std::unordered_map<Uint32, Uint32> relocations;
			std::unordered_map<Uint32, Uint32> usa_of;
			std::set<Uint32> reported_missing;
		};

		std::array<GroupTable, static_cast<size_t>(AddressGroup::Count)> s_tables;

		const char* GroupBaseName(AddressGroup group)
		{
			switch (group)
			{
			case AddressGroup::BonusStage: return "bonus_stage";
			case AddressGroup::Frontend: return "frontend";
			default: return "metadata";
			}
		}

		Uint32 ParseAddress(const nlohmann::json& value)
		{
			if (value.is_number_integer())
			{
				return value.get<Uint32>();
			}
			return static_cast<Uint32>(std::stoul(value.get<std::string>(), nullptr, 16));
		}
	}

	std::string AddressFilename(AddressGroup group, const std::string& version_id)
	{
		const std::string base = GroupBaseName(group);
		return version_id == "usa" ? base + ".json" : base + "_" + version_id + ".json";
	}

	void ClearAddressRelocations()
	{
		for (GroupTable& table : s_tables)
		{
			table = GroupTable{};
		}
	}

	bool LoadAddressRelocations(AddressGroup group, const std::filesystem::path& file)
	{
		GroupTable& table = s_tables[static_cast<size_t>(group)];
		table = GroupTable{};
		if (!Deserialiser::FileExists(file))
		{
			return false;
		}

		try
		{
			std::unique_ptr<Deserialiser> deserialiser = Deserialiser::OpenFile(file);
			if (deserialiser == nullptr)
			{
				return false;
			}
			const nlohmann::json& reader = deserialiser->Reader();
			const auto it = reader.find("rom_addresses");
			if (it == reader.end() || !it->is_object())
			{
				return false;
			}
			for (const auto& [usa, address] : it->items())
			{
				const Uint32 usa_address = static_cast<Uint32>(std::stoul(usa, nullptr, 16));
				const Uint32 relocated = ParseAddress(address);
				table.relocations[usa_address] = relocated;
				table.usa_of[relocated] = usa_address;
			}
			table.loaded = true;
			return true;
		}
		catch (const std::exception& error)
		{
			std::cerr << "Failed to load ROM addresses from " << file << ": " << error.what() << '\n';
			table = GroupTable{};
			return false;
		}
	}

	Uint32 UsaAddressOf(AddressGroup group, Uint32 address)
	{
		const GroupTable& table = s_tables[static_cast<size_t>(group)];
		const auto found = table.usa_of.find(address);
		return found != table.usa_of.end() ? found->second : address;
	}

	Uint32 Relocate(AddressGroup group, Uint32 usa_address)
	{
		GroupTable& table = s_tables[static_cast<size_t>(group)];
		if (!table.loaded)
		{
			return usa_address;
		}
		const auto found = table.relocations.find(usa_address);
		if (found != table.relocations.end())
		{
			return found->second;
		}
		if (table.reported_missing.insert(usa_address).second)
		{
			std::cerr << "No relocation for USA ROM address 0x" << std::hex << usa_address << std::dec
				<< " in " << GroupBaseName(group) << " metadata: using it as it is\n";
		}
		return usa_address;
	}
}
