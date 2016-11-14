CC		= gcc
CXX		= g++
LLVM_CONFIG	= llvm-config-3.4
# LLVM_CONFIG	= llvm-config
CFLAGS		= -std=c99 -O2 -Wall -g
CXXFLAGS	= -std=c++11 -O2 -Wall `$(LLVM_CONFIG) --cppflags` -g
LINK		= g++
LDFLAGS		= `$(LLVM_CONFIG) --ldflags --libs all` -pthread -ldl

HEADERS	= ast.h \
	  generator.h \
	  generatorstate.h \
	  parser.h \

OBJECTS	= driver.o \
	  ast.o \
	  parser.o \
	  codegen.o \

STDLIB	= toystd.o \

TARGET	= toycompiler

$(TARGET): $(OBJECTS) $(STDLIB)
	$(LINK) -o $(TARGET) $(OBJECTS) $(STDLIB) $(LDFLAGS)

.cpp.o:	$(HEADERS)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	@rm -f $(OBJECTS)

distclean: clean
	@rm -f $(TARGET) $(STDLIB)

