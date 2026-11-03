# Share FastLED's source inventory and SDK requirements, but deliberately do
# not register the repository root as a component: its idf_component.yml is
# for PlatformIO/pioarduino and uses newer component-manager rule syntax.
include("${CMAKE_CURRENT_LIST_DIR}/../../../../CMakeLists.txt")
