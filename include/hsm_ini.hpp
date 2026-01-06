/**
 * @file hsm_ini.hpp
 * @brief INI configuration file parsing for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - INI file parsing
 * - INI file writing
 * - Section and key management
 * - Type-safe value access
 */

#ifndef HSM_INI_HPP
#define HSM_INI_HPP

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace dfsmshsm {
namespace ini {

// ============================================================================
// INI Section
// ============================================================================

/**
 * @brief Represents a section in an INI file
 */
class IniSection {
public:
    using KeyValueMap = std::map<std::string, std::string>;
    
    IniSection() = default;
    explicit IniSection(std::string name) : name_(std::move(name)) {}
    
    /**
     * @brief Get section name
     */
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    
    /**
     * @brief Check if key exists
     */
    [[nodiscard]] bool contains(const std::string& key) const noexcept {
        return values_.find(key) != values_.end();
    }
    
    /**
     * @brief Get raw string value
     */
    [[nodiscard]] std::optional<std::string> get(const std::string& key) const noexcept {
        auto it = values_.find(key);
        if (it != values_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get value with default
     */
    [[nodiscard]] std::string get(const std::string& key, 
                                  const std::string& defaultValue) const noexcept {
        auto it = values_.find(key);
        return it != values_.end() ? it->second : defaultValue;
    }
    
    /**
     * @brief Get value as integer
     */
    [[nodiscard]] std::optional<int> getInt(const std::string& key) const noexcept {
        auto val = get(key);
        if (!val) return std::nullopt;
        try {
            return std::stoi(*val);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief Get integer with default
     */
    [[nodiscard]] int getInt(const std::string& key, int defaultValue) const noexcept {
        return getInt(key).value_or(defaultValue);
    }
    
    /**
     * @brief Get value as int64
     */
    [[nodiscard]] std::optional<int64_t> getInt64(const std::string& key) const noexcept {
        auto val = get(key);
        if (!val) return std::nullopt;
        try {
            return std::stoll(*val);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief Get int64 with default
     */
    [[nodiscard]] int64_t getInt64(const std::string& key, int64_t defaultValue) const noexcept {
        return getInt64(key).value_or(defaultValue);
    }
    
    /**
     * @brief Get value as double
     */
    [[nodiscard]] std::optional<double> getDouble(const std::string& key) const noexcept {
        auto val = get(key);
        if (!val) return std::nullopt;
        try {
            return std::stod(*val);
        } catch (...) {
            return std::nullopt;
        }
    }
    
    /**
     * @brief Get double with default
     */
    [[nodiscard]] double getDouble(const std::string& key, double defaultValue) const noexcept {
        return getDouble(key).value_or(defaultValue);
    }
    
    /**
     * @brief Get value as boolean
     * @note Recognizes: true/false, yes/no, 1/0, on/off
     */
    [[nodiscard]] std::optional<bool> getBool(const std::string& key) const noexcept {
        auto val = get(key);
        if (!val) return std::nullopt;
        
        std::string lower = *val;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
            return true;
        }
        if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
            return false;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Get boolean with default
     */
    [[nodiscard]] bool getBool(const std::string& key, bool defaultValue) const noexcept {
        return getBool(key).value_or(defaultValue);
    }
    
    /**
     * @brief Get value as string list (comma-separated)
     */
    [[nodiscard]] std::vector<std::string> getList(const std::string& key,
                                                   char delimiter = ',') const {
        auto val = get(key);
        if (!val) return {};
        
        std::vector<std::string> result;
        std::istringstream iss(*val);
        std::string item;
        
        while (std::getline(iss, item, delimiter)) {
            // Trim whitespace
            auto start = item.find_first_not_of(" \t");
            auto end = item.find_last_not_of(" \t");
            if (start != std::string::npos) {
                result.push_back(item.substr(start, end - start + 1));
            }
        }
        return result;
    }
    
    /**
     * @brief Set string value
     */
    void set(const std::string& key, const std::string& value) {
        values_[key] = value;
    }
    
    /**
     * @brief Set integer value
     */
    void set(const std::string& key, int value) {
        values_[key] = std::to_string(value);
    }
    
    /**
     * @brief Set int64 value
     */
    void set(const std::string& key, int64_t value) {
        values_[key] = std::to_string(value);
    }
    
    /**
     * @brief Set double value
     */
    void set(const std::string& key, double value) {
        std::ostringstream oss;
        oss << value;
        values_[key] = oss.str();
    }
    
    /**
     * @brief Set boolean value
     */
    void set(const std::string& key, bool value) {
        values_[key] = value ? "true" : "false";
    }
    
    /**
     * @brief Set list value
     */
    void setList(const std::string& key, const std::vector<std::string>& values,
                 char delimiter = ',') {
        std::ostringstream oss;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) oss << delimiter << ' ';
            oss << values[i];
        }
        values_[key] = oss.str();
    }
    
    /**
     * @brief Remove a key
     */
    bool remove(const std::string& key) {
        return values_.erase(key) > 0;
    }
    
    /**
     * @brief Clear all values
     */
    void clear() noexcept {
        values_.clear();
    }
    
    /**
     * @brief Get all keys
     */
    [[nodiscard]] std::vector<std::string> keys() const {
        std::vector<std::string> result;
        result.reserve(values_.size());
        for (const auto& [key, _] : values_) {
            result.push_back(key);
        }
        return result;
    }
    
    /**
     * @brief Get number of keys
     */
    [[nodiscard]] std::size_t size() const noexcept {
        return values_.size();
    }
    
    /**
     * @brief Check if section is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return values_.empty();
    }
    
    /**
     * @brief Access underlying map
     */
    [[nodiscard]] const KeyValueMap& values() const noexcept {
        return values_;
    }
    
    /**
     * @brief Array-style access
     */
    std::string& operator[](const std::string& key) {
        return values_[key];
    }

private:
    std::string name_;
    KeyValueMap values_;
};

// ============================================================================
// INI File
// ============================================================================

/**
 * @brief INI file parser and writer
 */
class IniFile {
public:
    using SectionMap = std::map<std::string, IniSection>;
    
    IniFile() = default;
    
    /**
     * @brief Parse INI from string
     * @param content INI content
     * @return Parsed IniFile
     */
    [[nodiscard]] static IniFile parse(std::string_view content);
    
    /**
     * @brief Load INI from file
     * @param path File path
     * @return Loaded IniFile or nullopt on error
     */
    [[nodiscard]] static std::optional<IniFile> load(const std::string& path);
    
    /**
     * @brief Save INI to file
     * @param path File path
     * @return true on success
     */
    [[nodiscard]] bool save(const std::string& path) const;
    
    /**
     * @brief Serialize to string
     */
    [[nodiscard]] std::string dump() const;
    
    /**
     * @brief Check if section exists
     */
    [[nodiscard]] bool hasSection(const std::string& name) const noexcept {
        return sections_.find(name) != sections_.end();
    }
    
    /**
     * @brief Get section (const)
     */
    [[nodiscard]] const IniSection* section(const std::string& name) const noexcept {
        auto it = sections_.find(name);
        return it != sections_.end() ? &it->second : nullptr;
    }
    
    /**
     * @brief Get or create section
     */
    [[nodiscard]] IniSection& section(const std::string& name) {
        auto it = sections_.find(name);
        if (it == sections_.end()) {
            it = sections_.emplace(name, IniSection(name)).first;
        }
        return it->second;
    }
    
    /**
     * @brief Get global section (values without section header)
     */
    [[nodiscard]] const IniSection& global() const noexcept {
        return global_;
    }
    
    /**
     * @brief Get global section (mutable)
     */
    [[nodiscard]] IniSection& global() noexcept {
        return global_;
    }
    
    /**
     * @brief Get value from section
     */
    [[nodiscard]] std::optional<std::string> get(const std::string& section,
                                                  const std::string& key) const noexcept {
        auto* sec = this->section(section);
        return sec ? sec->get(key) : std::nullopt;
    }
    
    /**
     * @brief Get value with default
     */
    [[nodiscard]] std::string get(const std::string& section, const std::string& key,
                                  const std::string& defaultValue) const noexcept {
        return get(section, key).value_or(defaultValue);
    }
    
    /**
     * @brief Set value in section
     */
    void set(const std::string& sectionName, const std::string& key, 
             const std::string& value) {
        section(sectionName).set(key, value);
    }
    
    /**
     * @brief Remove a section
     */
    bool removeSection(const std::string& name) {
        return sections_.erase(name) > 0;
    }
    
    /**
     * @brief Clear all sections
     */
    void clear() noexcept {
        sections_.clear();
        global_.clear();
    }
    
    /**
     * @brief Get all section names
     */
    [[nodiscard]] std::vector<std::string> sectionNames() const {
        std::vector<std::string> result;
        result.reserve(sections_.size());
        for (const auto& [name, _] : sections_) {
            result.push_back(name);
        }
        return result;
    }
    
    /**
     * @brief Get number of sections
     */
    [[nodiscard]] std::size_t sectionCount() const noexcept {
        return sections_.size();
    }
    
    /**
     * @brief Access underlying section map
     */
    [[nodiscard]] const SectionMap& sections() const noexcept {
        return sections_;
    }
    
    /**
     * @brief Array-style section access
     */
    IniSection& operator[](const std::string& name) {
        return section(name);
    }

private:
    IniSection global_;
    SectionMap sections_;
    
    static std::string trim(std::string_view str);
    static bool isComment(std::string_view line);
    static bool isSectionHeader(std::string_view line);
    static std::string extractSectionName(std::string_view line);
    static std::pair<std::string, std::string> parseKeyValue(std::string_view line);
};

// ============================================================================
// Implementation
// ============================================================================

inline std::string IniFile::trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(start, end - start + 1));
}

inline bool IniFile::isComment(std::string_view line) {
    auto trimmed = trim(line);
    return trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#';
}

inline bool IniFile::isSectionHeader(std::string_view line) {
    auto trimmed = trim(line);
    return trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']';
}

inline std::string IniFile::extractSectionName(std::string_view line) {
    auto trimmed = trim(line);
    if (trimmed.size() >= 2) {
        return trim(trimmed.substr(1, trimmed.size() - 2));
    }
    return "";
}

inline std::pair<std::string, std::string> IniFile::parseKeyValue(std::string_view line) {
    auto pos = line.find('=');
    if (pos == std::string_view::npos) {
        return {"", ""};
    }
    
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));
    
    // Remove quotes from value if present
    if (value.size() >= 2) {
        if ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
    
    return {key, value};
}

inline IniFile IniFile::parse(std::string_view content) {
    IniFile result;
    IniSection* currentSection = &result.global_;
    
    std::istringstream iss(std::string(content));
    std::string line;
    
    while (std::getline(iss, line)) {
        // Skip comments and empty lines
        if (isComment(line)) {
            continue;
        }
        
        // Check for section header
        if (isSectionHeader(line)) {
            std::string sectionName = extractSectionName(line);
            if (!sectionName.empty()) {
                currentSection = &result.section(sectionName);
            }
            continue;
        }
        
        // Parse key=value
        auto [key, value] = parseKeyValue(line);
        if (!key.empty()) {
            currentSection->set(key, value);
        }
    }
    
    return result;
}

inline std::optional<IniFile> IniFile::load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return parse(oss.str());
}

inline bool IniFile::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    
    file << dump();
    return file.good();
}

inline std::string IniFile::dump() const {
    std::ostringstream oss;
    
    // Write global section first (if not empty)
    if (!global_.empty()) {
        for (const auto& [key, value] : global_.values()) {
            oss << key << " = " << value << "\n";
        }
        if (!sections_.empty()) {
            oss << "\n";
        }
    }
    
    // Write named sections
    bool first = true;
    for (const auto& [name, section] : sections_) {
        if (!first) oss << "\n";
        first = false;
        
        oss << "[" << name << "]\n";
        for (const auto& [key, value] : section.values()) {
            oss << key << " = " << value << "\n";
        }
    }
    
    return oss.str();
}

// ============================================================================
// INI Builder
// ============================================================================

/**
 * @brief Fluent INI file builder
 */
class IniBuilder {
public:
    IniBuilder() = default;
    
    /**
     * @brief Start a new section
     */
    IniBuilder& beginSection(const std::string& name) {
        currentSection_ = name;
        return *this;
    }
    
    /**
     * @brief Add key-value to current section (or global if none)
     */
    template<typename T>
    IniBuilder& add(const std::string& key, const T& value) {
        if (currentSection_.empty()) {
            file_.global().set(key, value);
        } else {
            file_.section(currentSection_).set(key, value);
        }
        return *this;
    }
    
    /**
     * @brief Add list to current section
     */
    IniBuilder& addList(const std::string& key, const std::vector<std::string>& values,
                        char delimiter = ',') {
        if (currentSection_.empty()) {
            file_.global().setList(key, values, delimiter);
        } else {
            file_.section(currentSection_).setList(key, values, delimiter);
        }
        return *this;
    }
    
    /**
     * @brief Build the INI file
     */
    [[nodiscard]] IniFile build() {
        return std::move(file_);
    }

private:
    IniFile file_;
    std::string currentSection_;
};

/**
 * @brief Create INI builder
 */
[[nodiscard]] inline IniBuilder builder() {
    return IniBuilder();
}

} // namespace ini
} // namespace dfsmshsm

#endif // HSM_INI_HPP
