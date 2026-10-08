# Viduce Engine

All commands below must be run **inside the development container**. Start it
from the repo root with `./dev.sh`, which builds and starts the `viduce-dev`
container with this repo mounted at `/usr/src/viduce`, then open a shell with
`./dev.sh exec dev bash`. Then
`cd /usr/src/viduce/engine` before running any of the commands in this
document.

## Running the demo
* Build all targets
```
cmake -B build
cmake --build build -j 18
```

* Run the demo
```
VIDUCE_UPSCALER_PATH=<path_to_model> ./build/src/demo <input_file_path> <output_dir>
```

## Running tests
* Build all targets as specified above
* Run the tests
```
cd build
ctests
```
