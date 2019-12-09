#include "cs_compiler.h"
#include "cc_error.h"
#include "cs_prepro.h"
#include "cc_options.h"
#include "myfilestream.h"

#include <stdio.h>
#include <string.h>
#include <iostream>
#include "getopt.h"

static int usage(char *argv0) {
	fprintf(stderr, "usage: %s [OPTIONS] FILE.asc\n"
	"compiles FILE.asc to FILE.o\n"
	"OPTIONS:\n"
	"-i systemheaderdir : provide path to system headers\n"
	"   this is the path containing implicitly included headers (atm only agsdefns.sh).\n"
	"-E : run preprocessor only (create file with .i extension)\n"
	"-W : warn about non-fatal errors parsing system headers\n"
	"-g : turn on debug info\n"
	"-o : explicitily specify output file name\n"
	, argv0);
	return 1;
}

char *slurp_file(FILE *f) {
	fseek(f, 0, SEEK_END);
	unsigned len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *mem = (char*) malloc(len+1);
	fread(mem, 1, len, f);
	mem[len] = 0;
	fclose(f);
	return mem;
}

int main(int argc, char** argv) {
	int c;
	int only_preprocess = 0;
	char oname_buf[2048], *oname = oname_buf;
	static char *systemhdr_dir = "systemhdrs";
	while((c = getopt(argc, argv, "gWEi:o:")) != EOF) switch(c) {
		case 'i': systemhdr_dir = optarg; break;
		case 'o': oname = optarg; break;
		case 'E': only_preprocess = 1; break;
		case 'W': ccPrintAllErrors = 1; break;
		case 'g': ccDefineMacro("DEBUG", "1"); break;
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char *filename = argv[optind];

	// Editor/AGS.Editor/AGSEditor.cs
	ccDefineMacro("AGS_NEW_STRINGS", "1");
	ccDefineMacro("AGS_SUPPORTS_IFVER", "1");

	// FIXME: these should all be toggled by options
	ccDefineMacro("STRICT", "1");
	ccDefineMacro("LRPRECEDENCE", "1");
	//ccDefineMacro("STRICT_STRINGS", "1");
	ccDefineMacro("STRICT_AUDIO", "1");
	ccDefineMacro("NEW_DIALOGOPTS_API", "1");

	static const char* apiv[] = {"321", "330", "334", "335", "340", "341", "350", 0};
	unsigned i;
	for(i=0;apiv[i];++i) {
		char buf[64];
		sprintf(buf, "SCRIPT_API_v%s", apiv[i]);
		ccDefineMacro(buf, "1");
		sprintf(buf, "SCRIPT_COMPAT_v%s", apiv[i]);
		ccDefineMacro(buf, "1");
	}

        ccSetSoftwareVersion("3.5.0.12");

        ccSetOption(SCOPT_EXPORTALL, 1);
        ccSetOption(SCOPT_LINENUMBERS, 1);
        // Don't allow them to override imports in the room script
	// FIXME better check for whether it's a roomfile
        ccSetOption(SCOPT_NOIMPORTOVERRIDE, !!strstr(filename, "room") );

        ccSetOption(SCOPT_LEFTTORIGHT, 1 /* FIXME: optional */);
        ccSetOption(SCOPT_OLDSTRINGS, 1 /* FIXME: optional */);

	char filename_wo_ext[2048];
	char *p = filename, *e = strrchr(filename, '.');
	char *q = filename_wo_ext;
	if(!e) return usage(argv[0]);
	while(p < e) *(q++) = *(p++);
	*q = 0;
	FILE *f;
	sprintf(oname_buf, "%s/agsdefns.sh", systemhdr_dir);
	f = fopen(oname_buf, "r");
	if(!f) fprintf(stderr, "warning: default header agsdefns.sh not found, you may want to set -i to the dir containing it!\n");
	else {
		char *hdr = slurp_file(f);
		ccAddDefaultHeader(hdr, "_BuiltInScriptHeader.ash");
	}
	if(oname == oname_buf) {
		if(!only_preprocess)
			sprintf(oname, "%s.o", filename_wo_ext);
		else
			sprintf(oname, "%s.i", filename_wo_ext);
	}

	f = fopen(filename, "r");
	if(!f) {
		fprintf(stderr, "error opening %s\n", filename);
		return 1;
	}
	char *mem = slurp_file(f);
	q = strrchr(filename_wo_ext, '/');
	/* for windoze folks... */
	if(!q) q = strrchr(filename_wo_ext, '\\');
	if(q) q++;
	else q = filename_wo_ext;
	if(only_preprocess) {
		// FIXME, buf may be to small for elab. macro expansions
		char *outp = (char*)malloc(strlen(mem)*2+5000);
		cc_preprocess(mem, outp);
		f = fopen(oname, "w");
		if(!f) goto oname_err;
		fwrite(outp, 1, strlen(outp), f);
		fclose(f);
		return ccError != 0;
	}
	ccScript* obj = ccCompileText(mem, q);
	if(!obj || (ccError != 0)) {
		std::cout << ccErrorString;
		std::cout << std::endl;
		return 1;
		//std::cout << ccErrorLine;
	} else {
		FileWriteStream fout;
		if(!fout.Open(oname)) {
	oname_err:
			fprintf(stderr, "error opening %s\n", oname);
			return 1;
		}
		obj->Write(&fout);
		fout.Close();
	}
	return 0;
}
