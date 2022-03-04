#include "PyGenerator.h"

#include <pybind11/embed.h>

#define LOAD_PY_FROM_FILE 0

namespace Halide {
namespace PythonBindings {

namespace {

#if !LOAD_PY_FROM_FILE
// Everything here is implicitly in module 'halide'
const char builtin_helpers_src[] = R"(
from abc import ABC, abstractmethod
from collections import OrderedDict

# OutputBuffer if a Func with type-and-dim added
class OutputBuffer(Func):
    def __init__(self, type:Type, dimensions:int, name:str):
        Func.__init__(self, name)
        self._type = type;
        self._dimensions = dimensions;

    def type(self):
        return self._type

    def dimensions(self):
        return self._dimensions

class Generator(ABC):
    """Base class for Halide Generators in Python"""
    # def get_target(self):
    #     # TODO
    #     return None

    # def get_auto_schedule(self):
    #     # TODO
    #     return False

    # def get_machine_params(self):
    #     # TODO
    #     return None

    # def apply(self, *args, **kwargs):
    #     # TODO
    #     pass

    def __init__(self):
        self.__dict__ = OrderedDict()

    @abstractmethod
    def generate(self):
        pass

    def schedule(self):
        pass

    def _build_arguments(self):
        arguments = []
        for k, v in self.__class__.__bases__[0].__dict__.items():
            if isinstance(v, Param):
                arguments.append(Argument(k, ArgumentKind.InputScalar, v.type(), 0))
            elif isinstance(v, ImageParam):
                arguments.append(Argument(k, ArgumentKind.InputBuffer, v.type(), v.dimensions()))
            elif isinstance(v, OutputBuffer):
                arguments.append(Argument(k, ArgumentKind.OutputBuffer, v.type(), v.dimensions()))
        return arguments

    def _get_input_parameters(self):
        parameters = {}
        for k, v in self.__class__.__bases__[0].__dict__.items():
            if isinstance(v, Param) or isinstance(v, ImageParam):
                parameters[k] = v.parameter()
        return parameters

    def _get_output_funcs(self):
        funcs = {}
        for k, v in self.__class__.__bases__[0].__dict__.items():
            if isinstance(v, OutputBuffer):
                funcs[k] = v  # OutputBuffer is a subclass of Func
        return funcs

_python_generators:dict = {}

def _get_python_generator_names():
    return _python_generators.keys()

def _find_python_generator(name):
    if not name in _python_generators:
        return None
    return _python_generators[name]["class"]

def generator(name, other = []):
    def real_decorator(cls):
        assert not issubclass(cls, Generator), "Please use the @hl.generator decorator instead of inheriting from hl.Generator"
        new_cls = type(cls.__name__, (cls, Generator), {})
        _python_generators[name] = { "class": new_cls, "other": other };
        return new_cls

    return real_decorator
)";
#endif

using Halide::Internal::AbstractGenerator;
using Halide::Internal::AbstractGeneratorPtr;
using Halide::Internal::ExternsMap;
using Halide::Internal::GeneratorsForMain;
using Halide::Internal::IOKind;
using Halide::Internal::Parameter;

template<typename T>
std::map<std::string, T> dict_to_map(const py::dict &dict) {
    _halide_user_assert(!dict.is(py::none()));
    std::map<std::string, T> m;
    for (auto it : dict) {
        m[it.first.cast<std::string>()] = it.second.cast<T>();
    }
    return m;
}

class PyGeneratorBase : public AbstractGenerator {
    // Boilerplate
    const GeneratorContext context_;

    // The name declared in the Python function's decorator
    const std::string name_;

    // The Python class
    const py::object class_;

    // The instance of the Python class
    py::object generator_;

    // The Arguments declared in the Python function's decorator
    std::vector<Argument> arguments_;

    // GeneratorParams
    std::map<std::string, std::string> generator_params_;

    // State we build up
    std::map<std::string, Parameter> input_parameters_;
    std::map<std::string, Func> output_funcs_;
    // Our Pipeline
    Pipeline pipeline_;

    std::vector<AbstractGenerator::ArgInfo> get_arginfos(bool inputs) {
        std::vector<AbstractGenerator::ArgInfo> arginfo;
        for (const auto &arg : arguments_) {
            if (inputs != arg.is_input()) {
                continue;
            }
            arginfo.push_back({arg.name,
                               arg.is_scalar() ? IOKind::Scalar : IOKind::Buffer,
                               std::vector<Type>{arg.type},
                               arg.is_scalar() ? 0 : arg.dimensions});
        }
        return arginfo;
    }

public:
    // Note: this ctor should not throw any exceptions. Success will be checked
    // by calling is_valid() later.
    explicit PyGeneratorBase(const GeneratorContext &context, const std::string name)
        : context_(context),
          name_(name),
          class_(py::module_::import("halide").attr("_find_python_generator")(name)),  // could be None!
          generator_(class_.is(py::none()) ? py::none() : class_()) {                  // could be None!
        if (!generator_.is(py::none())) {
            auto arguments_obj = generator_.attr("_build_arguments")();
            if (!arguments_obj.is(py::none())) {
                arguments_ = args_to_vector<Argument>(arguments_obj);
            }
        }
    }

    bool is_valid() const {
        if (name_.empty() || class_.is(py::none()) || generator_.is(py::none()) || arguments_.empty()) {
            return false;
        }
        return true;
    }

    std::string get_name() override {
        return name_;
    }

    GeneratorContext context() const override {
        return context_;
    }

    std::vector<ArgInfo> get_input_arginfos() override {
        return get_arginfos(true);
    }

    std::vector<ArgInfo> get_output_arginfos() override {
        return get_arginfos(false);
    }

    std::vector<std::string> get_generatorparam_names() override {
        std::vector<std::string> v;
        for (const auto &c : generator_params_) {
            v.push_back(c.first);
        }
        return v;
    }

    void set_generatorparam_value(const std::string &name, const std::string &value) override {
        // TODO: convert _halide_user_assert() into something else
        _halide_user_assert(!pipeline_.defined());
        _halide_user_assert(generator_params_.count(name) == 1) << "Unknown GeneratorParam: " << name;
        generator_params_[name] = value;
    }

    void set_generatorparam_value(const std::string &name, const LoopLevel &value) override {
        _halide_user_assert(!pipeline_.defined());
        _halide_user_assert(generator_params_.count(name) == 1) << "Unknown GeneratorParam: " << name;
        _halide_user_assert(false) << "This Generator has no LoopLevel GeneratorParams.";
    }

    Pipeline build_pipeline() override {
        _halide_user_assert(!pipeline_.defined());
        _halide_user_assert(input_parameters_.empty() && output_funcs_.empty());

        // Validate the input argument specifications.
        size_t expected_inputs = 0, expected_outputs = 0;
        {
            for (const auto &arg : arguments_) {
                if (arg.is_input()) {
                    _halide_user_assert(expected_outputs == 0) << "Generator " << name_ << " must list all Inputs in Arguments before listing any Outputs.";
                    expected_inputs++;
                } else {
                    expected_outputs++;
                }
            }
            _halide_user_assert(expected_outputs > 0) << "Generator " << name_ << " must declare at least one Output in Arguments.";
        }

        generator_.attr("generate")();
        generator_.attr("schedule")();

        input_parameters_ = dict_to_map<Parameter>(generator_.attr("_get_input_parameters")());
        _halide_user_assert(input_parameters_.size() == expected_inputs);

        output_funcs_ = dict_to_map<Func>(generator_.attr("_get_output_funcs")());
        _halide_user_assert(output_funcs_.size() == expected_outputs);

        std::vector<Func> funcs;
        for (const auto &arg : arguments_) {
            if (!arg.is_output()) {
                continue;
            }
            Func f = output_funcs_[arg.name];
            _halide_user_assert(f.defined()) << "Output \"" << arg.name << "\" was not defined.\n";
            _halide_user_assert(f.dimensions() == arg.dimensions) << "Output \"" << arg.name
                                                                  << "\" requires dimensions=" << arg.dimensions
                                                                  << " but was defined as dimensions=" << f.dimensions() << ".\n";
            // TODO: allow for tuple-valued outputs here.
            std::vector<Type> arg_types = {arg.type};
            std::vector<Type> output_types = f.output_types();
            _halide_user_assert(output_types.size() == arg_types.size()) << "Output \"" << arg.name
                                                                         << "\" requires a Tuple of size " << arg_types.size()
                                                                         << " but was defined as Tuple of size " << output_types.size() << ".\n";
            for (size_t i = 0; i < f.output_types().size(); ++i) {
                Type expected = arg_types.at(i);
                Type actual = output_types.at(i);
                _halide_user_assert(expected == actual) << "Output \"" << arg.name
                                                        << "\" requires type " << expected
                                                        << " but was defined as type " << actual << ".\n";
            }
            funcs.push_back(f);
        }
        pipeline_ = Pipeline(funcs);

        // Validate the output argument specifications.
        {
            auto pipeline_outputs = pipeline_.outputs();
            _halide_user_assert(expected_outputs == pipeline_outputs.size()) << "Generator " << name_ << " does not return the correct number of Outputs.";
            size_t i = 0;
            for (const auto &arg : arguments_) {
                if (arg.is_input()) {
                    continue;
                }
                output_funcs_[arg.name] = pipeline_outputs.at(i++);
            }
        }

        return pipeline_;
    }

    std::vector<Parameter> get_parameters_for_input(const std::string &name) override {
        _halide_user_assert(pipeline_.defined());
        auto it = input_parameters_.find(name);
        _halide_user_assert(it != input_parameters_.end()) << "Unknown input: " << name;
        return {it->second};
    }

    std::vector<Func> get_funcs_for_output(const std::string &name) override {
        _halide_user_assert(pipeline_.defined());
        auto it = output_funcs_.find(name);
        _halide_user_assert(it != output_funcs_.end()) << "Unknown output: " << name;
        return {it->second};
    }

    ExternsMap get_external_code_map() override {
        // Python Generators don't support this (yet? ever?),
        // but don't throw an error, just return an empty map.
        return {};
    }

    void bind_input(const std::string &name, const std::vector<Parameter> &v) override {
        _halide_user_assert(false) << "Python Generators don't support bind_input()";
    }

    void bind_input(const std::string &name, const std::vector<Func> &v) override {
        _halide_user_assert(false) << "Python Generators don't support bind_input()";
    }

    void bind_input(const std::string &name, const std::vector<Expr> &v) override {
        _halide_user_assert(false) << "Python Generators don't support bind_input()";
    }

    bool emit_cpp_stub(const std::string & /*stub_file_path*/) override {
        // Python Generators don't support this (probably ever),
        // but don't throw an error, just return false.
        return false;
    }
};

class PyGeneratorsForMain : public GeneratorsForMain {
public:
    PyGeneratorsForMain() = default;

    std::vector<std::string> enumerate() const override {
        py::object f = py::module_::import("halide").attr("_get_python_generator_names");
        return args_to_vector<std::string>(f());
    }
    AbstractGeneratorPtr create(const std::string &name,
                                const Halide::GeneratorContext &context) const override {
        auto g = std::make_unique<PyGeneratorBase>(context, name);
        if (!g->is_valid()) {
            return nullptr;
        }
        return g;
    }
};

}  // namespace

void define_generator(py::module &m) {
    py::object scope = m.attr("__dict__");
#if LOAD_PY_FROM_FILE
    py::eval_file("/Users/srj/GitHub/Halide/builtin_helpers_src.py", scope);
#else
    py::exec(builtin_helpers_src, scope);
#endif

    auto generatorcontext_class =
        py::class_<GeneratorContext>(m, "GeneratorContext")
            .def(py::init<const Target &, bool, const MachineParams &>(),
                 py::arg("target"), py::arg("auto_schedule") = false, py::arg("machine_params") = MachineParams::generic())
            .def("get_target", &GeneratorContext::get_target)
            .def("get_auto_schedule", &GeneratorContext::get_auto_schedule)
            .def("get_machine_params", &GeneratorContext::get_machine_params)
            // TODO: handle get_externs_map() someday?
            .def("__repr__", [](const GeneratorContext &context) -> std::string {
                std::ostringstream o;
                o << "<halide.GeneratorContext " << context.get_target() << ">";
                return o.str();
            });

    m.def("main", []() -> void {
        py::object argv_object = py::module_::import("sys").attr("argv");
        std::vector<std::string> argv_vector = args_to_vector<std::string>(argv_object);
        std::vector<char *> argv;
        argv.reserve(argv_vector.size());
        for (auto &s : argv_vector) {
            argv.push_back(const_cast<char *>(s.c_str()));
        }
        std::ostringstream error_stream;
        int result = Halide::Internal::generate_filter_main((int)argv.size(), argv.data(), error_stream, PyGeneratorsForMain());
        if (!error_stream.str().empty()) {
            py::print(error_stream.str(), py::arg("end") = "");
        }
        if (result != 0) {
            // Some paths in generate_filter_main() will fail with user_error or similar (which throws an exception
            // due to how libHalide is built for python), but some paths just return an error code, so
            // be sure to handle both.
            throw std::runtime_error("Generator failed: " + std::to_string(result));
        }
    });
}

}  // namespace PythonBindings
}  // namespace Halide
