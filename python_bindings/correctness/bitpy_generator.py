import halide as hl

x = hl.Var('x')
y = hl.Var('y')

@hl.generator("bitpy")
class BitGenerator:
    bit_input = hl.ImageParam(hl.Bool(), 1, "input_uint1")
    bit_constant = hl.Param(hl.Bool(), "constant_uint1")
    bit_output = hl.OutputBuffer(hl.Bool(), 1, "output_uint1")

    def generate(self):
        self.bit_output[x] = self.bit_input[x] + self.bit_constant;

if __name__ == "__main__":
    hl.main()

