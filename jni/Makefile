linux:
	clang -fPIC -shared kgfw_jni.c -o ../libkgfwjni.so -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -L$(JAVA_HOME)/lib -L$(JAVA_HOME)/lib/server -I../lib/include -L.. -lkgfw -ljava -ljvm
