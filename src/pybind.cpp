#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "hilok.hpp"

namespace py = pybind11;

PYBIND11_MODULE(hilok, m)
{   
    m.doc() = "Hierarchical lock manager";

    py::enum_<HiFlags>(m, "HiLokFlags", py::arithmetic())
        .value("STRICT", HiFlags::STRICT)
        .value("LOOSE_READ_UNLOCK", HiFlags::LOOSE_READ_UNLOCK)
        .value("LOOSE_WRITE_UNLOCK", HiFlags::LOOSE_WRITE_UNLOCK)
        .value("RECURSIVE_WRITE", HiFlags::RECURSIVE_WRITE)
        .value("RECURSIVE_ONEWAY", HiFlags::RECURSIVE_ONEWAY)
        .value("RECURSIVE", HiFlags::RECURSIVE)
        .value("LOOSE_UNLOCK", static_cast<HiFlags>(HiFlags::LOOSE_READ_UNLOCK + HiFlags::LOOSE_WRITE_UNLOCK));

    py::class_<HiLok, std::shared_ptr<HiLok>>(m, "HiLok")
        .def(py::init<>())
        .def(py::init<char>(), py::arg("sep") = '/')
        .def(py::init<char, int>(), py::arg("sep") = '/', py::arg("flags") = HiFlags::RECURSIVE)
        .def("write", [](std::shared_ptr<HiLok> lok, std::string_view path, std::optional<bool> block, std::optional<double> timeout) {
                py::gil_scoped_release _gil_rel;
                if (!block.has_value())
                    block = true;
                if (!timeout.has_value())
                    timeout = 0.0;
                return lok->write(lok, path, block.value(), timeout.value());
            }, py::arg("path"), py::arg("block") = true, py::arg("timeout") = 0.0)
        .def("read", [](std::shared_ptr<HiLok> lok, std::string_view path, std::optional<bool> block, std::optional<double> timeout) {
                py::gil_scoped_release _gil_rel;
                if (!block.has_value())
                    block = true;
                if (!timeout.has_value())
                    timeout = 0.0;
                return lok->read(lok, path, block.value(), timeout.value());
            }, py::arg("path"), py::arg("block") = true, py::arg("timeout") = 0.0)
        .def("rename", [](std::shared_ptr<HiLok> lok, std::string_view from, std::string_view to, std::optional<bool> block, std::optional<double> timeout) {
                py::gil_scoped_release _gil_rel;
                if (!block.has_value())
                    block = true;
                if (!timeout.has_value())
                    timeout = 0.0;
                return lok->rename(from, to, block.value(), timeout.value());
            }, py::arg("from"), py::arg("to"), py::arg("block") = true, py::arg("timeout") = 0.0)
        ;

    py::class_<HiHandle, std::shared_ptr<HiHandle>>(m, "HiHandle")
        .def("release", &HiHandle::release)
        .def("__enter__", [](std::shared_ptr<HiHandle> hh) {return hh;})
        .def("__exit__", [](std::shared_ptr<HiHandle> hh, const py::object &, const py::object &, const py::object &) { hh->release(); })
        ;

    py::register_exception<HiErr>(m, "HiLokError", PyExc_TimeoutError);

    #ifdef VERSION_INFO
        m.attr("__version__") = VERSION_INFO;
    #else
        m.attr("__version__") = "dev";
    #endif
}
