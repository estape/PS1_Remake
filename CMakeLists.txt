# CMakeList.txt: projeto de CMake para PS1_Remake, incluir origem e definir
# lógica específica a um projeto aqui.
#
cmake_minimum_required (VERSION 3.10)

# Habilite a Recarga Dinâmica para compiladores MSVC, se houver suporte.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("PS1_Remake")

# Definição de caminhos base baseada nos seus arquivos
set(SDL3_PATH "D:/Users/Rodrigo/Documents/Dev/VisualStudio/Desktop/SDL3-3.4.0")
set(SDL3_MSVC_PATH "${SDL3_PATH}/build-scripts/pkg-support/msvc")

# Adicione a origem ao executável deste projeto.
add_executable (PS1_Remake "PS1_Remake.cpp" "PS1_Remake.h" "all/Source/src/CoreWrapper.cpp" "all/Source/include/CoreWrapper.h" "all/PS1_Remake/Model/src/SetAttr.cpp" "all/PS1_Remake/Model/include/SetAttr.h")

# Configurar Include e Bibliotecas Manualmente
target_include_directories(PS1_Remake PRIVATE "${SDL3_PATH}/include")
target_link_directories(PS1_Remake PRIVATE "${SDL3_MSVC_PATH}/x64")
target_link_libraries(PS1_Remake PRIVATE SDL3)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET PS1_Remake PROPERTY CXX_STANDARD 20)
endif()