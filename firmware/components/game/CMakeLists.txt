idf_component_register(
    SRCS
        "src/Characters.cpp"
        "src/VaiwuPet.cpp"
        "src/OfflinePhrases.cpp"
        "src/SoundSynth.cpp"
        "src/I18n.cpp"
        "src/Assistant.cpp"
        "src/Shop.cpp"
        "src/Rpg.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
