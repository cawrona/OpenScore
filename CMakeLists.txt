cmake_minimum_required(VERSION 3.21)
project(OpenScore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Concurrent)
qt_standard_project_setup()

# --- MuPDF (page rendering) -------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(MUPDF QUIET mupdf)
if(NOT MUPDF_FOUND)
    find_path(MUPDF_INCLUDE_DIRS mupdf/fitz.h)
    find_library(MUPDF_LIBRARIES NAMES mupdf)
    if(NOT MUPDF_INCLUDE_DIRS OR NOT MUPDF_LIBRARIES)
        message(FATAL_ERROR "MuPDF not found. Install libmupdf-dev (Debian/Ubuntu), mupdf-devel (Fedora) or mupdf (Arch).")
    endif()
endif()

# --- RtMidi (optional, MIDI pedals / controllers) ---------------------------
pkg_check_modules(RTMIDI QUIET rtmidi)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(openscore src/main.cpp)

qt_add_qml_module(openscore
    URI OpenScore
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/LibraryView.qml
        qml/EntryCard.qml
        qml/ReaderView.qml
        qml/ToolButtonX.qml
        qml/SettingsDialog.qml
    SOURCES
        src/Util.h
        src/PdfDocument.h        src/PdfDocument.cpp
        src/PageRenderService.h  src/PageRenderService.cpp
        src/ScoreView.h          src/ScoreView.cpp
        src/AnnotationStore.h    src/AnnotationStore.cpp
        src/InkLayer.h           src/InkLayer.cpp
        src/EntryModel.h         src/EntryModel.cpp
        src/Library.h            src/Library.cpp
        src/ActionMap.h          src/ActionMap.cpp
        src/MidiInput.h          src/MidiInput.cpp
)

target_include_directories(openscore PRIVATE src ${MUPDF_INCLUDE_DIRS})
target_link_libraries(openscore PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Concurrent
    ${MUPDF_LIBRARIES})
if(MUPDF_LIBRARY_DIRS)
    target_link_directories(openscore PRIVATE ${MUPDF_LIBRARY_DIRS})
endif()

if(RTMIDI_FOUND)
    target_compile_definitions(openscore PRIVATE OPENSCORE_WITH_MIDI)
    target_include_directories(openscore PRIVATE ${RTMIDI_INCLUDE_DIRS})
    target_link_libraries(openscore PRIVATE ${RTMIDI_LIBRARIES})
    message(STATUS "MIDI support: enabled (RtMidi)")
else()
    message(STATUS "MIDI support: disabled (install librtmidi-dev to enable)")
endif()

install(TARGETS openscore RUNTIME DESTINATION bin)
install(FILES data/openscore.desktop DESTINATION share/applications)
