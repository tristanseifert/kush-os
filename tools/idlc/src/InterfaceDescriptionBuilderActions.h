#ifndef INTERFACEDESCRIPTIONBUILDERACTIONS_H
#define INTERFACEDESCRIPTIONBUILDERACTIONS_H

#include "IDLGrammar.h"
#include "InterfaceDescriptionBuilder.h"

namespace idl {
// base template for actions evaluated in the grammar
template<typename Rule> struct action {};

// handle the start and end of an interface
template<> struct action<interface_name> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.beginInterface(in.string());
    }
};
template<> struct action<interface_end> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.endInterface();
    }
};

// handle the start and end of a method
template<> struct action<method_name> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.beginMethod(in.string());
    }
};
template<> struct action<method_end> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.endMethod();
    }
};

// handle the sync/async nature of a method
template<> struct action<method_async_return_marker> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.setMethodAsync(true);
    }
};
template<> struct action<method_sync_return_marker> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.setMethodAsync(false);
    }
};

// handle the arguments to a method
template<> struct action<method_args_open> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.beginMethodParams();
    }
};
template<> struct action<method_args_close> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.endMethodParams();
    }
};

// handle the return values from a method
template<> struct action<method_return_open> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.beginMethodReturns();
    }
};
template<> struct action<method_return_close> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.endMethodReturns();
    }
};

// handle an argument (or return type)
template<> struct action<method_arg_name> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.setNextArgName(in.string());
    }
};
template<> struct action<method_arg_type> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.setNextArgTypename(in.string());
    }
};

// create an argument with the stored name/type and push it on the argument stack
template<> struct action<method_arg_end> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.pushNextArg();
    }
};

// handle method decorators
template<> struct action<decorator_key> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.setNextDecoratorKey(in.string());
    }
};
template<> struct action<decorator_value> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.setNextDecoratorValue(in.string());
    }
};
template<> struct action<decorator_close> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.pushNextDecorator();
    }
};

// Include files
template<> struct action<include_path> {
    template<typename ParseInput>
    static void apply(const ParseInput &in, Builder &b) {
        b.addIncludePath(in.string());
    }
};
}

#endif
