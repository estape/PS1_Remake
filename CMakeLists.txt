cmake_minimum_required(VERSION 3.16)
project("PS1_Remake")

# --- 1. CONFIGURAÇÕES DO C++ ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 2. SEUS ARQUIVOS DO EMULADOR ---
add_executable(${PROJECT_NAME} 
    #Main
    "PS1_Remake.cpp" 
    "PS1_Remake.h"

    #Engine 
    "source/PS1_Remake/Engine/src/Core.cpp" 
    "source/PS1_Remake/Engine/include/Core.h" 
    "source/PS1_Remake/Engine/src/SetAttr.cpp" 
    "source/PS1_Remake/Engine/include/SetAttr.h"
    "source/PS1_Remake/Engine/src/ControllerHandle.cpp" 
    "source/PS1_Remake/Engine/include/ControllerHandle.h"
    "source/PS1_Remake/Engine/src/MMC_Handle.cpp" 
    "source/PS1_Remake/Engine/include/MMC_Handle.h"
    "source/PS1_Remake/Engine/src/SaveStateManager.cpp"
    "source/PS1_Remake/Engine/include/SaveStateManager.h"
    "source/PS1_Remake/Engine/src/CD_Management.cpp"
    "source/PS1_Remake/Engine/include/CD_Management.h"
    "source/PS1_Remake/Engine/include/json.hpp"

    #System
    "source/PS1_Remake/Misc/include/ClassErrorHandler.h"
    "source/PS1_Remake/Misc/src/ClassErrorHandler.cpp"

    #User Interface
    "source/PS1_Remake/View/src/BtnClass.cpp" 
    "source/PS1_Remake/View/include/BtnClass.h"
)

# --- 3. AMBIENTE DE DESENVOLVIMENTO (LINUX NATIVO) ---
# O CMake acha automaticamente as libs que instalamos com 'sudo make install'
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(Vulkan REQUIRED) # <--- O novo motor!

# Linka de forma moderna e segura
target_link_libraries(${PROJECT_NAME} PRIVATE 
    SDL3::SDL3 
    SDL3_image::SDL3_image 
    SDL3_ttf::SDL3_ttf
    Vulkan::Vulkan
)

# --- 4. COPIAR RECURSOS (ASSETS) ---
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# --- 5. REGRAS DE CÓPIA PÓS-COMPILAÇÃO ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # Copia a pasta system necessária para o emulador
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/system"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/system"
    
    # Copia o núcleo do Mednafen (HW) que compilamos/baixamos para Linux
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Mednafen/src/mednafen_psx_hw_libretro.so"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mednafen_psx_hw_libretro.so"
)