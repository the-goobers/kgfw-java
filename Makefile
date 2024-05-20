mac:
	clang -fPIC -shared $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o libkgfw.dylib -Ilib/include -Llib/mac -lglfw3 -framework OpenAL -framework Cocoa -framework IOKit -lm -DKGFW_OPENGL=33 -DKGFW_DEBUG -Wno-deprecated-declarations
	clang main.c -o program -Ilib/include -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/darwin -L$(JAVA_HOME)/lib/server -L$(JAVA_HOME)/lib -ljvm -ljava -L. -lkgfw

linux:
	clang -fPIC -shared $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o libkgfw.so -Ilib/include -lglfw -lGL -lopenal -lm -DKGFW_OPENGL=33 -DKGFW_DEBUG
	clang main.c -o program -Ilib/include -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -L$(JAVA_HOME)/lib/server -L$(JAVA_HOME)/lib -ljvm -ljava -L. -lkgfw

emscripten:
	emcc main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program.html -s USE_WEBGL2=1 -s USE_GLFW=3 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -Ilib/include -lglfw -lGL -lopenal -lm -DKGFW_OPENGL=33 --preload-file assets # -DKGFW_DEBUG -Wno-visibility -Wno-incompatible-pointer-types

run:
	pylauncher ./program $(PWD)
