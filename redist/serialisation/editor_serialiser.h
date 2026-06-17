#pragma once

#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

namespace spintool
{
    class Serialiser
    {
    public:
        ~Serialiser();

        [[nodiscard]] static std::unique_ptr<Serialiser> OpenFile(
            const std::filesystem::path& path
        );

        [[nodiscard]] static std::unique_ptr<Serialiser> OpenFile(
            const std::filesystem::path& path,
            std::string_view filename
        );

        [[nodiscard]] static bool FileExists(
            const std::filesystem::path& path
        );

        [[nodiscard]] static bool FileExists(
            const std::filesystem::path& path,
            std::string_view filename
        );

        [[nodiscard]] nlohmann::json& Writer();

        void Finalise();

    private:
        std::ofstream m_out_stream;
        nlohmann::json m_json_writer = nlohmann::json::object();
    };

    class Deserialiser
    {
    public:
        [[nodiscard]] static std::unique_ptr<Deserialiser> OpenFile(
            const std::filesystem::path& path
        );

        [[nodiscard]] static std::unique_ptr<Deserialiser> OpenFile(
            const std::filesystem::path& path,
            std::string_view filename
        );

        [[nodiscard]] static bool FileExists(
            const std::filesystem::path& path
        );

        [[nodiscard]] static bool FileExists(
            const std::filesystem::path& path,
            std::string_view filename
        );

        [[nodiscard]] const nlohmann::json& Reader() const;

    private:
        nlohmann::json m_json_reader = nlohmann::json::object();
    };
}
