/**
 * @file hsm_validation.hpp
 * @brief Input validation framework for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Fluent validation builders
 * - Common validators
 * - Validation result handling
 * - Custom validator support
 */

#ifndef HSM_VALIDATION_HPP
#define HSM_VALIDATION_HPP

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <optional>
#include <regex>
#include <algorithm>
#include <cctype>
#include <sstream>

namespace dfsmshsm {
namespace validation {

// ============================================================================
// Validation Error
// ============================================================================

/**
 * @brief Represents a single validation error
 */
struct ValidationError {
    std::string field;
    std::string message;
    std::string code;
    
    [[nodiscard]] std::string toString() const {
        if (field.empty()) {
            return message;
        }
        return field + ": " + message;
    }
};

// ============================================================================
// Validation Result
// ============================================================================

/**
 * @brief Result of validation operations
 */
class ValidationResult {
public:
    ValidationResult() = default;
    
    /**
     * @brief Check if validation passed
     */
    [[nodiscard]] bool isValid() const noexcept {
        return errors_.empty();
    }
    
    /**
     * @brief Check if validation failed
     */
    [[nodiscard]] bool hasErrors() const noexcept {
        return !errors_.empty();
    }
    
    /**
     * @brief Implicit bool conversion
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return isValid();
    }
    
    /**
     * @brief Get all errors
     */
    [[nodiscard]] const std::vector<ValidationError>& errors() const noexcept {
        return errors_;
    }
    
    /**
     * @brief Get first error message
     */
    [[nodiscard]] std::optional<std::string> firstError() const noexcept {
        if (errors_.empty()) return std::nullopt;
        return errors_.front().toString();
    }
    
    /**
     * @brief Get all error messages
     */
    [[nodiscard]] std::vector<std::string> errorMessages() const {
        std::vector<std::string> result;
        result.reserve(errors_.size());
        for (const auto& err : errors_) {
            result.push_back(err.toString());
        }
        return result;
    }
    
    /**
     * @brief Format all errors as string
     */
    [[nodiscard]] std::string format(std::string_view separator = "\n") const {
        std::ostringstream oss;
        for (std::size_t i = 0; i < errors_.size(); ++i) {
            if (i > 0) oss << separator;
            oss << errors_[i].toString();
        }
        return oss.str();
    }
    
    /**
     * @brief Add an error
     */
    void addError(ValidationError error) {
        errors_.push_back(std::move(error));
    }
    
    /**
     * @brief Add error with field and message
     */
    void addError(const std::string& field, const std::string& message,
                  const std::string& code = "") {
        errors_.push_back({field, message, code});
    }
    
    /**
     * @brief Merge another result into this one
     */
    void merge(const ValidationResult& other) {
        for (const auto& err : other.errors_) {
            errors_.push_back(err);
        }
    }
    
    /**
     * @brief Clear all errors
     */
    void clear() noexcept {
        errors_.clear();
    }
    
    /**
     * @brief Create valid result
     */
    [[nodiscard]] static ValidationResult ok() {
        return ValidationResult();
    }
    
    /**
     * @brief Create result with single error
     */
    [[nodiscard]] static ValidationResult fail(const std::string& message,
                                               const std::string& field = "",
                                               const std::string& code = "") {
        ValidationResult result;
        result.addError(field, message, code);
        return result;
    }

private:
    std::vector<ValidationError> errors_;
};

// ============================================================================
// Validator Interface
// ============================================================================

/**
 * @brief Generic validator function type
 */
template<typename T>
using Validator = std::function<ValidationResult(const T&)>;

// ============================================================================
// String Validators
// ============================================================================

namespace strings {

/**
 * @brief Validate string is not empty
 */
[[nodiscard]] inline Validator<std::string> notEmpty(const std::string& field = "") {
    return [field](const std::string& value) -> ValidationResult {
        if (value.empty()) {
            return ValidationResult::fail("Value cannot be empty", field, "EMPTY");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string length is at least min
 */
[[nodiscard]] inline Validator<std::string> minLength(std::size_t min, 
                                                       const std::string& field = "") {
    return [min, field](const std::string& value) -> ValidationResult {
        if (value.length() < min) {
            return ValidationResult::fail(
                "Length must be at least " + std::to_string(min), field, "MIN_LENGTH");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string length is at most max
 */
[[nodiscard]] inline Validator<std::string> maxLength(std::size_t max,
                                                       const std::string& field = "") {
    return [max, field](const std::string& value) -> ValidationResult {
        if (value.length() > max) {
            return ValidationResult::fail(
                "Length must be at most " + std::to_string(max), field, "MAX_LENGTH");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string length is within range
 */
[[nodiscard]] inline Validator<std::string> lengthBetween(std::size_t min, std::size_t max,
                                                          const std::string& field = "") {
    return [min, max, field](const std::string& value) -> ValidationResult {
        if (value.length() < min || value.length() > max) {
            return ValidationResult::fail(
                "Length must be between " + std::to_string(min) + " and " + 
                std::to_string(max), field, "LENGTH_RANGE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string matches regex pattern
 */
[[nodiscard]] inline Validator<std::string> matches(const std::string& pattern,
                                                     const std::string& field = "",
                                                     const std::string& message = "") {
    return [pattern, field, message](const std::string& value) -> ValidationResult {
        try {
            std::regex regex(pattern);
            if (!std::regex_match(value, regex)) {
                std::string msg = message.empty() ? 
                    "Value does not match required pattern" : message;
                return ValidationResult::fail(msg, field, "PATTERN");
            }
        } catch (const std::regex_error&) {
            return ValidationResult::fail("Invalid regex pattern", field, "REGEX_ERROR");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string contains only alphanumeric characters
 */
[[nodiscard]] inline Validator<std::string> alphanumeric(const std::string& field = "") {
    return [field](const std::string& value) -> ValidationResult {
        for (char c : value) {
            if (!std::isalnum(static_cast<unsigned char>(c))) {
                return ValidationResult::fail(
                    "Value must contain only alphanumeric characters", field, "ALPHANUMERIC");
            }
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string contains only alphabetic characters
 */
[[nodiscard]] inline Validator<std::string> alphabetic(const std::string& field = "") {
    return [field](const std::string& value) -> ValidationResult {
        for (char c : value) {
            if (!std::isalpha(static_cast<unsigned char>(c))) {
                return ValidationResult::fail(
                    "Value must contain only alphabetic characters", field, "ALPHABETIC");
            }
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string contains only digits
 */
[[nodiscard]] inline Validator<std::string> numeric(const std::string& field = "") {
    return [field](const std::string& value) -> ValidationResult {
        for (char c : value) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return ValidationResult::fail(
                    "Value must contain only numeric characters", field, "NUMERIC");
            }
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate email format (simple validation)
 */
[[nodiscard]] inline Validator<std::string> email(const std::string& field = "") {
    return matches(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})", 
                   field, "Invalid email format");
}

/**
 * @brief Validate string is in allowed list
 */
[[nodiscard]] inline Validator<std::string> oneOf(std::vector<std::string> allowed,
                                                   const std::string& field = "") {
    return [allowed = std::move(allowed), field](const std::string& value) -> ValidationResult {
        if (std::find(allowed.begin(), allowed.end(), value) == allowed.end()) {
            std::ostringstream oss;
            oss << "Value must be one of: ";
            for (std::size_t i = 0; i < allowed.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << allowed[i];
            }
            return ValidationResult::fail(oss.str(), field, "ONE_OF");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string starts with prefix
 */
[[nodiscard]] inline Validator<std::string> startsWith(const std::string& prefix,
                                                        const std::string& field = "") {
    return [prefix, field](const std::string& value) -> ValidationResult {
        if (value.length() < prefix.length() ||
            value.substr(0, prefix.length()) != prefix) {
            return ValidationResult::fail(
                "Value must start with '" + prefix + "'", field, "STARTS_WITH");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate string ends with suffix
 */
[[nodiscard]] inline Validator<std::string> endsWith(const std::string& suffix,
                                                      const std::string& field = "") {
    return [suffix, field](const std::string& value) -> ValidationResult {
        if (value.length() < suffix.length() ||
            value.substr(value.length() - suffix.length()) != suffix) {
            return ValidationResult::fail(
                "Value must end with '" + suffix + "'", field, "ENDS_WITH");
        }
        return ValidationResult::ok();
    };
}

} // namespace strings

// ============================================================================
// Numeric Validators
// ============================================================================

namespace numbers {

/**
 * @brief Validate number is at least min
 */
template<typename T>
[[nodiscard]] Validator<T> min(T minVal, const std::string& field = "") {
    return [minVal, field](const T& value) -> ValidationResult {
        if (value < minVal) {
            std::ostringstream oss;
            oss << "Value must be at least " << minVal;
            return ValidationResult::fail(oss.str(), field, "MIN");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate number is at most max
 */
template<typename T>
[[nodiscard]] Validator<T> max(T maxVal, const std::string& field = "") {
    return [maxVal, field](const T& value) -> ValidationResult {
        if (value > maxVal) {
            std::ostringstream oss;
            oss << "Value must be at most " << maxVal;
            return ValidationResult::fail(oss.str(), field, "MAX");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate number is in range [min, max]
 */
template<typename T>
[[nodiscard]] Validator<T> between(T minVal, T maxVal, const std::string& field = "") {
    return [minVal, maxVal, field](const T& value) -> ValidationResult {
        if (value < minVal || value > maxVal) {
            std::ostringstream oss;
            oss << "Value must be between " << minVal << " and " << maxVal;
            return ValidationResult::fail(oss.str(), field, "RANGE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate number is positive
 */
template<typename T>
[[nodiscard]] Validator<T> positive(const std::string& field = "") {
    return [field](const T& value) -> ValidationResult {
        if (value <= 0) {
            return ValidationResult::fail("Value must be positive", field, "POSITIVE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate number is non-negative
 */
template<typename T>
[[nodiscard]] Validator<T> nonNegative(const std::string& field = "") {
    return [field](const T& value) -> ValidationResult {
        if (value < 0) {
            return ValidationResult::fail("Value must be non-negative", field, "NON_NEGATIVE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate number is negative
 */
template<typename T>
[[nodiscard]] Validator<T> negative(const std::string& field = "") {
    return [field](const T& value) -> ValidationResult {
        if (value >= 0) {
            return ValidationResult::fail("Value must be negative", field, "NEGATIVE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate number equals expected
 */
template<typename T>
[[nodiscard]] Validator<T> equals(T expected, const std::string& field = "") {
    return [expected, field](const T& value) -> ValidationResult {
        if (value != expected) {
            std::ostringstream oss;
            oss << "Value must equal " << expected;
            return ValidationResult::fail(oss.str(), field, "EQUALS");
        }
        return ValidationResult::ok();
    };
}

} // namespace numbers

// ============================================================================
// Collection Validators
// ============================================================================

namespace collections {

/**
 * @brief Validate collection is not empty
 */
template<typename Container>
[[nodiscard]] Validator<Container> notEmpty(const std::string& field = "") {
    return [field](const Container& value) -> ValidationResult {
        if (value.empty()) {
            return ValidationResult::fail("Collection cannot be empty", field, "EMPTY");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate collection size is at least min
 */
template<typename Container>
[[nodiscard]] Validator<Container> minSize(std::size_t min, const std::string& field = "") {
    return [min, field](const Container& value) -> ValidationResult {
        if (value.size() < min) {
            return ValidationResult::fail(
                "Collection must have at least " + std::to_string(min) + " elements",
                field, "MIN_SIZE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate collection size is at most max
 */
template<typename Container>
[[nodiscard]] Validator<Container> maxSize(std::size_t max, const std::string& field = "") {
    return [max, field](const Container& value) -> ValidationResult {
        if (value.size() > max) {
            return ValidationResult::fail(
                "Collection must have at most " + std::to_string(max) + " elements",
                field, "MAX_SIZE");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate all elements pass validator
 */
template<typename Container, typename T>
[[nodiscard]] Validator<Container> all(Validator<T> elementValidator, 
                                        const std::string& field = "") {
    return [elementValidator, field](const Container& value) -> ValidationResult {
        ValidationResult result;
        std::size_t index = 0;
        for (const auto& elem : value) {
            auto elemResult = elementValidator(elem);
            if (elemResult.hasErrors()) {
                for (const auto& err : elemResult.errors()) {
                    std::string elemField = field.empty() ? 
                        "[" + std::to_string(index) + "]" :
                        field + "[" + std::to_string(index) + "]";
                    result.addError(elemField, err.message, err.code);
                }
            }
            ++index;
        }
        return result;
    };
}

} // namespace collections

// ============================================================================
// Validation Builder
// ============================================================================

/**
 * @brief Fluent validator builder
 */
template<typename T>
class ValidatorBuilder {
public:
    ValidatorBuilder() = default;
    
    /**
     * @brief Add a validator
     */
    ValidatorBuilder& add(Validator<T> validator) {
        validators_.push_back(std::move(validator));
        return *this;
    }
    
    /**
     * @brief Add validator with condition
     */
    ValidatorBuilder& addIf(bool condition, Validator<T> validator) {
        if (condition) {
            validators_.push_back(std::move(validator));
        }
        return *this;
    }
    
    /**
     * @brief Build and validate
     */
    [[nodiscard]] ValidationResult validate(const T& value) const {
        ValidationResult result;
        for (const auto& validator : validators_) {
            result.merge(validator(value));
        }
        return result;
    }
    
    /**
     * @brief Build validator function
     */
    [[nodiscard]] Validator<T> build() const {
        auto validators = validators_;
        return [validators](const T& value) -> ValidationResult {
            ValidationResult result;
            for (const auto& validator : validators) {
                result.merge(validator(value));
            }
            return result;
        };
    }

private:
    std::vector<Validator<T>> validators_;
};

/**
 * @brief Create validator builder
 */
template<typename T>
[[nodiscard]] ValidatorBuilder<T> validator() {
    return ValidatorBuilder<T>();
}

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Validate value with multiple validators
 */
template<typename T, typename... Validators>
[[nodiscard]] ValidationResult validate(const T& value, Validators... validators) {
    ValidationResult result;
    (result.merge(validators(value)), ...);
    return result;
}

/**
 * @brief Require value to be present (for optional)
 */
template<typename T>
[[nodiscard]] Validator<std::optional<T>> required(const std::string& field = "") {
    return [field](const std::optional<T>& value) -> ValidationResult {
        if (!value.has_value()) {
            return ValidationResult::fail("Value is required", field, "REQUIRED");
        }
        return ValidationResult::ok();
    };
}

/**
 * @brief Validate optional value if present
 */
template<typename T>
[[nodiscard]] Validator<std::optional<T>> ifPresent(Validator<T> validator) {
    return [validator](const std::optional<T>& value) -> ValidationResult {
        if (value.has_value()) {
            return validator(*value);
        }
        return ValidationResult::ok();
    };
}

} // namespace validation
} // namespace dfsmshsm

#endif // HSM_VALIDATION_HPP
