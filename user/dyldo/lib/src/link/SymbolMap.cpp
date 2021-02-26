#include "SymbolMap.h"
#include "Library.h"
#include "Linker.h"

#include "elf/ElfLibReader.h"

#include <cstring>

using namespace dyldo;

bool SymbolMap::gLogOverrides = false;

/**
 * Set up the symbol map.
 */
SymbolMap::SymbolMap() {
    int err;

    err = hashmap_create(kMapInitialSize, &this->map);
    if(err) {
        Linker::Abort("hashmap_create failed: %d", err);
    }

    err = hashmap_create(kOverrideMapInitialSize, &this->overridesMap);
    if(err) {
        Linker::Abort("hashmap_create failed: %d", err);
    }
}

/**
 * Adds a new symbol to the map.
 *
 * It's an error if the symbol is already globally defined; weak symbols can always be overridden
 * by global symbols, however.
 */
void SymbolMap::add(const char *name, const Elf32_Sym &sym, Library *library) {
    int err;
    const auto keyLen = strlen(name);

    // check if the symbol already exists
    auto ptr = hashmap_get(&this->map, name, keyLen);
    if(ptr) {
        auto existing = reinterpret_cast<Symbol *>(ptr);
        const auto bind = (existing->flags & SymbolFlags::BindMask);

        // if already defined as global, abort
        if(bind == SymbolFlags::BindGlobal) {
            Linker::Abort("duplicate definition for symbol '%s': in '%s' and '%s'", name,
                    existing->library->soname, library->soname);
        }

        // XXX: free this old entry (we'll overwrite it next)
        delete existing;
    }

    // allocate a new symbol entry
    auto info = new Symbol;
    info->name = name;
    info->library = library;

    info->length = sym.st_size;

    // get object type
    switch(ELF32_ST_TYPE(sym.st_info)) {
        /*
         * Data/object: address is a virtual address
         */
        case STT_OBJECT:
            info->address = library->reader->rebaseVmAddr(sym.st_value);
            info->flags |= SymbolFlags::TypeData;
            break;

        /**
         * Function (code)
         */
        case STT_FUNC:
            info->address = library->reader->rebaseVmAddr(sym.st_value);
            info->flags |= SymbolFlags::TypeData;
            break;

        default:
            Linker::Abort("unknown %s for '%s' in %s: %u (strtab %u, value %08x size %08x shdx %u)",
                    "symbol type", name, library->soname, ELF32_ST_TYPE(sym.st_info), sym.st_name,
                    sym.st_value, sym.st_size, sym.st_shndx);
    }

    // get binding type
    switch(ELF32_ST_BIND(sym.st_info)) {
        case STB_LOCAL:
            info->flags |= SymbolFlags::BindLocal;
            break;
        case STB_GLOBAL:
            info->flags |= SymbolFlags::BindGlobal;
            break;
        case STB_WEAK:
            info->flags |= SymbolFlags::BindWeakGlobal;
            break;
        default:
            Linker::Abort("unknown %s for '%s' in %s: %u (strtab %u, value %08x size %08x shdx %u)",
                    "binding type", name, library->soname, ELF32_ST_BIND(sym.st_info), sym.st_name,
                    sym.st_value, sym.st_size, sym.st_shndx);
    }

    // TODO: get visibility

    // insert it
    err = hashmap_put(&this->map, name, keyLen, info);
    if(err) {
        Linker::Abort("hashmap_put failed: %d", err);
    }
}

/**
 * Adds a symbol override for the given symbol.
 */
void SymbolMap::addOverride(const Symbol * _Nonnull inSym, const uintptr_t newAddr) {
    int err;

    // clone the symbol object
    auto oSym = new Symbol(*inSym);
    oSym->address = newAddr;

    if(gLogOverrides) {
        Linker::Trace("Overriding %s: %08x -> %08x", inSym->name, inSym->address, oSym->address);
    }

    // insert it
    err = hashmap_put(&this->overridesMap, inSym->name, strlen(inSym->name), oSym);
    if(err) {
        Linker::Abort("hashmap_put failed: %d", err);
    }
}

/**
 * Searches the symbol map for a symbol with the specified name, optionally limiting the search to
 * a particular library.
 *
 * @param searchIn If non-null, limit the search to symbols exported by the given library.
 */
SymbolMap::Symbol const * SymbolMap::get(const char *name, Library *searchIn) {
    const auto keyLen = strlen(name);
    void *element = nullptr;

    // have we an override?
    element = hashmap_get(&this->overridesMap, name, keyLen);
    if(element) {
        goto beach;
    }

    // does the map contain the item?
    element = hashmap_get(&this->map, name, keyLen);
    if(!element) {
        return nullptr;
    }

beach:;
    auto symbol = reinterpret_cast<const Symbol *>(element);
    if(searchIn && symbol->library != searchIn) {
        // symbol not exported by the library requested
        return nullptr;
    }

    // found the symbol
    return symbol;
}
