# Save compatibility regression

Runs the actual inventory/randomizer JSON converters against new ten-slot saves, legacy nine-slot saves,
and extra-slot data. The test does not start the game or change user save files.

From the project root after setting up the normal Windows build dependencies:

```powershell
cmake -S unbound-docs/tests -B build/x64/unbound-tests
cmake --build build/x64/unbound-tests --config Release
ctest --test-dir build/x64/unbound-tests -C Release --output-on-failure
```
