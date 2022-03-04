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
