#include "DbParser.h"
#include "DriverDb.h"
#include "Driver.h"
#include "DeviceMatch.h"
#include "Log.h"

#include <toml++/toml.h>

/**
 * Reads the given TOML file and parses it, extracting the objects under the `drivers` key and
 * constructing a driver object from them. The detected drivers are then added to the driver
 * database for later matching.
 */
bool DbParser::parse(const std::string_view &path, DriverDb *db) {
    toml::table tbl;
    try {
        Trace("Parsing driver DB: %s", path.data());
        tbl = toml::parse_file(path);
    } catch(const toml::parse_error &err) {
        Warn("Failed to parse driver DB at %s: %s", path.data(), err.what());
        return false;
    }

    // process the entries contained within
    auto drivers = tbl["drivers"];
    auto driversArray = drivers.as_array();
    if(!drivers || !driversArray) {
        Warn("Driver DB at %s is invalid: %s", path.data(), "missing or invalid `drivers` key");
        return false;
    }

    DriverList temp;
    temp.reserve(driversArray->size());

    for(const auto &elem : *driversArray) {
        auto table = elem.as_table();
        if(!table) {
            Warn("Driver DB at %s is invalid: %s", path.data(),
                    "invalid driver object type (expected table)");
            return false;
        }

        if(!this->processEntry(*table, temp)) {
            return false;
        }
    }

    // if all entries were successfully processed, yeet them into driver db
    Trace("Read %lu drivers from %s", temp.size(), path.data());
    for(const auto &driver : temp) {
        db->addDriver(driver);
    }

    return true;
}

/**
 * Create a driver object from the given TOML object and add it to the provided database.
 *
 * @return Whether a driver object was successfully added to the db or not
 */
bool DbParser::processEntry(const toml::table &n, DriverList &dl) {
    // create driver with its path
    auto path = n["path"].value<std::string>();
    if(!path) return false;

    auto driver = std::make_shared<Driver>(*path);

    // build the match structures
    auto matches = n["match"].as_array();
    if(matches) {
        for(const auto &elem : *matches) {
            auto match = elem.as_table();
            if(!match) {
                Warn("Driver %s is invalid: %s", path->c_str(), "Invalid match object");
                return false;
            }

            // process this match object
            if(!this->processMatch(*match, driver)) {
                return false;
            }
        }
    } else {
        Warn("Driver %s is invalid: %s", path->c_str(), "Invalid or missing matches array");
        return false;
    }

    // optional keys
    if(n.contains("matchAll")) {
        driver->mustMatchAll = n["matchAll"].value<bool>().value();
    }

    // done
    dl.push_back(std::move(driver));
    return true;
}

/**
 * Processes a match structure for a driver.
 */
bool DbParser::processMatch(const toml::table &n, const DriverPtr &driver) {
    // determine the type of match
    if(n.contains("name")) {
        // read optional priority value
        int priority = 0;
        if(n.contains("priority")) {
            priority = n["priority"].value<int>().value_or(0);
        }

        // create and add match
        auto name = n["name"].value<std::string>();

        auto m = new DeviceNameMatch(*name, priority);
        driver->addMatch(m);
    }
    // PCI device
    else if(n.contains("pci")) {
        if(!this->processPciMatch(n, driver)) {
            Warn("Driver %s is invalid: %s", driver->getPath().c_str(),
                    "Failed to create PCI device match");
            return false;
        }
    }
    // we don't know this type
    else {
        Warn("Driver %s is invalid: %s", driver->getPath().c_str(),
                "Failed to determine match type");
        return false;
    }

    return true;
}

/**
 * Processes a PCI device match. These are tables with the following top level keys:
 *
 * - conjunction: If set, ALL conditions must be satisfied. Default false.
 * - priority: If set, the priority of this driver match.
 * - class: If specified, the device must have this value in the class ID field.
 * - subclass: If specified, device must have this value in the subclass ID field.
 * - device: If specified, an array of tables of vendor/product IDs, any of which will be a match.
 *   - vid: Vendor id; mandatory.
 *   - pid: If specified, a literal product id to match. If omitted, only the vendor id must match.
 *   - priority: Overrides the default priority of the driver if this vid/pid matches.
 */
bool DbParser::processPciMatch(const toml::table &n, const DriverPtr &driver) {
    // read optional priority value
    int priority = 0;
    if(n.contains("priority")) {
        priority = n["priority"].value<int>().value_or(0);
    }

    // get conjunction flag and create the PCI match boi
    const bool conjunction = n["conjunction"].value<bool>().value_or(false);

    auto m = new PciDeviceMatch(conjunction, priority);

    // construct the class match
    if(n.contains("class")) {
        m->setClassId(n["class"].value<uint8_t>().value_or(0xFF));
    }
    if(n.contains("subclass")) {
        m->setSubclassId(n["subclass"].value<uint8_t>().value_or(0xFF));
    }

    // construct the individual vid/pid matches
    if(n.contains("device")) {
        auto vids = n["device"].as_array();
        if(!vids) return false;

        for(const auto &elem : *vids) {
            auto match = elem.as_table();
            if(!match) return false;
            const auto &info = *match;

            m->addVidPidMatch(info["vid"].value<uint16_t>().value_or(0xFFFF),
                    info["pid"].value<uint16_t>(), info["priority"].value<int>());
        }
    }

    // finally, add to driver
    driver->addMatch(m);
    return true;
}
