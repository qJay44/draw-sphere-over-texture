cmake -S . -B Build\\cc -G "MinGW Makefiles" -D CMAKE_EXPORT_COMPILE_COMMANDS=ON -D CMAKE_BUILD_TYPE=Release
move /y Build\\cc\\compile_commands.json Build\\compile_commands.json
