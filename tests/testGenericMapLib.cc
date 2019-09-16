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

#include <pybind11/pybind11.h>

#include <memory>
#include <sstream>

#include "lsst/pex/exceptions.h"

#include "lsst/afw/typehandling/GenericMap.h"
#include "lsst/afw/typehandling/SimpleGenericMap.h"
#include "lsst/afw/typehandling/Storable.h"

namespace py = pybind11;
using namespace pybind11::literals;
using namespace std::string_literals;

namespace lsst {
namespace afw {
namespace typehandling {

namespace {

/// A Storable with simple, mutable state.
class CppStorable : public Storable {
public:
    explicit CppStorable(std::string const& value) : Storable(), value(value) {}

    CppStorable(CppStorable const&) = default;
    CppStorable(CppStorable&&) = default;
    CppStorable& operator=(CppStorable const&) = default;
    CppStorable& operator=(CppStorable&&) = default;
    ~CppStorable() noexcept = default;

    /**
     * Two CppStorables are equal if and only if their internal states are equal.
     *
     * @{
     */
    bool operator==(CppStorable const& other) const noexcept { return value == other.value; }
    bool operator!=(CppStorable const& other) const noexcept { return !(*this == other); }

    /** @} */

    /// Assign a new value to this object
    void reset(std::string const& value) { this->value = value; }
    /// Retrieve the value in this object
    std::string const& get() const noexcept { return value; }

    // Storable methods
    std::shared_ptr<Storable> cloneStorable() const override { return std::make_shared<CppStorable>(*this); }
    std::string toString() const override { return value; }
    bool equals(Storable const& other) const noexcept override { return singleClassEquals(*this, other); }

private:
    std::string value;
};

/**
 * Test whether a map contains a key-value pair.
 *
 * @param map The map to test
 * @param key, value The key-value pair to test.
 *
 * @throws pex::exceptions::NotFoundError Thrown if the key is not present in
 *      the map, or maps to a different value.
 */
template <typename T>
void assertKeyValue(GenericMap<std::string> const& map, std::string const& key, T const& value) {
    using lsst::pex::exceptions::NotFoundError;

    if (!map.contains(key)) {
        throw LSST_EXCEPT(NotFoundError, "Map does not contain key " + key);
    }

    auto typedKey = makeKey<T>(key);
    if (!map.contains(typedKey)) {
        std::stringstream buffer;
        buffer << "Map maps " << key << " to a different type than " << typedKey;
        throw LSST_EXCEPT(NotFoundError, buffer.str());
    }

    T const& mapValue = map.at(typedKey);
    if (mapValue != value) {
        std::stringstream buffer;
        buffer << "Map maps " << typedKey << " to " << mapValue << ", expected " << value;
        throw LSST_EXCEPT(NotFoundError, buffer.str());
    }
}

/**
 * Test whether a CppStorable contains a specific value.
 *
 * @param storable the storable to test
 * @param value the expected internal string
 */
void assertCppValue(CppStorable const& storable, std::string const& value) {
    using lsst::pex::exceptions::RuntimeError;

    if (storable.get() != value) {
        std::stringstream buffer;
        buffer << "CppStorable contains " << storable << ", expected " << value;
        throw LSST_EXCEPT(RuntimeError, buffer.str());
    }
}

/**
 * Create a MutableGenericMap that can be passed to Python for testing.
 *
 * @returns a map containing the state `{"one": 1, "pi": 3.1415927,
 *          "string": "neither a number nor NaN"}`. This state is hardcoded
 *          into the Python test code, and should be changed with caution.
 */
std::shared_ptr<MutableGenericMap<std::string>> makeInitialMap() {
    auto map = std::make_unique<SimpleGenericMap<std::string>>();
    // TODO: workaround for DM-21268
    map->insert("one", std::int64_t(1));
    map->insert("pi", 3.1415927);
    // TODO: workaround for DM-21216
    map->insert("string", "neither a number nor NaN"s);
    return map;
}

/**
 * Change the values in a GenericMap.
 *
 * @param testmap the map to update. Assumed to be in the state created by
 *                makeInitialMap.
 *
 * This function performs changes equivalent to the following Python:
 *
 *     testmap['answer'] = 42
 *     testmap['pi'] = 3.0
 *     testmap['string'] = False
 *
 * This difference is hardcoded into the Python test code, and should be
 * changed with caution.
 */
void makeCppUpdates(MutableGenericMap<std::string>& testmap) {
    // TODO: workaround for DM-21268
    testmap.insert("answer", std::int64_t(42));

    testmap.at(makeKey<double>("pi"s)) = 3.0;

    testmap.erase(makeKey<std::string>("string"s));
    testmap.insert("string", false);
}

}  // namespace

namespace {

// Functions for working with values of arbitrary type
template <typename T>
void declareAnyTypeFunctions(py::module& mod) {
    mod.def("assertKeyValue",
            static_cast<void (*)(GenericMap<std::string> const&, std::string const&, T const&)>(
                    &assertKeyValue),
            "map"_a, "key"_a, "value"_a);
}

}  // namespace

PYBIND11_MODULE(testGenericMapLib, mod) {
    py::module::import("lsst.afw.typehandling");

    declareAnyTypeFunctions<bool>(mod);
    declareAnyTypeFunctions<std::int64_t>(mod);
    declareAnyTypeFunctions<double>(mod);
    declareAnyTypeFunctions<std::string>(mod);
    mod.def("assertCppValue", &assertCppValue, "storable"_a, "value"_a);

    mod.def("makeInitialMap", &makeInitialMap);
    mod.def("makeCppUpdates", &makeCppUpdates, "testmap"_a);

    py::class_<CppStorable, std::shared_ptr<CppStorable>, Storable> cls(mod, "CppStorable");
    cls.def(py::init<std::string>());
    cls.def("__eq__", &CppStorable::operator==, py::is_operator());
    cls.def("__ne__", &CppStorable::operator!=, py::is_operator());
    cls.def_property("value", &CppStorable::get, &CppStorable::reset);
    cls.def("__str__", &CppStorable::toString);
    cls.def("__repr__", &CppStorable::toString);
}

}  // namespace typehandling
}  // namespace afw
}  // namespace lsst
