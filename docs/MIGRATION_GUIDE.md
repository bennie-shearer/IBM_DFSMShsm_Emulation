# IBM DFSMShsm (Hierarchy Storage Manager) Emulation - Migration Guide
Version 4.2.0

## v3.3.x to v4.0.2

### New Features
- `Result<T,E>` monadic error handling
- `ConfigValidator` for configuration validation
- 40+ `ErrorCode` enumeration values
- Extended test suite (40+ new tests)
- Enhanced documentation

### New Headers
```cpp
#include "result.hpp"           // Error handling
#include "config_validator.hpp" // Config validation
```

### Using Result<T,E>

```cpp
// Creating results
Result<int> success = Ok(42);
Result<int> failure = Err<int>(ErrorCode::NOT_FOUND, "Missing");

// Checking status
if (success.isOk()) {
    int val = success.value();
}

// Fluent error handling
getDataset("X")
    .onSuccess([](const DatasetInfo& ds) { use(ds); })
    .onError([](const Error& e) { log(e.message); });
```

### Config Validation

```cpp
auto& cfg = HSMConfig::instance();
cfg.set("migration.age_days", "30");

auto result = ConfigValidator::validate(cfg);
if (!result.valid) {
    for (const auto& e : result.errors) {
        std::cerr << e.toString() << std::endl;
    }
}
```

### Breaking Changes
- None (fully backward compatible)

## v3.2.x to v3.3.x

### Added Features
- Compression (RLE, LZ4, ZSTD)
- Prometheus metrics
- REST API server

### No Breaking Changes

## Build Requirements

- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.16+
- Threads library
