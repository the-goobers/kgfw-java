mac:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Wno-deprecated-declarations -Ilib/include -Llib/mac -lglfw3 -framework Cocoa -framework IOKit -framework OpenGL -framework OpenAL -lm -DKGFW_DEBUG

linux:
	clang main.c $(shell find ./lib/src -type f -name "*.c") $(shell find ./kgfw -type f -name "*.c") -o program -Ilib/include -lglfw -lGL -lopenal -lm -DKGFW_DEBUG

run:
	pylauncher ./program $(PWD)