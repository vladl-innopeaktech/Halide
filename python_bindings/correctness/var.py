import halide as hl

def test_var():
    v1 = hl.Var()
    v2 = hl.Var()
    assert len(v1.name()) > 0
    assert len(v2.name()) > 0
    assert not v1.same_as(v2)

    v1 = hl.Var.implicit(1)
    assert v1.name() == "_1"
    v2 = hl.Var("_1")
    assert v1.same_as(v2)
    v3 = hl._1
    assert v1.same_as(v3)
    v4 = hl.Var("v4")
    assert not v1.same_as(v4)

    assert v1.is_implicit()
    assert v2.is_implicit()
    assert v3.is_implicit()
    assert not v4.is_implicit()

    assert hl.Var("_1").is_implicit()
    assert not hl.Var("v4").is_implicit()

    assert v1.implicit_index() == 1
    assert v2.implicit_index() == 1
    assert v3.implicit_index() == 1
    assert v4.implicit_index() == -1

    assert hl.Var("_1").implicit_index() == 1
    assert hl.Var("v4").implicit_index() == -1

    ph = hl._
    assert ph.name() == "_"
    assert ph.is_placeholder()
    assert hl.Var.is_placeholder(ph)
    assert not v1.is_placeholder()

    outermost = hl.Var.outermost()
    assert outermost.name() == "__outermost"

    # repr() and str()
    x = hl.Var('x')
    assert str(x) == "x"
    assert repr(x) == "<halide.Var 'x'>"

    # This verifies that halide.Var is implicitly convertible to halide.Expr
    r = hl.random_int(x)

    # This verifies that halide.Var is explicitly convertible to halide.Expr
    r = hl.random_int(hl.Expr(x))

if __name__ == "__main__":
    test_var()


# import halide as hl

# x = hl.Var('x')
# y = hl.Var('y')

# # # Just as with Func.compile_to_something(), you must explicitly specify
# # # the Arguments here for now. TODO: add type hinting everywhere as an alternative.
# # _FooGenArgs = [
# #     hl.Argument("input_buf", hl.ArgumentKind.InputBuffer, hl.UInt(8), 2),
# #     hl.Argument("input_scalar", hl.ArgumentKind.InputScalar, hl.Int(32), 0),
# #     hl.Argument("output_buf", hl.ArgumentKind.OutputBuffer, hl.UInt(8), 2),
# # ]

# # # "foo_bar" is the build-system name of the Generator
# # @hl.generator("foo_bar", _FooGenArgs)
# # def FooGen(context, input_buf, input_scalar):
# #     output_buf = hl.Func("output_buf")
# #     output_buf[x, y] = input_buf[x, y] + input_scalar
# #     return output_buf

# @hl.generator("foo_bar")
# class FooGen:
#     zinput = hl.ImageParam("zinput", hl.UInt(8), 3)
#     aoffset = hl.Param("aoffset", hl.Int(32))
#     youtput = hl.OutputBuffer("youtput", hl.Float(32), 2)
#     output = hl.OutputBuffer("output", hl.Float(64), 0)

#     def configure(self):
#         print("      FooGen configure")

#     def generate(self):
#         print("      FooGen generate")
#         print("  self.youtput is ", self.youtput)
#         self.youtput[x, y] = hl.f32(self.zinput[x, y, 0])
#         foo = hl.Func("foo");
#         foo[()] = 0
#         self.output[()] = hl.f64(self.aoffset) * 2.5
#         print("  self.youtput is now ", self.youtput)

#     def schedule(self):
#         print("      FooGen schedule")


# if __name__ == "__main__":
#     hl.main()
