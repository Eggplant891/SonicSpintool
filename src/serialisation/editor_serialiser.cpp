#include "serialisation/editor_serialiser.h"

#include <fstream>

namespace spintool
{
    constexpr int indentation = 2;

    Serialiser::~Serialiser()
    {
        Finalise();
    }

    std::unique_ptr<Serialiser> Serialiser::OpenFile(const std::filesystem::path& path)
    {


        auto serialiser = std::make_unique<Serialiser>();

        serialiser->m_out_stream = std::ofstream{ path };
        serialiser->m_json_writer = nlohmann::json{};

        return serialiser;
    }

    std::unique_ptr<Serialiser> Serialiser::OpenFile(const std::filesystem::path &path, std::string_view filename)
    {
        std::filesystem::path in_path{path};
        in_path.append(filename);

        return OpenFile(in_path);
    }

    bool Serialiser::FileExists(const std::filesystem::path &path)
    {
       return std::filesystem::exists(path) == false;
    }

    bool Serialiser::FileExists(const std::filesystem::path &path, std::string_view filename)
    {
        std::filesystem::path in_path{path};
        in_path.append(filename);

        return FileExists(in_path);
    }

    nlohmann::json& Serialiser::Writer()
    {
        return m_json_writer;
    }

    void Serialiser::Finalise()
    {
        m_out_stream << m_json_writer.dump(indentation);
    }

    std::unique_ptr<Deserialiser> Deserialiser::OpenFile(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path) == false)
        {
            return nullptr;
        }

        auto deserialiser = std::make_unique<Deserialiser>();

        deserialiser->m_in_stream = std::ifstream{ path };
        deserialiser->m_json_reader = nlohmann::json{nlohmann::json::parse(deserialiser->m_in_stream)};

        return deserialiser;
    }

    std::unique_ptr<Deserialiser> Deserialiser::OpenFile(const std::filesystem::path &path, std::string_view filename)
    {
        std::filesystem::path in_path{path};
        in_path.append(filename);

        return OpenFile(in_path);
    }

    bool Deserialiser::FileExists(const std::filesystem::path &path)
    {
        return std::filesystem::exists(path) == false;
    }

    bool Deserialiser::FileExists(const std::filesystem::path &path, std::string_view filename)
    {
        std::filesystem::path in_path{path};
        in_path.append(filename);

        return FileExists(in_path);
    }

    const nlohmann::json& Deserialiser::Reader()
    {
        if (m_json_reader.empty())
        {
            return m_json_reader;
        }

        return m_json_reader.front();
    }
}
