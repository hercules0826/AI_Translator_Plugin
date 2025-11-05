cmake_minimum_required(VERSION 3.20)

# ---- JUCE boilerplate is already here (Projucer generated) ----
# set(VST3_SDK_PATH "C:/SDKs/vst3sdk")   # set in Projucer exporter or here

# Whisper.cpp
add_subdirectory(Source/external/whisper.cpp)     # target "whisper"

# Your plug-in target is usually ${PROJECT_NAME}
# Make sure these sources are included (or add them via Projucer CMake exporter)
target_sources(${PROJECT_NAME} PRIVATE
    Source/dsp/LockFreeRingBuffer.h
    Source/engine/WhisperEngine.h
    Source/engine/WhisperEngine.cpp
    Source/engine/Pipeline.h
    Source/engine/Pipeline.cpp
    Source/ui/Languages.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

# Windows: prefer static runtime for Waves env stability
if (MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus /permissive-)
endif()

# Link whisper
target_link_libraries(${PROJECT_NAME}
    PRIVATE
      whisper                       # from external/whisper.cpp
      juce::juce_dsp
      juce::juce_gui_extra
)

# Where to find models at runtime (simple relative path)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
      WHISPER_MODEL_PATH="$<IF:$<BOOL:${CMAKE_SOURCE_DIR}>,${CMAKE_SOURCE_DIR}/models/ggml-base.en.bin,./Source/external/whisper.cpp/models/ggml-base.en.bin>"
)
