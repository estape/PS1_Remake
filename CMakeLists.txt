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
    "source/PS1_Remake/Engine/src/SaveStateManager"
    "source/PS1_Remake/Engine/include/SaveStateManager"
    "source/PS1_Remake/Engine/src/CD_Management"
    "source/PS1_Remake/Engine/include/CD_Management"

    #SDL Base
    #"source/PS1_Remake/View/src/BtnClass.cpp" 
    #"source/PS1_Remake/View/include/BtnClass.h"
)

# --- 3. AMBIENTE DE DESENVOLVIMENTO (WINDOWS) ---
set(SDL3_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDL3-3.4.2")
set(SDL3_IMAGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_image-3.4.0")

target_include_directories(${PROJECT_NAME} PRIVATE 
    "${SDL3_PATH}/include"
    "${SDL3_IMAGE_PATH}/include"
)
target_link_directories(${PROJECT_NAME} PRIVATE 
    "${SDL3_PATH}/lib/x64"
    "${SDL3_IMAGE_PATH}/lib/x64"
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3 SDL3_image)

# --- 4. REGRAS DE CÓPIA PÓS-COMPILAÇÃO ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/system"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/system"
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Mednafen/src/mednafen_psx_hw_libretro.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mednafen_psx_hw_libretro.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_PATH}/lib/x64/SDL3.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL3.dll"
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_IMAGE_PATH}/lib/x64/SDL3_image.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL3_image.dll"
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL3_IMAGE_PATH}/lib/x64/optional/libpng16-16.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libpng16-16.dll"
)