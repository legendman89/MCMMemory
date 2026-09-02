#pragma once

#include "profile/types.hpp"
#include "utils/helper.hpp"

namespace MCMMemory
{
    struct JSON
    {
        JSON() = delete;

        static bool IsValidUTF8(std::string_view a_text)
        {
            if (a_text.empty()) {
                return true;
            }
            return REX::W32::MultiByteToWideChar(utf8CodePage, errorOnInvalidCharacters, a_text.data(), static_cast<int>(a_text.size()), nullptr, 0) > 0;
        }

        // JSON accepts only UTF-8, so malformed game text is stored as marked hexadecimal bytes.
        // Loading reverses the encoding so MCM names and labels still match their original values.
        static std::string EncodeText(std::string_view a_text)
        {
            if (IsValidUTF8(a_text)) {
                return std::string(a_text);
            }

            std::string encoded(encodedTextPrefix);
            encoded.reserve(encodedTextPrefix.size() + a_text.size() * 2);
            for (const auto character : a_text) {
                const auto byte = static_cast<unsigned char>(character);
                encoded.push_back(hexDigits[byte >> 4]);
                encoded.push_back(hexDigits[byte & 0x0F]);
            }
            return encoded;
        }

        static std::optional<std::string> DecodeText(std::string_view a_text)
        {
            if (!a_text.starts_with(encodedTextPrefix)) {
                return std::nullopt;
            }

            const auto encoded = a_text.substr(encodedTextPrefix.size());
            if ((encoded.size() % 2) != 0) {
                return std::nullopt;
            }

            std::string decoded;
            decoded.reserve(encoded.size() / 2);
            for (size_t index = 0; index < encoded.size(); index += 2) {
                const int high = HexValue(encoded[index]);
                const int low = HexValue(encoded[index + 1]);
                if (high < 0 || low < 0) {
                    return std::nullopt;
                }
                decoded.push_back(static_cast<char>((high << 4) | low));
            }
            return decoded;
        }

        static nlohmann::json EncodeDocumentText(const nlohmann::json& a_document, std::string_view a_path = "$")
        {
            if (a_document.is_string()) {
                const auto& text = a_document.get_ref<const std::string&>();
                if (!IsValidUTF8(text)) {
                    logger::warn("Encoded malformed UTF-8 JSON text at {}", a_path);
                }
                return EncodeText(text);
            }
            if (a_document.is_array()) {
                auto encoded = nlohmann::json::array();
                for (size_t index = 0; index < a_document.size(); ++index) {
                    encoded.push_back(EncodeDocumentText(a_document[index], std::format("{}[{}]", a_path, index)));
                }
                return encoded;
            }
            if (a_document.is_object()) {
                auto encoded = nlohmann::json::object();
                for (auto item = a_document.begin(); item != a_document.end(); ++item) {
                    const auto key = EncodeText(item.key());
                    encoded[key] = EncodeDocumentText(item.value(), std::format("{}.{}", a_path, IsValidUTF8(item.key()) ? item.key() : "<invalid-key>"));
                }
                return encoded;
            }
            return a_document;
        }

        static nlohmann::json DecodeDocumentText(const nlohmann::json& a_document)
        {
            if (a_document.is_string()) {
                const auto& text = a_document.get_ref<const std::string&>();
                auto decoded = DecodeText(text);
                return decoded ? nlohmann::json(std::move(*decoded)) : a_document;
            }
            if (a_document.is_array()) {
                auto decoded = nlohmann::json::array();
                for (const auto& value : a_document) {
                    decoded.push_back(DecodeDocumentText(value));
                }
                return decoded;
            }
            if (a_document.is_object()) {
                auto decoded = nlohmann::json::object();
                for (auto item = a_document.begin(); item != a_document.end(); ++item) {
                    auto decodedKey = DecodeText(item.key());
                    decoded[decodedKey ? std::move(*decodedKey) : item.key()] = DecodeDocumentText(item.value());
                }
                return decoded;
            }
            return a_document;
        }

        static std::string Dump(const nlohmann::json& a_document, int a_indent = -1)
        {
            try {
                return a_document.dump(a_indent);
            }
            catch (const nlohmann::json::type_error& exception) {
                if (exception.id != 316) {
                    throw;
                }
            }
            return EncodeDocumentText(a_document).dump(a_indent);
        }

        template <class T>
        static bool ReadValue(const nlohmann::json& a_object, std::string_view a_name, T& a_value)
        {
            auto value = a_object.find(std::string(a_name));
            if (value == a_object.end()) {
                return false;
            }
            value->get_to(a_value);
            return true;
        }

        static std::optional<double> ReadNumber(const nlohmann::json& a_object, std::string_view a_name)
        {
            auto value = a_object.find(std::string(a_name));
            if (value == a_object.end() || !value->is_number()) {
                return std::nullopt;
            }
            return value->get<double>();
        }

        static std::optional<std::string> ReadString(const nlohmann::json& a_object, std::string_view a_name)
        {
            auto value = a_object.find(std::string(a_name));
            if (value == a_object.end() || !value->is_string()) {
                return std::nullopt;
            }
            return value->get<std::string>();
        }

        static bool WriteFile(const std::filesystem::path& a_path, const nlohmann::json& a_document)
        {
            std::error_code error;
            std::filesystem::create_directories(a_path.parent_path(), error);
            if (error) {
                logger::error("Failed to create JSON directory for {}: {}", ToUTF8(a_path), error.message());
                return false;
            }

            auto temporaryPath = a_path;
            temporaryPath += ".tmp";
            std::ofstream stream(temporaryPath, std::ios::trunc);
            if (!stream) {
                logger::error("Failed to open temporary JSON file {}", ToUTF8(temporaryPath));
                return false;
            }
            try {
                stream << Dump(a_document, 2);
            }
            catch (const std::exception& exception) {
                stream.close();
                std::filesystem::remove(temporaryPath, error);
                logger::error("Failed to serialize JSON file {}: {}", ToUTF8(a_path), exception.what());
                return false;
            }
            stream.flush();
            if (!stream) {
                logger::error("Failed to finish writing temporary JSON file {}", ToUTF8(temporaryPath));
                return false;
            }
            stream.close();

            if (!MoveFileExW(temporaryPath.c_str(), a_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                const auto windowsError = GetLastError();
                logger::error("Failed to replace JSON file {} with Windows error {}", ToUTF8(a_path), windowsError);
                std::filesystem::remove(temporaryPath, error);
                return false;
            }
            return true;
        }

        static nlohmann::json ToJson(const MCMSelection& a_selection)
        {
            return {
                { "modIndex", a_selection.modIndex },
                { "modName", a_selection.identity.modName },
                { "modID", a_selection.identity.modID },
                { "pageIndex", a_selection.pageIndex },
                { "pageName", a_selection.pageName },
                { "optionIndex", a_selection.optionIndex }
            };
        }

        static nlohmann::json ToJson(const CapturedSetting& a_setting, bool a_includeModIndex = true)
        {
            auto document = ToJson(a_setting.selection);
            if (!a_includeModIndex) {
                document.erase("modIndex");
            }
            document["sourceEventID"] = a_setting.sourceEventID;
            document["controlType"] = std::string(ControlTypeName(a_setting.type));
            document["optionLabel"] = a_setting.optionLabel;
            if (!a_setting.settingID.empty()) {
                document["settingID"] = a_setting.settingID;
            }
            document["stateName"] = a_setting.stateName;
            if (a_setting.pageScopedState) {
                document["pageScopedState"] = true;
            }
            document["value"] = a_setting.value;
            document["valueSource"] = a_setting.valueSource;
            document["identityComplete"] = a_setting.identityComplete;
            return document;
        }

    private:

        // Windows values for CP_UTF8 and MB_ERR_INVALID_CHARS.
        // There is currently a clash with CP_UTF8 in windows sdk.
        static constexpr uint32_t utf8CodePage{ 65001 };
        static constexpr uint32_t errorOnInvalidCharacters{ 0x00000008 };

        static int HexValue(char a_character)
        {
            if (a_character >= '0' && a_character <= '9') {
                return a_character - '0';
            }
            if (a_character >= 'A' && a_character <= 'F') {
                return a_character - 'A' + 10;
            }
            return -1;
        }

        static constexpr char encodedTextPrefixData[]{ "\0MCMMemoryBytes:" };

        static constexpr std::string_view encodedTextPrefix{ encodedTextPrefixData, sizeof(encodedTextPrefixData) - 1 };

        static constexpr std::string_view hexDigits{ "0123456789ABCDEF" };

    };
}
