#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "hilok.hpp"

namespace py = pybind11;

PYBIND11_MODULE(hilok, m)
{   
    m.doc() = "Hierarchical lock manager";
    py::class_<HiLok>(m, "HiLok")
        .def(py::init<>())
        .def(py::init<char>(), py::arg("sep") = '/')
        .def(py::init<char, bool>(), py::arg("sep") = '/', py::arg("recursive") = true)
        .def("write", [](HiLok &lok, std::string_view path, bool block = true, double timeout = 0) {
                py::gil_scoped_release();
                return lok.write(path, block, timeout);
            }, py::arg("path"), py::arg("block") = true, py::arg("timeout") = 0.0)
        .def("read", [](HiLok &lok, std::string_view path, bool block = true, double timeout = 0) {
                py::gil_scoped_release();
                return lok.read(path, block, timeout);
            }, py::arg("path"), py::arg("block") = true, py::arg("timeout") = 0.0)
        ;

    py::class_<HiHandle>(m, "HiHandle")
        .def("release", &HiHandle::release)
        .def("__enter__", [](HiHandle &hh) {})
        .def("__exit__", [](HiHandle &hh, const py::object &, const py::object &, const py::object &) { hh.release(); })
        ;

    py::register_exception<HiErr>(m, "HiLokError", PyExc_TimeoutError);

    #ifdef VERSION_INFO
        m.attr("__version__") = VERSION_INFO;
    #else
        m.attr("__version__") = "dev";
    #endif
}
