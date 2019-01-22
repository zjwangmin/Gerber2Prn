#!/bin/bash
flex -o src/gerber_flex.cc src/gerber_flex.ll
bison -d -o src/gerber_bison.cc src/gerber_bison.yy
sed -i 's/#include <unistd.h>/#ifdef __linux__\r\n #include <unistd.h>\r\n #endif\r\n/g' src/gerber_flex.cc
sed -i 's/#include <stdlib.h>/#include <stdlib.h>\r\n #ifndef __linux__\r\n #include <io.h>\r\n #include <process.h>\r\n #endif\r\n/g' src/gerber_flex.cc
sed -i 's/b->yy_is_interactive = file ? (isatty( fileno(file) ) > 0) : 0;/ \
          #ifdef __linux__\r\n \
          b->yy_is_interactive = file ? (isatty( fileno(file) ) > 0) : 0;\r\n \
          #else \r\n \
          b->yy_is_interactive = file ? (_isatty( _fileno(file) ) > 0) : 0;\r\n \
          #endif\r\n/g' src/gerber_flex.cc
