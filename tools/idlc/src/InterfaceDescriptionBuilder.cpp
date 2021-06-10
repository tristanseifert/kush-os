#include "InterfaceDescriptionBuilder.h"
#include "InterfaceDescription.h"

#include <cassert>
#include <iostream>

using namespace idl;

/**
 * Prepare the builder for re-use by clearing all internal state.
 */
void Builder::reset() {
    this->current.reset();
    this->allIds.clear();

    // argument state
    this->argContext = ArgContext::None;
    this->nextArgName.clear();
    this->nextArgTypename.clear();

    // decorator state
    this->nextDecoratorKey.clear();
    this->nextDecoratorValue.clear();
    this->decorators.clear();
}

/**
 * During finalization, simply copy out all interface descriptors.
 */
void Builder::finalize(std::vector<IDPointer> &out) {
    out.insert(out.end(), this->allIds.begin(), this->allIds.end());

    // prepare for re-use
    this->reset();
}



/**
 * Creates a new interface.
 */
void Builder::beginInterface(const std::string &name) {
    assert(!this->current);

    this->current = std::make_shared<InterfaceDescription>(name, this->filename);
}

/**
 * Finishes the current interface and adds it to the list of interfaces.
 */
void Builder::endInterface() {
    assert(this->current);

    this->allIds.emplace_back(std::move(this->current));
    this->current.reset();
}

/**
 * Creates a new named method.
 */
void Builder::beginMethod(const std::string &name) {
    assert(this->current);
    assert(!this->currentMethod);

    this->currentMethod = std::make_shared<InterfaceDescription::Method>(name);
}

/**
 * Sets the async flag of the given method.
 */
void Builder::setMethodAsync(const bool isAsync) {
    assert(this->currentMethod);

    this->currentMethod->async = isAsync;
}

/**
 * Finishes the current method and inserts it to the interface.
 */
void Builder::endMethod() {
    assert(this->current);
    assert(this->currentMethod);

    // decode decorators
    if(!this->decorators.empty()) {
        // identifier value for the method
        if(this->decorators.count("identifier")) {
            this->currentMethod->identifier = stoull(this->decorators["identifier"], nullptr, 0);
        }
    }
    this->decorators.clear();

    // add it
    this->current->addMethod(*this->currentMethod);
    this->currentMethod.reset();
}

/**
 * Begins the parameter section of a method.
 */
void Builder::beginMethodParams() {
    assert(this->current);
    assert(this->currentMethod);
    assert(this->argContext == ArgContext::None);

    this->argContext = ArgContext::Parameter;
}
/**
 * Finishes the method parameter section and assigns them.
 */
void Builder::endMethodParams() {
    assert(this->current);
    assert(this->currentMethod);
    assert(this->argContext == ArgContext::Parameter);

    // XXX: add to arg stack?
    this->argContext = ArgContext::None;
}

/**
 * Begins the return value section of a method.
 */
void Builder::beginMethodReturns() {
    assert(this->current);
    assert(this->currentMethod);
    assert(this->argContext == ArgContext::None);

    this->argContext = ArgContext::Return;
}
/**
 * Finishes the method parameter section and assigns them.
 */
void Builder::endMethodReturns() {
    assert(this->current);
    assert(this->currentMethod);
    assert(this->argContext == ArgContext::Return);

    // XXX: add to return stack?
    this->argContext = ArgContext::None;
}


/**
 * Sets the name for the next argument.
 */
void Builder::setNextArgName(const std::string &name) {
    this->nextArgName = name;
}
/**
 * Sets the typename for the next argument.
 */
void Builder::setNextArgTypename(const std::string &name) {
    this->nextArgTypename = name;
}
/**
 * Create an argument with the stored name and type info and add it to either the argument or the
 * return argument stack, depending on what part of a method declaration we're parsing.
 */
void Builder::pushNextArg() {
    assert(this->currentMethod);
    assert(this->argContext != ArgContext::None);

    // create the argument
    assert(!this->nextArgName.empty());
    assert(!this->nextArgTypename.empty());

    InterfaceDescription::Argument arg(this->nextArgName, this->nextArgTypename);

    switch(this->argContext) {
        case ArgContext::Parameter:
            this->currentMethod->addParameter(arg);
            break;
        case ArgContext::Return:
            this->currentMethod->addReturn(arg);
            break;

        // this is just to shut up clang despite the assert above
        case ArgContext::None: break;
    }

    // reset arg state
    this->nextArgName.clear();
    this->nextArgTypename.clear();
}


/**
 * Sets the key for the next decorator.
 */
void Builder::setNextDecoratorKey(const std::string &key) {
    this->nextDecoratorKey = key;
}
/**
 * Sets the value string for the next decorator.
 */
void Builder::setNextDecoratorValue(const std::string &value) {
    this->nextDecoratorValue = value;
}
/**
 * Inserts the last created decorator into the map.
 */
void Builder::pushNextDecorator() {
    assert(!this->nextDecoratorKey.empty());
    assert(!this->nextDecoratorValue.empty());

    // ensure we don't allow duplicate keys
    if(this->decorators.count(this->nextDecoratorKey)) {
        throw std::runtime_error("Duplicate decorator key");
    }

    this->decorators.emplace(this->nextDecoratorKey, this->nextDecoratorValue);

    // reset decorator state
    this->nextDecoratorKey.clear();
    this->nextDecoratorValue.clear();
}
