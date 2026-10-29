# The PrimedGun Echoes launcher's Qt-free half: settings.cfg editing, the key
# table, the disc check, the PrimedGun settings import and the cannon texture pack.
add_library(primedgun_launcher_core STATIC
    cannon_textures.cpp
    dds_preview.cpp
    disc_probe.cpp
    launcher_keys.cpp
    port_settings_file.cpp
    primedgun_import.cpp
    settings_model.cpp)
target_include_directories(primedgun_launcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/include)
