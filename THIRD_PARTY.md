# Third-party components

VYNE uses MinHook v1.3.4, pinned to commit `c3fcafdc10146beb5919319d0683e44e3c30d537`, as a Git submodule in `vendor/minhook`.

Source: https://github.com/TsudaKageyu/minhook/tree/c3fcafdc10146beb5919319d0683e44e3c30d537

The full MinHook and bundled disassembler notices are preserved in `vendor/minhook/LICENSE.txt`. The test installer copies that file beside VYNE's support files. MinHook is a general detouring dependency; no REPlus source, binary or configuration is included or required.
