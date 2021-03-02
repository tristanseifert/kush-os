#include "driver/Discovery.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <mpack/mpack.h>

using namespace libdriver;

/**
 * Releases pointers if we own them.
 */
DeviceDiscovered::~DeviceDiscovered() {
   if(this->ownsPtrs) {
        for(auto match : this->matches) {
            delete match;
        }
        this->matches.clear();
    }
}

/**
 * Serializes the device discovery message.
 */
void DeviceDiscovered::serialize(mpack_writer_t *writer) {
    mpack_start_map(writer, 2);

    // write the match objects
    mpack_write_cstr(writer, "matches");
    mpack_start_array(writer, this->matches.size());
    for(auto match : this->matches) {
        match->serialize(writer);
    }
    mpack_finish_array(writer);

    // also write the aux data
    mpack_write_cstr(writer, "aux");
    if(!this->aux.empty()) {
        mpack_write_bin(writer, reinterpret_cast<const char *>(this->aux.data()), this->aux.size());
    } else {
        mpack_write_nil(writer);
    }

    mpack_finish_map(writer);
}

/**
 * Deserializes a device discovery message.
 *
 * XXX: ensure allocated objects get deallocated
 */
void DeviceDiscovered::deserialize(mpack_node_t &root) {
    // release old shit
    if(this->ownsPtrs) {
        for(auto match : this->matches) {
            delete match;
        }
        this->matches.clear();
    }
    this->ownsPtrs = true;

    // deserialize matches
    auto matchesNode = mpack_node_map_cstr(root, "matches");
    const auto numMatches = mpack_node_array_length(matchesNode);

    for(size_t i = 0; i < numMatches; i++) {
        auto entry = mpack_node_array_at(matchesNode, i);
        auto match = DeviceMatch::fromNode(entry);

        if(!match) {
            throw std::runtime_error("failed to deserialize match struct");
        }

        this->matches.push_back(match);
    }

    // deserialize aux data 
    auto auxNode = mpack_node_map_cstr(root, "aux");
    if(mpack_node_type(auxNode) == mpack_type_bin) {
        this->aux.resize(mpack_node_bin_size(auxNode));

        auto data = mpack_node_bin_data(auxNode);
        if(data) {
            memcpy(this->aux.data(), reinterpret_cast<const std::byte *>(data), this->aux.size());
        }
    } else {
        this->aux.clear();
    }
}

const uint32_t DeviceDiscovered::getRpcType() const {
    return 'DDSC';
}



/**
 * Reads the `type` field from the given node to determine what kind of match structure to
 * deserialize.
 *
 * @return An initialized match struct subclass or `nullptr`.
 */
DeviceMatch *DeviceMatch::fromNode(mpack_node_t &node) {
    auto typeNode = mpack_node_map_cstr(node, "type");
    auto type = mpack_node_u8(typeNode);

    switch(type) {
        case DeviceNameMatch::kMatchType: {
            auto match = new DeviceNameMatch;
            match->deserialize(node);
            return match;
        }

        default:
            fprintf(stderr, "unsupported note type: %u\n", type);
            return nullptr;
    }
}



/**
 * Serialize a name match.
 */
void DeviceNameMatch::serialize(mpack_writer_t *writer) {
    mpack_start_map(writer, 2);

    // type is mandatory
    mpack_write_cstr(writer, "type");
    mpack_write_u8(writer, kMatchType);

    // write name string
    mpack_write_cstr(writer, "name");
    mpack_write_cstr_or_nil(writer, this->name.c_str());

    mpack_finish_map(writer); 
}


/**
 * Deserialize a name match.
 */
void DeviceNameMatch::deserialize(mpack_node_t &root) {
    auto nameNode = mpack_node_map_cstr(root, "name");

    auto namePtrLen = mpack_node_strlen(nameNode);
    auto namePtr = mpack_node_str(nameNode);

    if(namePtrLen && namePtr) {
        this->name = std::string(namePtr, namePtrLen);
    } else {
        this->name.clear();
    }
}

const uint32_t DeviceNameMatch::getRpcType() const {
    return 'DMN ';
}
