// implementation tu for a couple things I found kinda bloaty
// to be inlines

#include <codeutils.h>

namespace elfldr::util {
		
	void ReplaceString(void* addr, const char* string) {
		DebugOut("Replacing string \"%s\" at %p: \"%s\"...", UBCast<char*>(addr), addr, string);
		__builtin_memcpy(addr, string, strlen(string) + 1);
	}

	void WriteString(void* addr, const char* string) {
		DebugOut("Writing string at %p: \"%s\"...", addr, string);
		__builtin_memcpy(addr, string, strlen(string) + 1);
	}
	
}