#include "serialisation/editor_serialiser.h"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace spintool
{
    namespace
    {
        constexpr int indentation = 2;
    }

    Serialiser::~Serialiser()
    {
        Finalise();
    }

    std::unique_ptr<Serialiser> Serialiser::OpenFile(
        const std::filesystem::path& path
    )
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        auto serialiser = std::make_unique<Serialiser>();

        serialiser->m_out_stream = std::ofstream{
            path,
            std::ios::out | std::ios::trunc | std::ios::binary
        };

        if (!serialiser->m_out_stream.is_open())
        {
            throw std::runtime_error(
                "Unable to create JSON file: " + path.string()
            );
        }

        serialiser->m_json_writer = nlohmann::json::object();

        return serialiser;
    }

    std::unique_ptr<Serialiser> Serialiser::OpenFile(
        const std::filesystem::path& path,
        std::string_view filename
    )
    {
        std::filesystem::path output_path{path};
        output_path.append(filename);

        return OpenFile(output_path);
    }

    bool Serialiser::FileExists(const std::filesystem::path& path)
    {
        return std::filesystem::is_regular_file(path);
    }

    bool Serialiser::FileExists(
        const std::filesystem::path& path,
        std::string_view filename
    )
    {
        std::filesystem::path input_path{path};
        input_path.append(filename);

        return FileExists(input_path);
    }

    nlohmann::json& Serialiser::Writer()
    {
        return m_json_writer;
    }

    void Serialiser::Finalise()
    {
        if (!m_out_stream.is_open())
        {
            return;
        }

        m_out_stream << m_json_writer.dump(indentation);
        m_out_stream.flush();
        m_out_stream.close();
    }

    std::unique_ptr<Deserialiser> Deserialiser::OpenFile(
        const std::filesystem::path& path
    )
    {
        if (!std::filesystem::is_regular_file(path))
        {
            return nullptr;
        }

        std::ifstream input_stream{
            path,
            std::ios::in | std::ios::binary
        };

        if (!input_stream.is_open())
        {
            throw std::runtime_error(
                "Unable to open JSON file: " + path.string()
            );
        }

        input_stream.seekg(0, std::ios::end);
        const std::streampos file_size = input_stream.tellg();
        input_stream.seekg(0, std::ios::beg);

        if (file_size <= 0)
        {
            throw std::runtime_error(
                "JSON file is empty: " + path.string()
            );
        }

        nlohmann::json parsed_json;

        try
        {
            parsed_json = nlohmann::json::parse(
                input_stream,
                nullptr,
                true,
                true
            );
        }
        catch (const nlohmann::json::exception& error)
        {
            throw std::runtime_error(
                "Invalid JSON file " + path.string() + ": " + error.what()
            );
        }

        auto deserialiser = std::make_unique<Deserialiser>();
        deserialiser->m_json_reader = std::move(parsed_json);

        return deserialiser;
    }

    std::unique_ptr<Deserialiser> Deserialiser::OpenFile(
        const std::filesystem::path& path,
        std::string_view filename
    )
    {
        std::filesystem::path input_path{path};
        input_path.append(filename);

        return OpenFile(input_path);
    }

    bool Deserialiser::FileExists(const std::filesystem::path& path)
    {
        return std::filesystem::is_regular_file(path);
    }

    bool Deserialiser::FileExists(
        const std::filesystem::path& path,
        std::string_view filename
    )
    {
        std::filesystem::path input_path{path};
        input_path.append(filename);

        return FileExists(input_path);
    }

    const nlohmann::json& Deserialiser::Reader() const
    {
        return m_json_reader;
    }
}
