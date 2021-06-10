#ifndef INTERFACEDESCRIPTIONBUILDER_H
#define INTERFACEDESCRIPTIONBUILDER_H

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "InterfaceDescription.h"

namespace idl {
/**
 * This class can be used in conjunction with the PEGTL parser to generate one or more interface
 * description objects from a run of the parser. The actions provided in
 * InterfaceDescriptionBuilderActions.h enable this behavior.
 */
class Builder {
    template<typename> friend struct action;

    using IDPointer = std::shared_ptr<InterfaceDescription>;
    using MethodPointer = std::shared_ptr<InterfaceDescription::Method>;

    /// Define the context in which an argument is to be defined
    enum class ArgContext {
        None, Parameter, Return,
    };

    public:
        // sets up a builder with the given source file name
        Builder(const std::string &_filename = "::memory") : filename(_filename) {}

        /// update the source file name
        void setFilename(const std::string &newFilename) {
            this->filename = newFilename;
        }

        // Finalize parsing and output the created interface descriptions.
        void finalize(std::vector<IDPointer> &outInterfaces);
        // Prepares the builder for reuse.
        void reset();

    private:
        /// Start defining an interface with the given name
        void beginInterface(const std::string &name);
        /// Finishes parsing the given interface.
        void endInterface();

        /// Begin parsing a method with the given name.
        void beginMethod(const std::string &name);
        /// Set the current method as async or sync.
        void setMethodAsync(const bool isAsync);
        /// Completes parsing a method.
        void endMethod();

        /// Begin parsing the parameter section of the method
        void beginMethodParams();
        /// Finish parsing the parameter section of the method
        void endMethodParams();
        /// Begin parsing the return arguments of the method
        void beginMethodReturns();
        /// Finish parsing the return section of the method
        void endMethodReturns();

        /// Define the name of an argument/return value
        void setNextArgName(const std::string &name);
        /// Define the type of an argument/return value
        void setNextArgTypename(const std::string &type);
        /// Pushes the argument with the name/typename just specified on of the arg/return stack
        void pushNextArg();

        /// Set the key of the next decorator value
        void setNextDecoratorKey(const std::string &key);
        /// Set the value of the next decorator
        void setNextDecoratorValue(const std::string &value);
        /// Pushes the decorator whose key/value we've collected on the decorator stack
        void pushNextDecorator();

    private:
        /// filename being processed
        std::string filename;

        /// Interface descriptor being currently parsed
        IDPointer current;
        /// list of all interface descriptors we've parsed
        std::list<IDPointer> allIds;

        /// current method being parsed
        MethodPointer currentMethod;

        /// context for the current argument set
        ArgContext argContext{ArgContext::None};
        /// name for the next argument
        std::string nextArgName;
        /// typename for the next argument
        std::string nextArgTypename;

        /// key for the next decorator
        std::string nextDecoratorKey;
        /// value for the next decorator
        std::string nextDecoratorValue;
        /// map of decorators collected for a function
        std::unordered_map<std::string, std::string> decorators;
};
}

#endif
