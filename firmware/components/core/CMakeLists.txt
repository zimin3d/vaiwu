idf_component_register(
    SRCS "src/ConsoleLogger.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES log
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
