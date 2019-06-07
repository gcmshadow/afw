/*
 * This file is part of afw.
 *
 * Developed for the LSST Data Management System.
 * This product includes software developed by the LSST Project
 * (https://www.lsst.org).
 * See the COPYRIGHT file at the top-level directory of this distribution
 * for details of code ownership.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <utility>

#include "lsst/utils/python.h"
#include "lsst/pex/exceptions.h"

#include "lsst/afw/typehandling/GenericMap.h"
#include "lsst/afw/typehandling/python.h"

// From https://pybind11.readthedocs.io/en/stable/advanced/cast/stl.html#c-17-library-containers
// Not needed once we can use std::variant
namespace pybind11 {
namespace detail {
template <typename... Ts>
struct type_caster<boost::variant<Ts...>> : variant_caster<boost::variant<Ts...>> {};
template <>
struct visit_helper<boost::variant> {
    template <typename... Args>
    static auto call(Args&&... args) -> decltype(boost::apply_visitor(args...)) {
        return boost::apply_visitor(args...);
    }
};
}  // namespace detail
}  // namespace pybind11

namespace py = pybind11;
using namespace py::literals;

namespace lsst {
namespace afw {
namespace typehandling {

namespace {

// Type safety pointless in Python, use unsafe methods to avoid manual type checking
// https://pybind11.readthedocs.io/en/stable/advanced/classes.html#binding-protected-member-functions
template <typename K>
class Publicist : public MutableGenericMap<K> {
public:
    using typename GenericMap<K>::ConstValueReference;
    using GenericMap<K>::unsafeLookup;
    using MutableGenericMap<K>::unsafeErase;
};

template <typename K>
py::object get(GenericMap<K>& self, K const& key) {
    auto callable = static_cast<typename Publicist<K>::ConstValueReference (GenericMap<K>::*)(K) const>(
            &Publicist<K>::unsafeLookup);
    return py::cast((self.*callable)(key));
};

template <typename K>
void declareGenericMap(utils::python::WrapperCollection& wrappers, std::string const& suffix,
                       std::string const& key) {
    using Class = GenericMap<K>;
    using PyClass = py::class_<Class, std::shared_ptr<Class>>;

    std::string className = "GenericMap" + suffix;
    // Give the class a custom docstring to avoid confusing Python users
    std::string docstring =
            "An abstract `~collections.abc.Mapping` for use when sharing a map between C++ and Python.\n" +
            declareGenericMapRestrictions(className, key);
    wrappers.wrapType(PyClass(wrappers.module, className.c_str(), docstring.c_str()), [](auto& mod,
                                                                                         auto& cls) {
        // __str__ easier to implement in Python
        // __repr__ easier to implement in Python
        // __eq__ easier to implement in Python
        // __ne__ easier to implement in Python
        // For unknown reasons, overload_cast doesn't work
        // cls.def("__contains__", py::overload_cast<K const&>(&Class::contains, py::const_), "key"_a);
        cls.def("__contains__", static_cast<bool (Class::*)(K const&) const>(&Class::contains), "key"_a);

        cls.def("__getitem__",
                [](Class& self, K const& key) {
                    try {
                        return get(self, key);
                    } catch (pex::exceptions::OutOfRangeError const& e) {
                        // pybind11 doesn't seem to recognize chained exceptions
                        std::stringstream buffer;
                        buffer << "Unknown key: " << key;
                        std::throw_with_nested(py::key_error(buffer.str()));
                    }
                },
                // Prevent segfaults when assigning a key<Storable> to Python variable, then deleting from map
                // No existing code depends on being able to modify an item stored by value
                "key"_a, py::return_value_policy::copy);
        cls.def("get",
                [](Class& self, K const& key, py::object const& def) {
                    try {
                        return get(self, key);
                    } catch (pex::exceptions::OutOfRangeError const& e) {
                        return def;
                    }
                },
                // Prevent segfaults when assigning a key<Storable> to Python variable, then deleting from map
                // No existing code depends on being able to modify an item stored by value
                "key"_a, "default"_a = py::none(), py::return_value_policy::copy);
        cls.def("__iter__",
                [](Class const& self) { return py::make_iterator(self.keys().begin(), self.keys().end()); },
                py::keep_alive<0, 1>());
        cls.def("__len__", &Class::size);
        cls.def("__bool__", [](Class const& self) { return !self.empty(); });
        // Can't wrap keys directly because pybind11 always copies vectors, so it won't be a view
        // items easier to implement in Python
        // values easier to implement in Python
    });
}

template <typename V, class PyClass>
void declareMutableGenericMapTypedMethods(PyClass& cls) {
    using Class = typename PyClass::type;
    cls.def("__setitem__",
            [](Class& self, typename Class::key_type const& key, V const& value) {
                // Need to delete previous key, which may not be of type V
                // TODO: this method provides only basic exception safety
                if (self.contains(key)) {
                    auto callable = &Publicist<typename Class::key_type>::unsafeErase;
                    (self.*callable)(key);
                }
                self.insert(key, value);
            },
            "key"_a, "value"_a);
    // setdefault easier to implement in Python
    // pop easier to implement in Python
}

template <typename K>
void declareMutableGenericMap(utils::python::WrapperCollection& wrappers, std::string const& suffix,
                              std::string const& key) {
    using Class = MutableGenericMap<K>;
    using PyClass = py::class_<Class, std::shared_ptr<Class>, GenericMap<K>>;

    std::string className = "MutableGenericMap" + suffix;
    // Give the class a custom docstring to avoid confusing Python users
    std::string docstring =
            "An abstract `~collections.abc.MutableMapping` for use when sharing a map between C++ and "
            "Python.\n" +
            declareGenericMapRestrictions(className, key);
    wrappers.wrapType(PyClass(wrappers.module, className.c_str(), docstring.c_str()),
                      [](auto& mod, auto& cls) {
                          // Don't rewrap members of GenericMap
                          declareMutableGenericMapTypedMethods<std::shared_ptr<Storable>>(cls);
                          declareMutableGenericMapTypedMethods<bool>(cls);
                          declareMutableGenericMapTypedMethods<std::int32_t>(cls);
                          declareMutableGenericMapTypedMethods<std::int64_t>(cls);
                          declareMutableGenericMapTypedMethods<float>(cls);
                          declareMutableGenericMapTypedMethods<double>(cls);
                          declareMutableGenericMapTypedMethods<std::string>(cls);
                          cls.def("__delitem__",
                                  [](Class& self, K const& key) {
                                      if (self.contains(key)) {
                                          auto callable = &Publicist<K>::unsafeErase;
                                          (self.*callable)(key);
                                      } else {
                                          std::stringstream buffer;
                                          buffer << "Unknown key: " << key;
                                          throw py::key_error(buffer.str());
                                      }
                                  },
                                  "key"_a);
                          cls.def("popitem", [](Class& self) {
                              if (!self.empty()) {
                                  K key = self.keys().back();
                                  auto result = std::make_pair(key, get(self, key));
                                  auto callable = &Publicist<K>::unsafeErase;
                                  (self.*callable)(key);
                                  return result;
                              } else {
                                  throw py::key_error("Cannot pop from empty GenericMap.");
                              }
                          });
                          cls.def("clear", &Class::clear);
                          // cls.def("update",, "other"_a);
                      });
}
}  // namespace

void wrapGenericMap(utils::python::WrapperCollection& wrappers) {
    declareGenericMap<std::string>(wrappers, "S", "strings");
    declareMutableGenericMap<std::string>(wrappers, "S", "strings");
}

}  // namespace typehandling
}  // namespace afw
}  // namespace lsst
