#ifndef DB_DBPARSER_H
#define DB_DBPARSER_H

#include <memory>
#include <string_view>
#include <vector>

#include <toml++/toml.h>

class DriverDb;
class Driver;

/**
 * Parses an text based driver database description.
 *
 * These descriptions are TOML files.
 */
class DbParser {
    using DriverPtr = std::shared_ptr<Driver>;
    using DriverList = std::vector<DriverPtr>;

    public:
        /// Parse drivers from given file and add to database
        [[nodiscard]] bool parse(const std::string_view &path, DriverDb * _Nonnull db);

    private:
        /// Generate a driver object for an entry.
        [[nodiscard]] bool processEntry(const toml::table &, DriverList &);
        /// Processes a match structure
        [[nodiscard]] bool processMatch(const toml::table &, const DriverPtr &);
};

#endif
