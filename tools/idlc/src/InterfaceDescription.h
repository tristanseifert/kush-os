#ifndef INTERFACEDESCRIPTION_H
#define INTERFACEDESCRIPTION_H

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace idl {
class Builder;
}

/**
 * An interface description encapsulates a parsed IDL file, and contains the methods that each
 * interface exports. Additionally, any metadata associated with the interface is captured as
 * well.
 */
class InterfaceDescription {
    friend class idl::Builder;

    /// random seed value to use when hashing interface names
    constexpr static const uint64_t kInterfaceNameHashSeed{0x9B06E367BED00BBBULL};

    public:
        /**
         * Defines an argument, which may either be passed into a method as its parameters, or
         * returned from it.
         */
        class Argument {
            friend class InterfaceDescription;
            friend class idl::Builder;

            public:
                Argument(const std::string &_name, const std::string &_typeName);

                /// Return the name of the argument
                constexpr inline auto &getName() const {
                    return this->name;
                }
                /// Return the type name of the argument
                constexpr inline auto &getTypeName() const {
                    return this->typeName;
                }
                /// Is the type primitive?
                constexpr inline auto isPrimitiveType() const {
                    return this->isPrimitive;
                }

                friend std::ostream& operator<<(std::ostream& os, const Argument& m);

            private:
                // name of the argument
                std::string name;
                // name of the type
                std::string typeName;

                // determine if the type is a primitive
                bool isPrimitive{false};

            private:
                // list of the names of all primitive types
                static const std::array<std::string, 13> gPrimitiveTypeNames;
        };

        /**
         * Defines a single callable method on an interface.
         *
         * Methods can be either asynchronous (meaning they have no return type, and return once
         * the request has been sent) or synchronous (meaning there is a reply, even an empty one,
         * that the call waits for before returning). The number of arguments to the call is
         * unlimited, while return values with more than one argument will be packaged into a
         * struct.
         */
        class Method {
            friend class InterfaceDescription;
            friend class idl::Builder;

            /// random seed value to use when hashing method names for method IDs
            constexpr static const uint64_t kMethodNameHashSeed{0xB64C6EF10B0E96F9ULL};

            public:
                /// Create a new method with the given name
                Method(const std::string &_name, const uint64_t identifier = 0);

                /// Return the name of the method.
                constexpr inline auto &getName() const {
                    return this->name;
                }
                /// Is the method asynchronous?
                constexpr inline auto isAsync() const {
                    return this->async;
                }
                /// Return the protocol message identifier for this call
                constexpr inline auto getIdentifier() const {
                    return this->identifier;
                }

                // Adds a new function call input parameter
                void addParameter(const Argument &param) {
                    this->params.emplace_back(param);
                }
                // Gets a read-only reference to the input parameters
                constexpr auto &getParameters() const {
                    return this->params;
                }
                // Adds a new function call return value
                void addReturn(const Argument &param) {
                    this->returns.emplace_back(param);
                }
                // Gets a read-only reference to the return values
                constexpr auto &getReturns() const {
                    return this->returns;
                }

                friend std::ostream& operator<<(std::ostream& os, const Method& m);

            private:
                // name of the method
                std::string name;
                // when true, the method has no return types
                bool async{false};

                // identifier unique in the interface to identify method
                uint64_t identifier{0};

                // list of parameters passed into the function
                std::vector<Argument> params;
                // list of values returned from the function (if synchronous)
                std::vector<Argument> returns;
        };

    public:
        /**
         * Create a new interface descriptor for an interface with a given name.
         */
        InterfaceDescription(const std::string &name, const std::string &filename = "");

        /**
         * Adds a new method to the interface.
         */
        void addMethod(const Method &m) {
            this->methods.emplace_back(m);
        }

        /// Return the name of the source file from which this interface was read
        constexpr auto &getSourceFilename() const {
            return this->filename;
        }
        /// Return the name of the interface.
        constexpr auto &getName() const {
            return this->name;
        }
        /// Return the interface's identifier
        constexpr auto getIdentifier() const {
            return this->identifier;
        }
        /// Return read-only access to each method
        constexpr auto &getMethods() const {
            return this->methods;
        }

        friend std::ostream& operator<<(std::ostream& os, const InterfaceDescription& intf);

    protected:
        /// filename from which this interface's description was read, if any
        std::string filename;
        /// Name of the interface
        std::string name;
        /// callable methods on the interface
        std::vector<Method> methods;

        /// identifier used for the Cap'n Proto structures (file id). generated from name
        uint64_t identifier{0};
};

#endif
