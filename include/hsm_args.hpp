/**
 * @file hsm_args.hpp
 * @brief Command-line argument parsing for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Command-line argument parsing
 * - Option definitions with short/long forms
 * - Positional arguments
 * - Help generation
 * - Type conversion
 */

#ifndef HSM_ARGS_HPP
#define HSM_ARGS_HPP

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace dfsmshsm {
namespace args {

// ============================================================================
// Argument Types
// ============================================================================

/**
 * @brief Argument value type
 */
enum class ArgType {
    Flag,       // Boolean flag (no value)
    Value,      // Single value
    MultiValue  // Multiple values
};

/**
 * @brief Parsed argument value
 */
class ArgValue {
public:
    ArgValue() = default;
    
    /**
     * @brief Check if value is set
     */
    [[nodiscard]] bool isSet() const noexcept { return isSet_; }
    
    /**
     * @brief Get value count
     */
    [[nodiscard]] std::size_t count() const noexcept { return values_.size(); }
    
    /**
     * @brief Get as string
     */
    [[nodiscard]] std::string asString(const std::string& defaultValue = "") const {
        return values_.empty() ? defaultValue : values_[0];
    }
    
    /**
     * @brief Get as integer
     */
    [[nodiscard]] int asInt(int defaultValue = 0) const {
        if (values_.empty()) return defaultValue;
        try {
            return std::stoi(values_[0]);
        } catch (...) {
            return defaultValue;
        }
    }
    
    /**
     * @brief Get as int64
     */
    [[nodiscard]] int64_t asInt64(int64_t defaultValue = 0) const {
        if (values_.empty()) return defaultValue;
        try {
            return std::stoll(values_[0]);
        } catch (...) {
            return defaultValue;
        }
    }
    
    /**
     * @brief Get as double
     */
    [[nodiscard]] double asDouble(double defaultValue = 0.0) const {
        if (values_.empty()) return defaultValue;
        try {
            return std::stod(values_[0]);
        } catch (...) {
            return defaultValue;
        }
    }
    
    /**
     * @brief Get as boolean
     */
    [[nodiscard]] bool asBool(bool defaultValue = false) const {
        if (!isSet_) return defaultValue;
        if (values_.empty()) return true;  // Flag with no value
        
        std::string val = values_[0];
        std::transform(val.begin(), val.end(), val.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return val == "true" || val == "yes" || val == "1";
    }
    
    /**
     * @brief Get all values
     */
    [[nodiscard]] const std::vector<std::string>& values() const noexcept {
        return values_;
    }
    
    /**
     * @brief Implicit bool conversion (checks if set)
     */
    [[nodiscard]] explicit operator bool() const noexcept { return isSet_; }
    
    // Internal setters
    void set() { isSet_ = true; }
    void addValue(std::string value) { 
        isSet_ = true;
        values_.push_back(std::move(value)); 
    }

private:
    bool isSet_ = false;
    std::vector<std::string> values_;
};

// ============================================================================
// Argument Definition
// ============================================================================

/**
 * @brief Defines a command-line argument
 */
struct ArgDef {
    std::string name;           // Long name (e.g., "output")
    char shortName = '\0';      // Short name (e.g., 'o')
    std::string description;    // Help description
    ArgType type = ArgType::Value;
    std::string defaultValue;
    std::string metavar;        // Value placeholder in help (e.g., "FILE")
    bool required = false;
};

// ============================================================================
// Parse Result
// ============================================================================

/**
 * @brief Result of argument parsing
 */
class ParseResult {
public:
    ParseResult() = default;
    
    /**
     * @brief Check if parsing succeeded
     */
    [[nodiscard]] bool success() const noexcept { return error_.empty(); }
    
    /**
     * @brief Get error message
     */
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    
    /**
     * @brief Get named argument
     */
    [[nodiscard]] const ArgValue& get(const std::string& name) const {
        static ArgValue empty;
        auto it = args_.find(name);
        return it != args_.end() ? it->second : empty;
    }
    
    /**
     * @brief Check if argument is set
     */
    [[nodiscard]] bool has(const std::string& name) const {
        auto it = args_.find(name);
        return it != args_.end() && it->second.isSet();
    }
    
    /**
     * @brief Get positional arguments
     */
    [[nodiscard]] const std::vector<std::string>& positional() const noexcept {
        return positional_;
    }
    
    /**
     * @brief Get positional argument by index
     */
    [[nodiscard]] std::string positional(std::size_t index, 
                                         const std::string& defaultValue = "") const {
        return index < positional_.size() ? positional_[index] : defaultValue;
    }
    
    /**
     * @brief Array-style access
     */
    [[nodiscard]] const ArgValue& operator[](const std::string& name) const {
        return get(name);
    }
    
    // Internal setters
    void setError(std::string error) { error_ = std::move(error); }
    void setArg(const std::string& name, ArgValue value) { args_[name] = std::move(value); }
    void addPositional(std::string value) { positional_.push_back(std::move(value)); }
    ArgValue& argRef(const std::string& name) { return args_[name]; }

private:
    std::string error_;
    std::map<std::string, ArgValue> args_;
    std::vector<std::string> positional_;
};

// ============================================================================
// Argument Parser
// ============================================================================

/**
 * @brief Command-line argument parser
 */
class ArgParser {
public:
    /**
     * @brief Constructor
     * @param programName Program name for help
     * @param description Program description
     */
    explicit ArgParser(std::string programName = "", std::string description = "")
        : programName_(std::move(programName))
        , description_(std::move(description)) {}
    
    /**
     * @brief Add flag argument (boolean, no value)
     */
    ArgParser& addFlag(const std::string& name, char shortName = '\0',
                       const std::string& description = "") {
        ArgDef def;
        def.name = name;
        def.shortName = shortName;
        def.description = description;
        def.type = ArgType::Flag;
        args_.push_back(std::move(def));
        return *this;
    }
    
    /**
     * @brief Add option argument (with value)
     */
    ArgParser& addOption(const std::string& name, char shortName = '\0',
                         const std::string& description = "",
                         const std::string& defaultValue = "",
                         const std::string& metavar = "VALUE") {
        ArgDef def;
        def.name = name;
        def.shortName = shortName;
        def.description = description;
        def.type = ArgType::Value;
        def.defaultValue = defaultValue;
        def.metavar = metavar;
        args_.push_back(std::move(def));
        return *this;
    }
    
    /**
     * @brief Add required option
     */
    ArgParser& addRequired(const std::string& name, char shortName = '\0',
                           const std::string& description = "",
                           const std::string& metavar = "VALUE") {
        ArgDef def;
        def.name = name;
        def.shortName = shortName;
        def.description = description;
        def.type = ArgType::Value;
        def.metavar = metavar;
        def.required = true;
        args_.push_back(std::move(def));
        return *this;
    }
    
    /**
     * @brief Add multi-value option
     */
    ArgParser& addMulti(const std::string& name, char shortName = '\0',
                        const std::string& description = "",
                        const std::string& metavar = "VALUE") {
        ArgDef def;
        def.name = name;
        def.shortName = shortName;
        def.description = description;
        def.type = ArgType::MultiValue;
        def.metavar = metavar;
        args_.push_back(std::move(def));
        return *this;
    }
    
    /**
     * @brief Set positional argument description
     */
    ArgParser& setPositional(const std::string& name, 
                             const std::string& description = "") {
        positionalName_ = name;
        positionalDesc_ = description;
        return *this;
    }
    
    /**
     * @brief Parse command-line arguments
     * @param argc Argument count
     * @param argv Argument values
     * @return Parse result
     */
    [[nodiscard]] ParseResult parse(int argc, char* argv[]) const {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        return parse(args);
    }
    
    /**
     * @brief Parse from string vector
     */
    [[nodiscard]] ParseResult parse(const std::vector<std::string>& args) const {
        ParseResult result;
        
        // Initialize with defaults
        for (const auto& def : args_) {
            if (!def.defaultValue.empty()) {
                result.argRef(def.name).addValue(def.defaultValue);
            }
        }
        
        std::size_t i = 0;
        while (i < args.size()) {
            const std::string& arg = args[i];
            
            if (arg == "--help" || arg == "-h") {
                result.argRef("help").set();
                ++i;
                continue;
            }
            
            if (arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
                // Long option
                std::string name = arg.substr(2);
                std::string value;
                
                // Check for --name=value
                auto eqPos = name.find('=');
                if (eqPos != std::string::npos) {
                    value = name.substr(eqPos + 1);
                    name = name.substr(0, eqPos);
                }
                
                auto* def = findByName(name);
                if (!def) {
                    result.setError("Unknown option: --" + name);
                    return result;
                }
                
                if (def->type == ArgType::Flag) {
                    result.argRef(def->name).set();
                } else {
                    if (value.empty() && i + 1 < args.size()) {
                        value = args[++i];
                    }
                    if (value.empty()) {
                        result.setError("Option --" + name + " requires a value");
                        return result;
                    }
                    result.argRef(def->name).addValue(value);
                }
            }
            else if (arg.size() >= 2 && arg[0] == '-') {
                // Short option(s)
                for (std::size_t j = 1; j < arg.size(); ++j) {
                    char shortName = arg[j];
                    auto* def = findByShort(shortName);
                    
                    if (!def) {
                        result.setError(std::string("Unknown option: -") + shortName);
                        return result;
                    }
                    
                    if (def->type == ArgType::Flag) {
                        result.argRef(def->name).set();
                    } else {
                        // Value follows
                        std::string value;
                        if (j + 1 < arg.size()) {
                            value = arg.substr(j + 1);
                        } else if (i + 1 < args.size()) {
                            value = args[++i];
                        }
                        
                        if (value.empty()) {
                            result.setError(std::string("Option -") + shortName + 
                                          " requires a value");
                            return result;
                        }
                        result.argRef(def->name).addValue(value);
                        break;
                    }
                }
            }
            else {
                // Positional argument
                result.addPositional(arg);
            }
            
            ++i;
        }
        
        // Check required arguments
        for (const auto& def : args_) {
            if (def.required && !result.has(def.name)) {
                result.setError("Required option missing: --" + def.name);
                return result;
            }
        }
        
        return result;
    }
    
    /**
     * @brief Generate help text
     */
    [[nodiscard]] std::string help() const {
        std::ostringstream oss;
        
        // Usage line
        oss << "Usage: " << programName_;
        oss << " [OPTIONS]";
        if (!positionalName_.empty()) {
            oss << " [" << positionalName_ << "...]";
        }
        oss << "\n\n";
        
        // Description
        if (!description_.empty()) {
            oss << description_ << "\n\n";
        }
        
        // Options
        oss << "Options:\n";
        
        // Calculate column width
        std::size_t maxWidth = 0;
        for (const auto& def : args_) {
            std::size_t width = 4 + def.name.size();
            if (def.type != ArgType::Flag) {
                width += 1 + def.metavar.size();
            }
            maxWidth = std::max(maxWidth, width);
        }
        maxWidth = std::max(maxWidth, std::size_t(20));
        
        // Help option
        oss << "  -h, --help";
        oss << std::string(maxWidth - 8, ' ');
        oss << "Show this help message\n";
        
        // Other options
        for (const auto& def : args_) {
            oss << "  ";
            
            if (def.shortName != '\0') {
                oss << "-" << def.shortName << ", ";
            } else {
                oss << "    ";
            }
            
            oss << "--" << def.name;
            
            std::size_t width = 4 + def.name.size();
            if (def.type != ArgType::Flag) {
                oss << " " << def.metavar;
                width += 1 + def.metavar.size();
            }
            
            oss << std::string(maxWidth - width + 2, ' ');
            oss << def.description;
            
            if (def.required) {
                oss << " (required)";
            } else if (!def.defaultValue.empty()) {
                oss << " [default: " << def.defaultValue << "]";
            }
            
            oss << "\n";
        }
        
        // Positional arguments
        if (!positionalName_.empty()) {
            oss << "\nPositional Arguments:\n";
            oss << "  " << positionalName_;
            if (!positionalDesc_.empty()) {
                oss << std::string(maxWidth - positionalName_.size() + 2, ' ');
                oss << positionalDesc_;
            }
            oss << "\n";
        }
        
        return oss.str();
    }

private:
    std::string programName_;
    std::string description_;
    std::string positionalName_;
    std::string positionalDesc_;
    std::vector<ArgDef> args_;
    
    [[nodiscard]] const ArgDef* findByName(const std::string& name) const {
        for (const auto& def : args_) {
            if (def.name == name) return &def;
        }
        return nullptr;
    }
    
    [[nodiscard]] const ArgDef* findByShort(char shortName) const {
        for (const auto& def : args_) {
            if (def.shortName == shortName) return &def;
        }
        return nullptr;
    }
};

} // namespace args
} // namespace dfsmshsm

#endif // HSM_ARGS_HPP
