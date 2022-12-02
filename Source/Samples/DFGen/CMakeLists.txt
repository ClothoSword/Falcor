add_falcor_executable(DFGen)

target_sources(DFGen PRIVATE
    DFGen.cpp
    DFGen.h
)

target_copy_shaders(DFGen Samples/DFGen)

target_source_group(DFGen "Samples")
