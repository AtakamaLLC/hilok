#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "hilok.hpp"

namespace py = pybind11;

PYBIND11_MODULE(hilok, m)
{   
    m.doc() = "Hierarchical lock manager";
    py::class_<HiLok>(m, "HiLok")
        .def(py::init<>())
        .def(py::init<char>(), py::arg("/"))
        .def("write", &HiLok::write, py::arg("path"), py::arg("block") = true, py::arg("timeout") = 0)
        .def("read", &HiLok::read, py::arg("path"), py::arg("block") = true, py::arg("timeout") = 0)
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
