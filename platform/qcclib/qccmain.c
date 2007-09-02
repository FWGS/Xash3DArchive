
#include "qcclib.h"
int mkdir(const char *path);

byte *qccpool;
char v_copyright[1024];

extern def_t *functemps;		//floats/strings/funcs/ents...

extern int optres_test1;
extern int optres_test2;

int writeasm;
int level;
	
cachedsourcefile_t *sourcefile;
bool PR_SimpleGetToken (void);
void PR_LexWhitespace (void);

void *FS_ReadToMem(char *fname, void *membuf, int *len);
void FS_CloseFromMem(void *mem);

struct qcc_includechunk_s *currentchunk;

unsigned int MAX_REGS;

int	MAX_STRINGS;
int	MAX_GLOBALS;
int	MAX_FIELDS;
int	MAX_STATEMENTS;
int	MAX_FUNCTIONS;
int MAX_CONSTANTS;
int max_temps;

int *qcc_tempofs;
int tempsstart;
int numtemps;

void PR_ResetErrorScope(void);

bool	compressoutput;

bool newstylesource;
char		destfile[1024];

float		*pr_globals;
unsigned int	numpr_globals;

char		*strings;
int			strofs;

dstatement_t	*statements;
int			numstatements;
int			*statement_linenums;

dfunction_t	*functions;
int			numfunctions;

ddef_t		*qcc_globals;
int			numglobaldefs;

ddef_t		*fields;
int			numfielddefs;

PATHSTRING		*precache_sounds;
int			numsounds;

PATHSTRING		*precache_textures;
int			numtextures;

PATHSTRING		*precache_models;
int			nummodels;

PATHSTRING		*precache_files;
int			numfiles;

extern int numCompilerConstants;
hashtable_t compconstantstable;
hashtable_t globalstable;
hashtable_t localstable;
hashtable_t floatconstdefstable;
hashtable_t stringconstdefstable;

bool pr_warning[WARN_MAX];

targetformat_t targetformat;

bool bodylessfuncs;

type_t *qcc_typeinfo;
int numtypeinfos;
int maxtypeinfos;


struct {
	char *name;
	int index;
} warningnames[] =
{
	{"Q302", WARN_NOTREFERENCED},
//	{"", WARN_NOTREFERENCEDCONST},
//	{"", WARN_CONFLICTINGRETURNS},
	{"Q105", WARN_TOOFEWPARAMS},
	{"Q101", WARN_TOOMANYPARAMS},
//	{"", WARN_UNEXPECTEDPUNCT},
	{"Q106", WARN_ASSIGNMENTTOCONSTANT},
	{"Q203", WARN_MISSINGRETURNVALUE},
	{"Q204", WARN_WRONGRETURNTYPE},
	{"Q205", WARN_POINTLESSSTATEMENT},
	{"Q206", WARN_MISSINGRETURN},
	{"Q207", WARN_DUPLICATEDEFINITION},
	{"Q100", WARN_PRECOMPILERMESSAGE},
//	{"", WARN_STRINGTOOLONG},
//	{"", WARN_BADTARGET},
	{"Q120", WARN_BADPRAGMA},
//	{"", WARN_HANGINGSLASHR},
//	{"", WARN_NOTDEFINED},
//	{"", WARN_SWITCHTYPEMISMATCH},
//	{"", WARN_CONFLICTINGUNIONMEMBER},
//	{"", WARN_KEYWORDDISABLED},
//	{"", WARN_ENUMFLAGS_NOTINTEGER},
//	{"", WARN_ENUMFLAGS_NOTBINARY},
//	{"", WARN_CASEINSENSATIVEFRAMEMACRO},
	{"Q111", WARN_DUPLICATELABEL},
	{"Q201", WARN_ASSIGNMENTINCONDITIONAL},
	{"F300", WARN_DEADCODE},
	{NULL}
};

int PR_WarningForName(char *name)
{
	int i;
	for (i = 0; warningnames[i].name; i++)
	{
		if (!stricmp(name, warningnames[i].name))
			return warningnames[i].index;
	}
	return -1;
}

optimisations_t optimisations[] =
{
	//level 0 = no optimisations
	//level 1 = size optimisations
	//level 2 = speed optimisations
	//level 3 = dodgy optimisations.
	//level 4 = experimental...

	{&opt_assignments,				"t",	1,	FLAG_ASDEFAULT,			"assignments",		"c = a*b is performed in one operation rather than two, and can cause older decompilers to fail."},
	{&opt_shortenifnots,			"i",	1,	FLAG_ASDEFAULT,			"shortenifs",		"if (!a) was traditionally compiled in two statements. This optimisation does it in one, but can cause some decompilers to get confused."},
	{&opt_nonvec_parms,				"p",	1,	FLAG_ASDEFAULT,			"nonvec_parms",		"In the original qcc, function parameters were specified as a vector store even for floats. This fixes that."},
	{&opt_constant_names,			"c",	2,	FLAG_KILLSDEBUGGERS,	"constant_names",	"This optimisation strips out the names of constants (but not strings) from your progs, resulting in smaller files. It makes decompilers leave out names or fabricate numerical ones."},
	{&opt_constant_names_strings,	"cs",	3,	FLAG_KILLSDEBUGGERS,	"constant_names_strings", "This optimisation strips out the names of string constants from your progs. However, this can break addons, so don't use it in those cases."},
	{&opt_dupconstdefs,				"d",	1,	FLAG_ASDEFAULT,			"dupconstdefs",		"This will merge definitions of constants which are the same value. Pay extra attention to assignment to constant warnings."},
	{&opt_noduplicatestrings,		"s",	1,	0,						"noduplicatestrings", "This will compact the string table that is stored in the progs. It will be considerably smaller with this."},
	{&opt_locals,					"l",	1,	FLAG_KILLSDEBUGGERS,	"locals",			"Strips out local names and definitions. This makes it REALLY hard to decompile"},
	{&opt_function_names,			"n",	1,	FLAG_KILLSDEBUGGERS,	"function_names",	"This strips out the names of functions which are never called. Doesn't make much of an impact though."},
	{&opt_filenames,				"f",	1,	FLAG_KILLSDEBUGGERS,	"filenames",		"This strips out the filenames of the progs. This can confuse the really old decompilers, but is nothing to the more recent ones."},
	{&opt_unreferenced,				"u",	1,	FLAG_ASDEFAULT,			"unreferenced",		"Removes the entries of unreferenced variables. Doesn't make a difference in well maintained code."},
	{&opt_overlaptemps,				"r",	1,	FLAG_ASDEFAULT,			"overlaptemps",		"Optimises the pr_globals count by overlapping temporaries. In QC, every multiplication, division or operation in general produces a temporary variable. This optimisation prevents excess, and in the case of Hexen2's gamecode, reduces the count by 50k. This is the most important optimisation, ever."},
	{&opt_constantarithmatic,		"a",	1,	FLAG_ASDEFAULT,			"constantarithmatic", "5*6 actually emits an operation into the progs. This prevents that happening, effectivly making the compiler see 30"},
	{&opt_precache_file,			"pf",	2,	0,						"precache_file",	"Strip out stuff wasted used in function calls and strings to the precache_file builtin (which is actually a stub in quake)."},
	{&opt_return_only,				"ro",	3,	FLAG_KILLSDEBUGGERS,					"return_only",		"Functions ending in a return statement do not need a done statement at the end of the function. This can confuse some decompilers, making functions appear larger than they were."},
	{&opt_compound_jumps,			"cj",	3,	FLAG_KILLSDEBUGGERS,					"compound_jumps",	"This optimisation plays an effect mostly with nested if/else statements, instead of jumping to an unconditional jump statement, it'll jump to the final destination instead. This will bewilder decompilers."},
//	{&opt_comexprremoval,			"cer",	4,	0,						"expression_removal",	"Eliminate common sub-expressions"},	//this would be too hard...
	{&opt_stripfunctions,			"sf",	3,	0,						"strip_functions",	"Strips out the 'defs' of functions that were only ever called directly. This does not affect saved games."},
	{&opt_locals_marshalling,		"lm",	4,	FLAG_KILLSDEBUGGERS,		"locals_marshalling", "Store all locals in one section of the pr_globals. Vastly reducing it. This effectivly does the job of overlaptemps. It's been noticed as buggy by a few, however, and the curcumstances where it causes problems are not yet known."},
	{&opt_vectorcalls,				"vc",	4,	FLAG_KILLSDEBUGGERS,					"vectorcalls",		"Where a function is called with just a vector, this causes the function call to store three floats instead of one vector. This can save a good number of pr_globals where those vectors contain many duplicate coordinates but do not match entirly."},
	{NULL}
};

#define defaultkeyword FLAG_HIDDENINGUI|FLAG_ASDEFAULT|FLAG_MIDCOMPILE
#define nondefaultkeyword FLAG_HIDDENINGUI|0|FLAG_MIDCOMPILE
//global to store useage to, flags, codename, human-readable name, help text
compiler_flag_t compiler_flag[] = {
	//keywords
	{&keyword_asm,			defaultkeyword, "asm",			"Keyword: asm",			"Disables the 'asm' keyword. Use the writeasm flag to see an example of the asm."},
	{&keyword_break,		defaultkeyword, "break",		"Keyword: break",		"Disables the 'break' keyword."},
	{&keyword_case,			defaultkeyword, "case",			"Keyword: case",		"Disables the 'case' keyword."},
	{&keyword_class,		defaultkeyword, "class",		"Keyword: class",		"Disables the 'class' keyword."},
	{&keyword_const,		defaultkeyword, "const",		"Keyword: const",		"Disables the 'const' keyword."},
	{&keyword_continue,		defaultkeyword, "continue",		"Keyword: continue",	"Disables the 'continue' keyword."},
	{&keyword_default,		defaultkeyword, "default",		"Keyword: default",		"Disables the 'default' keyword."},
	{&keyword_entity,		defaultkeyword, "entity",		"Keyword: entity",		"Disables the 'entity' keyword."},
	{&keyword_enum,			defaultkeyword, "enum",			"Keyword: enum",		"Disables the 'enum' keyword."},	//kinda like in c, but typedef not supported.
	{&keyword_enumflags,	defaultkeyword, "enumflags",	"Keyword: enumflags",	"Disables the 'enumflags' keyword."},	//like enum, but doubles instead of adds 1.
	{&keyword_extern,		defaultkeyword, "extern",		"Keyword: extern",		"Disables the 'extern' keyword. Use only on functions inside addons."},	//function is external, don't error or warn if the body was not found
	{&keyword_float,		defaultkeyword, "float",		"Keyword: float",		"Disables the 'float' keyword. (Disables the float keyword without 'local' preceeding it)"},
	{&keyword_for,			defaultkeyword, "for",			"Keyword: for",			"Disables the 'for' keyword. Syntax: for(assignment; while; increment) {codeblock;}"},
	{&keyword_goto,			defaultkeyword, "goto",			"Keyword: goto",		"Disables the 'goto' keyword."},
	{&keyword_int,			defaultkeyword, "int",			"Keyword: int",			"Disables the 'int' keyword."},
	{&keyword_integer,		defaultkeyword, "integer",		"Keyword: integer",		"Disables the 'integer' keyword."},
	{&keyword_noref,		defaultkeyword, "noref",		"Keyword: noref",		"Disables the 'noref' keyword."},	//nowhere else references this, don't strip it.
	{&keyword_nosave,		defaultkeyword, "nosave",		"Keyword: nosave",		"Disables the 'nosave' keyword."},	//don't write the def to the output.
	{&keyword_shared,		defaultkeyword, "shared",		"Keyword: shared",		"Disables the 'shared' keyword."},	//mark global to be copied over when progs changes (part of FTE_MULTIPROGS)
	{&keyword_state,		nondefaultkeyword,"state",		"Keyword: state",		"Disables the 'state' keyword."},
	{&keyword_string,		defaultkeyword, "string",		"Keyword: string",		"Disables the 'string' keyword."},
	{&keyword_struct,		defaultkeyword, "struct",		"Keyword: struct",		"Disables the 'struct' keyword."},
	{&keyword_switch,		defaultkeyword, "switch",		"Keyword: switch",		"Disables the 'switch' keyword."},
	{&keyword_thinktime,	nondefaultkeyword,"thinktime",	"Keyword: thinktime",	"Disables the 'thinktime' keyword which is used in HexenC"},
	{&keyword_typedef,		defaultkeyword, "typedef",		"Keyword: typedef",		"Disables the 'typedef' keyword."},	//fixme
	{&keyword_union,		defaultkeyword, "union",		"Keyword: union",		"Disables the 'union' keyword."},	//you surly know what a union is!
	{&keyword_var,			defaultkeyword, "var",			"Keyword: var",			"Disables the 'var' keyword."},
	{&keyword_vector,		defaultkeyword, "vector",		"Keyword: vector",		"Disables the 'vector' keyword."},


	//options
	{&keywords_coexist,		FLAG_ASDEFAULT, "kce",			"Keywords Coexist",		"If you want keywords to NOT be disabled when they a variable by the same name is defined, check here."},
	{&output_parms,			0,				"parms",		"Define offset parms",	"if PARM0 PARM1 etc should be defined by the compiler. These are useful if you make use of the asm keyword for function calls, or you wish to create your own variable arguments. This is an easy way to break decompilers."},	//controls weather to define PARMx for the parms (note - this can screw over some decompilers)
	{&autoprototype,		0,				"autoproto",	"Automatic Prototyping","Causes compilation to take two passes instead of one. The first pass, only the definitions are read. The second pass actually compiles your code. This means you never have to remember to prototype functions again."},	//so you no longer need to prototype functions and things in advance.
	{&writeasm,				0,				"wasm",			"Dump Assembler",		"Writes out a qc.asm which contains all your functions but in assembler. This is a great way to look for bugs in fteqcc, but can also be used to see exactly what your functions turn into, and thus how to optimise statements better."},			//spit out a qc.asm file, containing an assembler dump of the ENTIRE progs. (Doesn't include initialisation of constants)
	{&flag_ifstring,		FLAG_MIDCOMPILE,"ifstring",		"if(string) fix",		"Causes if(string) to behave identically to if(string!="") This is most useful with addons of course, but also has adverse effects with FRIK_FILE's fgets, where it becomes impossible to determin the end of the file. In such a case, you can still use asm {IF string 2;RETURN} to detect eof and leave the function."},		//correction for if(string) no-ifstring to get the standard behaviour.
	{&flag_laxcasts,		FLAG_MIDCOMPILE,"lax",			"Lax type checks",		"Disables many errors (generating warnings instead) when function calls or operations refer to two normally incompatable types. This is required for reacc support, and can also allow certain (evil) mods to compile that were originally written for frikqcc."},		//Allow lax casting. This'll produce loadsa warnings of course. But allows compilation of certain dodgy code.
	{&flag_hashonly,		FLAG_MIDCOMPILE,"hashonly",		"Hash-only constants", "Allows use of only #constant for precompiler constants, allows certain preqcc using mods to compile"},
	{&opt_logicops,			FLAG_MIDCOMPILE,"lo",			"Logic ops",			"This changes the behaviour of your code. It generates additional if operations to early-out in if statements. With this flag, the line if (0 && somefunction()) will never call the function. It can thus be considered an optimisation. However, due to the change of behaviour, it is not considered so by fteqcc. Note that due to inprecisions with floats, this flag can cause runaway loop errors within the player walk and run functions. This code is advised:\nplayer_stand1:\n    if (self.velocity_x || self.velocity_y)\nplayer_run\n    if (!(self.velocity_x || self.velocity_y))"},
	{&flag_fasttrackarrays,	FLAG_MIDCOMPILE|FLAG_ASDEFAULT,"fastarrays",	"fast arrays where possible",	"Generates extra instructions inside array handling functions to detect engine and use extension opcodes only in supporting engines.\nAdds a global which is set by the engine if the engine supports the extra opcodes. Note that this applies to all arrays or none."},		//correction for if(string) no-ifstring to get the standard behaviour.
	{NULL}
};

struct {
	targetformat_t target;
	char *name;
} targets[] = {
	{QCF_STANDARD,	"standard"},
	{QCF_STANDARD,	"q1"},
	{QCF_STANDARD,	"quakec"},
	{QCF_HEXEN2,	"hexen2"},
	{QCF_HEXEN2,	"h2"},
	{QCF_KK7,		"kkqwsv"},
	{QCF_KK7,		"kk7"},
	{QCF_KK7,		"bigprogs"},
	{QCF_KK7,		"version7"},
	{QCF_KK7,		"kkqwsv"},
	{QCF_FTE,		"fte"},
	{0,		NULL}
};

// CopyString returns an offset from the string heap
int	PR_CopyString (char *str)
{
	int		old;
	char *s;

	if (opt_noduplicatestrings)
	{
		if (!str || !*str)
			return 0;

		for (s = strings; s < strings+strofs; s++)
			if (!strcmp(s, str))
			{
				optres_noduplicatestrings += strlen(str);
				return s-strings;
			}
	}

	old = strofs;
	strcpy (strings+strofs, str);
	strofs += strlen(str)+1;
	return old;
}

int	PR_CopyDupBackString (char *str)
{
	int		old;
	char *s;

	for (s = strings+strofs-1; s>strings ; s--)
		if (!strcmp(s, str))
			return s-strings;
	
	old = strofs;
	strcpy (strings+strofs, str);
	strofs += strlen(str)+1;
	return old;
}

void PR_PrintStrings (void)
{
	int		i, l, j;
	
	for (i=0 ; i<strofs ; i += l)
	{
		l = strlen(strings+i) + 1;
		Msg ("%5i : ",i);
		for (j=0 ; j<l ; j++)
		{
			if (strings[i+j] == '\n')
			{
				putchar ('\\');
				putchar ('n');
			}
			else
				putchar (strings[i+j]);
		}
		Msg ("\n");
	}
}


/*void PR_PrintFunctions (void)
{
	int		i,j;
	dfunction_t	*d;
	
	for (i=0 ; i<numfunctions ; i++)
	{
		d = &functions[i];
		Msg ("%s : %s : %i %i (", strings + d->s_file, strings + d->s_name, d->first_statement, d->parm_start);
		for (j=0 ; j<d->numparms ; j++)
			Msg ("%i ",d->parm_size[j]);
		Msg (")\n");
	}
}*/

void PR_PrintFields (void)
{
	int		i;
	ddef_t	*d;
	
	for (i=0 ; i<numfielddefs ; i++)
	{
		d = &fields[i];
		Msg ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}

void PR_PrintGlobals (void)
{
	int		i;
	ddef_t	*d;
	
	for (i=0 ; i<numglobaldefs ; i++)
	{
		d = &qcc_globals[i];
		Msg ("%5i : (%i) %s\n", d->ofs, d->type, strings + d->s_name);
	}
}

int encode(int len, int method, char *in, int handle);
int WriteSourceFiles(vfile_t *h, dprograms_t *progs, bool sourceaswell)
{
	includeddatafile_t *idf;
	cachedsourcefile_t *f;
	int num=0;
	int ofs;

	/*
	for (f = sourcefile; f ; f=f->next)
	{
		if (f->type == FT_CODE && !sourceaswell)
			continue;

		VFS_Write(h, f->filename, strlen(f->filename)+1);
		i = LittleLong(f->size);
		VFS_Write(h, &i, sizeof(int));

		i = LittleLong(encrpytmode);
		VFS_Write(h, &i, sizeof(int));

		if (encrpytmode)
			for (i = 0; i < f->size; i++)
				f->file[i] ^= 0xA5;

		VFS_Write(h, f->file, f->size);
	}*/

	for (f = sourcefile,num=0; f ; f=f->next)
	{
		if (f->type == FT_CODE && !sourceaswell)
			continue;

		num++;
	}
	if (!num)
		return 0;
	idf = Qalloc(sizeof(includeddatafile_t)*num);
	for (f = sourcefile,num=0; f ; f=f->next)
	{
		if (f->type == FT_CODE && !sourceaswell)
			continue;

		strcpy(idf[num].filename, f->filename);
		idf[num].size = f->size;
		idf[num].compmethod = 1;
		idf[num].ofs = VFS_Tell(h);
		idf[num].compsize = PR_encode(f->size, idf[num].compmethod, f->file, h);
		num++;
	}

	ofs = VFS_Tell(h);	
	VFS_Write(h, &num, sizeof(int));
	VFS_Write(h, idf, sizeof(includeddatafile_t)*num);

	sourcefile = NULL;

	return ofs;
}

void PR_InitData (void)
{
	static char parmname[12][MAX_PARMS];
	static temp_t ret_temp;
	int		i;

	sourcefile = NULL;
	
	numstatements = 1;
	strofs = 1;
	numfunctions = 1;
	numglobaldefs = 1;
	numfielddefs = 1;

	memset(&ret_temp, 0, sizeof(ret_temp));
	
	def_ret.ofs = OFS_RETURN;
	def_ret.name = "return";
	def_ret.temp = &ret_temp;
	def_ret.constant = false;
	def_ret.type	= NULL;
	ret_temp.ofs = def_ret.ofs;
	ret_temp.scope = NULL;
	ret_temp.size = 3;
	ret_temp.next = NULL;
	for (i=0 ; i<MAX_PARMS ; i++)
	{
		def_parms[i].temp = NULL;
		def_parms[i].type = NULL;
		def_parms[i].ofs = OFS_PARM0 + 3*i;
		def_parms[i].name = parmname[i];
		sprintf(parmname[i], "parm%i", i);
	}
}

int WriteBodylessFuncs (vfile_t *handle)
{
	def_t		*d;
	int		ret = 0;

	for (d = pr.def_head.next; d; d = d->next)
	{
		if (d->type->type == ev_function && !d->scope)// function parms are ok
		{
			if (d->initialized != 1 && d->references > 0)
			{
				VFS_Write(handle, d->name, strlen(d->name)+1);
				ret++;
			}
		}
	}

	return ret;
}

//marshalled locals remaps all the functions to use the range MAX_REGS onwards for the offset to thier locals.
//this function remaps all the locals back into the function.
void PR_UnmarshalLocals(void)
{
	def_t		*def;
	unsigned int ofs;
	unsigned int maxo;
	int i;

	ofs = numpr_globals;
	maxo = ofs;

	for (def = pr.def_head.next ; def ; def = def->next)
	{
		if (def->ofs >= MAX_REGS)	//unmap defs.
		{
			def->ofs = def->ofs + ofs - MAX_REGS;
			if (maxo < def->ofs)
				maxo = def->ofs;
		}
	}

	for (i = 0; i < numfunctions; i++)
	{
		if (functions[i].parm_start == MAX_REGS)
			functions[i].parm_start = ofs;
	}

	PR_RemapOffsets(0, numstatements, MAX_REGS, MAX_REGS + maxo-numpr_globals + 3, ofs);

	numpr_globals = maxo+3;
	if (numpr_globals > MAX_REGS)
		Sys_Error("Too many globals are in use to unmarshal all locals");

	if (maxo-ofs)
		Msg("Total of %i marshalled globals\n", maxo-ofs);
}

const_t *PR_CheckCompConstDefined(char *def);

void PR_WriteData (int crc)
{
	char element[MAX_NAME];
	def_t		*def, *comp_x, *comp_y, *comp_z;
	ddef_t		*dd;
	dprograms_t	progs;
	vfile_t		*h;
	file_t		*pr_file;
	int		i, num_ref, len;
	bool debugtarget = false;
	bool types = false;
	int outputsize = 16;

	progs.blockscompressed=0;

	if (numstatements > MAX_STATEMENTS)
		Sys_Error("Too many statements - %i\nAdd \"MAX_STATEMENTS\" \"%i\" to qcc.cfg", numstatements, (numstatements+32768)&~32767);

	if (strofs > MAX_STRINGS)
		Sys_Error("Too many strings - %i\nAdd \"MAX_STRINGS\" \"%i\" to qcc.cfg", strofs, (strofs+32768)&~32767);

	PR_UnmarshalLocals();

	switch (targetformat)
	{
	case QCF_HEXEN2:
	case QCF_STANDARD:
		if (bodylessfuncs)
			Msg("Warning: There are some functions without bodies.\n");

		if (numpr_globals > 65530 )
		{
			Msg("Forcing target to FTE32 due to numpr_globals\n");
			outputsize = 32;
		}
		else if (targetformat == QCF_HEXEN2)
		{
			Msg("Progs execution requires a Hexen2 compatable engine\n");
			break;
		}
		else
		{
			if (numpr_globals >= 32768)	//not much of a different format. Rewrite output to get it working on original executors?
				Msg("An enhanced executor will be required (FTE/QF/KK)\n");
			else
				Msg("Progs should run on any Quake executor\n");
			break;
		}
		//intentional
		targetformat = QCF_FTE;
	case QCF_FTEDEBUG:
	case QCF_FTE:
		if (targetformat == QCF_FTEDEBUG)
			debugtarget = true;

		if (numpr_globals > 65530)
		{
			Msg("Using 32 bit target due to numpr_globals\n");
			outputsize = 32;
		}

		//compression of blocks?
		if (compressoutput)	progs.blockscompressed |=1;		//statements
		if (compressoutput)	progs.blockscompressed |=2;		//defs
		if (compressoutput)	progs.blockscompressed |=4;		//fields
		if (compressoutput)	progs.blockscompressed |=8;		//functions
		if (compressoutput)	progs.blockscompressed |=16;		//strings
		if (compressoutput)	progs.blockscompressed |=32;		//globals
		if (compressoutput)	progs.blockscompressed |=64;		//line numbers
		if (compressoutput)	progs.blockscompressed |=128;		//types
		//include a type block?
		types = debugtarget;//!!PR_CheckCompConstDefined("TYPES");	//useful for debugging and saving (maybe, anyway...).

		Msg("An FTE executor will be required\n");
		break;
	case QCF_KK7:
		if (bodylessfuncs)
			Msg("Warning: There are some functions without bodies.\n");
		if (numpr_globals > 65530)
			Msg("Warning: Saving is not supported. Ensure all engine read fields and globals are defined early on.\n");

		Msg("A KK compatable executor will be required (FTE/KK)\n");
		break;
	}

	//part of how compilation works. This def is always present, and never used.
	def = PR_GetDef(NULL, "end_sys_globals", NULL, false, 0);
	if (def) def->references++;

	def = PR_GetDef(NULL, "end_sys_fields", NULL, false, 0);
	if (def) def->references++;

	for (def = pr.def_head.next ; def ; def = def->next)
	{
		if (def->type->type == ev_vector || (def->type->type == ev_field && def->type->aux_type->type == ev_vector))
		{	
			//do the references, so we don't get loadsa not referenced VEC_HULL_MINS_x
			sprintf(element, "%s_x", def->name);
			comp_x = PR_GetDef(NULL, element, def->scope, false, 0);
			sprintf(element, "%s_y", def->name);
			comp_y = PR_GetDef(NULL, element, def->scope, false, 0);
			sprintf(element, "%s_z", def->name);
			comp_z = PR_GetDef(NULL, element, def->scope, false, 0);

			num_ref = def->references;
			if (comp_x && comp_y && comp_z)
			{
				num_ref += comp_x->references;
				num_ref += comp_y->references;
				num_ref += comp_z->references;

				if (!def->references)
				{
					if (!comp_x->references || !comp_y->references || !comp_z->references)
					{	
						//one of these vars is useless...
						num_ref = 0;
					}
				}
				def->references = num_ref;
				
					
				if (!num_ref) num_ref = 1;
				if (comp_x) comp_x->references = num_ref;
				if (comp_y) comp_y->references = num_ref;
				if (comp_z) comp_z->references = num_ref;
			}
		}
		if (def->references <= 0)
		{
			if(def->local) PR_Warning(WARN_NOTREFERENCED, strings + def->s_file, def->s_line, "'%s' : unreferenced local variable", def->name);
			if (opt_unreferenced && def->type->type != ev_field)
			{
				optres_unreferenced++;
				continue;
			}
		}

		if (def->type->type == ev_function)
		{
			if (opt_function_names && functions[G_FUNCTION(def->ofs)].first_statement<0)
			{
				optres_function_names++;
				def->name = "";
			}
			if (!def->timescalled)
			{
				if (def->references <= 1)
					PR_Warning(WARN_DEADCODE, strings + def->s_file, def->s_line, "%s is never directly called or referenced (spawn function or dead code)", def->name);
//				else
//					PR_Warning(WARN_DEADCODE, strings + def->s_file, def->s_line, "%s is never directly called", def->name);
			}
			if (opt_stripfunctions && def->timescalled >= def->references-1)	//make sure it's not copied into a different var.
			{								//if it ever does self.think then it could be needed for saves.
				optres_stripfunctions++;	//if it's only ever called explicitly, the engine doesn't need to know.
				continue;
			}

//			df = &functions[numfunctions];
//			numfunctions++;

		}
		else if (def->type->type == ev_field)// && !def->constant)
		{
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = PR_CopyString (def->name);
			dd->ofs = G_INT(def->ofs);
		}
		else if ((def->scope||def->constant) && (def->type->type != ev_string || opt_constant_names_strings))
		{
			if (opt_constant_names)
			{
				if (def->type->type == ev_string)
					optres_constant_names_strings += strlen(def->name);
				else
					optres_constant_names += strlen(def->name);
				continue;
			}
		}

//		if (!def->saved && def->type->type != ev_string)
//			continue;
		dd = &qcc_globals[numglobaldefs];
		numglobaldefs++;

		if (types)
			dd->type = def->type-qcc_typeinfo;
		else
			dd->type = def->type->type;

		if ( def->saved && ((!def->initialized || def->type->type == ev_function) && def->type->type != ev_field && def->scope == NULL))
			dd->type |= DEF_SAVEGLOBAL;

		if (def->shared)
			dd->type |= DEF_SHARED;

		if (opt_locals && (def->scope || !strcmp(def->name, "IMMEDIATE")))
		{
			dd->s_name = 0;
			optres_locals += strlen(def->name);
		}
		else
			dd->s_name = PR_CopyString (def->name);
		dd->ofs = def->ofs;
	}

	if (numglobaldefs > MAX_GLOBALS)
		Sys_Error("Too many globals - %i\nAdd \"MAX_GLOBALS\" \"%i\" to qcc.cfg", numglobaldefs, (numglobaldefs+32768)&~32767);

	strofs = (strofs+3)&~3;

	Msg ("%6i strofs (of %i)\n", strofs, MAX_STRINGS);
	Msg ("%6i numstatements (of %i)\n", numstatements, MAX_STATEMENTS);
	Msg ("%6i numfunctions (of %i)\n", numfunctions, MAX_FUNCTIONS);
	Msg ("%6i numglobaldefs (of %i)\n", numglobaldefs, MAX_GLOBALS);
	Msg ("%6i numfielddefs (%i unique) (of %i)\n", numfielddefs, pr.size_fields, MAX_FIELDS);
	Msg ("%6i numpr_globals (of %i)\n", numpr_globals, MAX_REGS);	
	
	if (!*destfile) strcpy(destfile, "progs.dat");
	Msg("Writing %s\n", destfile);
	
	pr_file = FS_Open( destfile, "wb" );
	h = VFS_Open (pr_file, "w" );

	VFS_Write (h, &progs, sizeof(progs));
	VFS_Write (h, "\r\n\r\n", 4);
	VFS_Write (h, v_copyright, strlen(v_copyright)+1);
	VFS_Write (h, "\r\n\r\n", 4);

	while(VFS_Tell(h) & 3)//this is a lame way to do it
	{
		VFS_Seek (h, 0, SEEK_CUR);
		VFS_Write (h, "\0", 1);
	}

	progs.ofs_strings = VFS_Tell(h);
	progs.numstrings = strofs;

	if (progs.blockscompressed&16)
	{		
		VFS_Write (h, &len, sizeof(int));	//save for later
		len = PR_encode(strofs*sizeof(char), 2, (char *)strings, h);	//write
		i = VFS_Tell(h);
		VFS_Seek(h, progs.ofs_strings, SEEK_SET);//seek back
		len = LittleLong(len);
		VFS_Write (h, &len, sizeof(int));	//write size.
		VFS_Seek(h, i, SEEK_SET);
	}
	else
		VFS_Write (h, strings, strofs);

	progs.ofs_statements = VFS_Tell(h);
	progs.numstatements = numstatements;

	if (targetformat == QCF_HEXEN2)
	{
		for (i=0 ; i<numstatements ; i++)
		{
			if (statements[i].op >= OP_CALL1 && statements[i].op <= OP_CALL8)
				Sys_Error("Target switching produced incompatable instructions");
			else if (statements[i].op >= OP_CALL1H && statements[i].op <= OP_CALL8H)
				statements[i].op = statements[i].op - OP_CALL1H + OP_CALL1;
		}
	}

	for (i=0 ; i<numstatements ; i++)

	switch(targetformat == QCF_KK7?32:outputsize)	//KK7 sucks.
	{
	case 32:
		for (i=0 ; i<numstatements ; i++)
		{
			statements[i].op = LittleLong/*LittleShort*/(statements[i].op);
			statements[i].a = LittleLong/*LittleShort*/(statements[i].a);
			statements[i].b = LittleLong/*LittleShort*/(statements[i].b);
			statements[i].c = LittleLong/*LittleShort*/(statements[i].c);
		}
		
		if (progs.blockscompressed&1)
		{		
			VFS_Write (h, &len, sizeof(int));	//save for later
			len = PR_encode(numstatements*sizeof(dstatement32_t), 2, (char *)statements, h);	//write
			i = VFS_Tell(h);
			VFS_Seek(h, progs.ofs_statements, SEEK_SET);//seek back
			len = LittleLong(len);
			VFS_Write (h, &len, sizeof(int));	//write size.
			VFS_Seek(h, i, SEEK_SET);
		}
		else
			VFS_Write (h, statements, numstatements*sizeof(dstatement32_t));
		break;
	case 16:
#define statements16 ((dstatement16_t*) statements)
		for (i=0 ; i<numstatements ; i++)	//resize as we go - scaling down
		{
			statements16[i].op = LittleShort((unsigned short)statements[i].op);
			if (statements[i].a < 0)
				statements16[i].a = LittleShort((short)statements[i].a);
			else
				statements16[i].a = (unsigned short)LittleShort((unsigned short)statements[i].a);
			if (statements[i].b < 0)
				statements16[i].b = LittleShort((short)statements[i].b);
			else
				statements16[i].b = (unsigned short)LittleShort((unsigned short)statements[i].b);
			if (statements[i].c < 0)
				statements16[i].c = LittleShort((short)statements[i].c);
			else
				statements16[i].c = (unsigned short)LittleShort((unsigned short)statements[i].c);
		}
		
		if (progs.blockscompressed&1)
		{		
			VFS_Write (h, &len, sizeof(int));	//save for later
			len = PR_encode(numstatements*sizeof(dstatement16_t), 2, (char *)statements16, h);	//write
			i = VFS_Tell(h);
			VFS_Seek(h, progs.ofs_statements, SEEK_SET);//seek back
			len = LittleLong(len);
			VFS_Write (h, &len, sizeof(int));	//write size.
			VFS_Seek(h, i, SEEK_SET);
		}
		else
			VFS_Write (h, statements16, numstatements*sizeof(dstatement16_t));
		break;
	default:
		Sys_Error("intsize error");
	}

	progs.ofs_functions = VFS_Tell(h);
	progs.numfunctions = numfunctions;
	for (i=0 ; i<numfunctions ; i++)
	{
		functions[i].first_statement = LittleLong (functions[i].first_statement);
		functions[i].parm_start = LittleLong (functions[i].parm_start);
		functions[i].s_name = LittleLong (functions[i].s_name);
		functions[i].s_file = LittleLong (functions[i].s_file);
		functions[i].numparms = LittleLong ((functions[i].numparms>MAX_PARMS)?MAX_PARMS:functions[i].numparms);
		functions[i].locals = LittleLong (functions[i].locals);
	}

	if (progs.blockscompressed&8)
	{		
		VFS_Write (h, &len, sizeof(int));	//save for later
		len = PR_encode(numfunctions*sizeof(dfunction_t), 2, (char *)functions, h);	//write
		i = VFS_Tell(h);
		VFS_Seek(h, progs.ofs_functions, SEEK_SET);//seek back
		len = LittleLong(len);
		VFS_Write (h, &len, sizeof(int));	//write size.
		VFS_Seek(h, i, SEEK_SET);
	}
	else
		VFS_Write (h, functions, numfunctions*sizeof(dfunction_t));

	switch(outputsize)
	{
	case 32:
		progs.ofs_globaldefs = VFS_Tell(h);
		progs.numglobaldefs = numglobaldefs;
		for (i=0 ; i<numglobaldefs ; i++)
		{
			qcc_globals[i].type = LittleLong/*LittleShort*/ (qcc_globals[i].type);
			qcc_globals[i].ofs = LittleLong/*LittleShort*/ (qcc_globals[i].ofs);
			qcc_globals[i].s_name = LittleLong (qcc_globals[i].s_name);
		}

		if (progs.blockscompressed&2)
		{		
			VFS_Write (h, &len, sizeof(int));	//save for later
			len = PR_encode(numglobaldefs*sizeof(ddef_t), 2, (char *)qcc_globals, h);	//write
			i = VFS_Tell(h);
			VFS_Seek(h, progs.ofs_globaldefs, SEEK_SET);//seek back
			len = LittleLong(len);
			VFS_Write (h, &len, sizeof(int));	//write size.
			VFS_Seek(h, i, SEEK_SET);
		}
		else
			VFS_Write (h, qcc_globals, numglobaldefs*sizeof(ddef_t));

		progs.ofs_fielddefs = VFS_Tell(h);
		progs.numfielddefs = numfielddefs;

		for (i=0 ; i<numfielddefs ; i++)
		{
			fields[i].type = LittleLong/*LittleShort*/ (fields[i].type);
			fields[i].ofs = LittleLong/*LittleShort*/ (fields[i].ofs);
			fields[i].s_name = LittleLong (fields[i].s_name);
		}

		if (progs.blockscompressed&4)
		{		
			VFS_Write (h, &len, sizeof(int));	//save for later
			len = PR_encode(numfielddefs*sizeof(ddef_t), 2, (char *)fields, h);	//write
			i = VFS_Tell(h);
			VFS_Seek(h, progs.ofs_fielddefs, SEEK_SET);//seek back
			len = LittleLong(len);
			VFS_Write (h, &len, sizeof(int));	//write size.
			VFS_Seek(h, i, SEEK_SET);
		}
		else
			VFS_Write (h, fields, numfielddefs*sizeof(ddef_t));
		break;
	case 16:
#define qcc_globals16 ((ddef16_t*)qcc_globals)
#define fields16 ((ddef16_t*)fields)
		progs.ofs_globaldefs = VFS_Tell(h);
		progs.numglobaldefs = numglobaldefs;
		for (i=0 ; i<numglobaldefs ; i++)
		{
			qcc_globals16[i].type = (unsigned short)LittleShort ((unsigned short)qcc_globals[i].type);
			qcc_globals16[i].ofs = (unsigned short)LittleShort ((unsigned short)qcc_globals[i].ofs);
			qcc_globals16[i].s_name = LittleLong (qcc_globals[i].s_name);
		}

		if (progs.blockscompressed&2)
		{		
			VFS_Write (h, &len, sizeof(int));	//save for later
			len = PR_encode(numglobaldefs*sizeof(ddef16_t), 2, (char *)qcc_globals16, h);	//write
			i = VFS_Tell(h);
			VFS_Seek(h, progs.ofs_globaldefs, SEEK_SET);//seek back
			len = LittleLong(len);
			VFS_Write (h, &len, sizeof(int));	//write size.
			VFS_Seek(h, i, SEEK_SET);
		}
		else
			VFS_Write (h, qcc_globals16, numglobaldefs*sizeof(ddef16_t));

		progs.ofs_fielddefs = VFS_Tell(h);
		progs.numfielddefs = numfielddefs;

		for (i=0 ; i<numfielddefs ; i++)
		{
			fields16[i].type = (unsigned short)LittleShort ((unsigned short)fields[i].type);
			fields16[i].ofs = (unsigned short)LittleShort ((unsigned short)fields[i].ofs);
			fields16[i].s_name = LittleLong (fields[i].s_name);
		}

		if (progs.blockscompressed&4)
		{		
			VFS_Write (h, &len, sizeof(int));	//save for later
			len = PR_encode(numfielddefs*sizeof(ddef16_t), 2, (char *)fields16, h);	//write
			i = VFS_Tell(h);
			VFS_Seek(h, progs.ofs_fielddefs, SEEK_SET);//seek back
			len = LittleLong(len);
			VFS_Write (h, &len, sizeof(int));	//write size.
			VFS_Seek(h, i, SEEK_SET);
		}
		else
			VFS_Write (h, fields16, numfielddefs*sizeof(ddef16_t));
		break;
	default:
		Sys_Error("intsize error");
	}

	progs.ofs_globals = VFS_Tell(h);
	progs.numglobals = numpr_globals;

	for (i=0 ; (unsigned)i<numpr_globals ; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);

	if (progs.blockscompressed&32)
	{		
		VFS_Write (h, &len, sizeof(int));	//save for later
		len = PR_encode(numpr_globals*4, 2, (char *)pr_globals, h);	//write
		i = VFS_Tell(h);
		VFS_Seek(h, progs.ofs_globals, SEEK_SET);//seek back
		len = LittleLong(len);
		VFS_Write (h, &len, sizeof(int));	//write size.
		VFS_Seek(h, i, SEEK_SET);
	}
	else
		VFS_Write (h, pr_globals, numpr_globals*4);	

	if (types)
	for (i=0 ; i<numtypeinfos ; i++)
	{
		if (qcc_typeinfo[i].aux_type)
			qcc_typeinfo[i].aux_type = (type_t*)(qcc_typeinfo[i].aux_type - qcc_typeinfo);
		if (qcc_typeinfo[i].next)
			qcc_typeinfo[i].next = (type_t*)(qcc_typeinfo[i].next - qcc_typeinfo);
		qcc_typeinfo[i].name = (char *)PR_CopyDupBackString(qcc_typeinfo[i].name);
	}

	progs.ofsfiles = 0;
	progs.ofslinenums = 0;
	progs.header = 0;
	progs.ofsbodylessfuncs = 0;
	progs.numbodylessfuncs = 0;
	progs.ofs_types = 0;
	progs.numtypes = 0;

	switch(targetformat)
	{
	case QCF_KK7:
		progs.version = PROG_EXTENDEDVERSION;
		break;
	case QCF_STANDARD:
	case QCF_HEXEN2:	//urgh
		progs.version = VPROGS_VERSION;
		break;
	case QCF_FTE:
	case QCF_FTEDEBUG:
		progs.version = PROG_EXTENDEDVERSION;

		if (outputsize == 32) progs.header = VPROGSHEADER32;
		else progs.header = VPROGSHEADER16;

		progs.ofsbodylessfuncs = VFS_Tell(h);
		progs.numbodylessfuncs = WriteBodylessFuncs(h);		

		if (debugtarget)
		{
			progs.ofslinenums = VFS_Tell(h);
			if (progs.blockscompressed&64)
			{		
				VFS_Write (h, &len, sizeof(int));	//save for later
				len = PR_encode(numstatements*sizeof(int), 2, (char *)statement_linenums, h);	//write
				i = VFS_Tell(h);
				VFS_Seek(h, progs.ofslinenums, SEEK_SET);//seek back
				len = LittleLong(len);
				VFS_Write (h, &len, sizeof(int));	//write size.
				VFS_Seek(h, i, SEEK_SET);
			}
			else VFS_Write (h, statement_linenums, numstatements * sizeof(int));
		}
		else progs.ofslinenums = 0;

		if (types)
		{
			progs.ofs_types = VFS_Tell(h);
			if (progs.blockscompressed & 128)
			{		
				VFS_Write (h, &len, sizeof(int));	//save for later
				len = PR_encode(sizeof(type_t)*numtypeinfos, 2, (char *)qcc_typeinfo, h);	//write
				i = VFS_Tell(h);
				VFS_Seek(h, progs.ofs_types, SEEK_SET);//seek back#
				len = LittleLong(len);
				VFS_Write (h, &len, sizeof(int));	//write size.
				VFS_Seek(h, i, SEEK_SET);
			}
			else
				VFS_Write (h, qcc_typeinfo, sizeof(type_t)*numtypeinfos);
			progs.numtypes = numtypeinfos;		
		}
		else
		{
			progs.ofs_types = 0;
			progs.numtypes = 0;
		}

		progs.ofsfiles = WriteSourceFiles(h, &progs, debugtarget);
		break;
	}

	Msg ("%6i TOTAL SIZE\n", VFS_Tell(h));

	progs.entityfields = pr.size_fields;

	progs.crc = crc;

	// byte swap the header and write it out
	for (i=0 ; i<sizeof(progs)/4 ; i++)
		((int *)&progs)[i] = LittleLong ( ((int *)&progs)[i] );		

	
	VFS_Seek (h, 0, SEEK_SET);
	VFS_Write (h, &progs, sizeof(progs));
	VFS_Close (h);
}



/*
===============
PR_String

Returns a string suitable for printing (no newlines, max 60 chars length)
===============
*/
char *PR_String (char *string)
{
	static char buf[80];
	char	*s;
	
	s = buf;
	*s++ = '"';
	while (string && *string)
	{
		if (s == buf + sizeof(buf) - 2)
			break;
		if (*string == '\n')
		{
			*s++ = '\\';
			*s++ = 'n';
		}
		else if (*string == '"')
		{
			*s++ = '\\';
			*s++ = '"';
		}
		else
			*s++ = *string;
		string++;
		if (s - buf > 60)
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;
	return buf;
}



def_t *PR_DefForFieldOfs (gofs_t ofs)
{
	def_t	*d;
	
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (d->type->type != ev_field)
			continue;
		if (*((uint *)&pr_globals[d->ofs]) == ofs)
			return d;
	}
	Sys_Error ("PR_DefForFieldOfs: couldn't find %i",ofs);
	return NULL;
}

/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (etype_t type, void *val)
{
	static char	line[256];
	def_t		*def;
	dfunction_t	*f;
	
	switch (type)
	{
	case ev_string:
		sprintf (line, "%s", PR_String(strings + *(int *)val));
		break;
	case ev_entity:	
		sprintf (line, "entity %i", *(int *)val);
		break;
	case ev_function:
		f = functions + *(int *)val;
		if (!f)
			sprintf (line, "undefined function");
		else
			sprintf (line, "%s()", strings + f->s_name);
		break;
	case ev_field:
		def = PR_DefForFieldOfs ( *(int *)val );
		sprintf (line, ".%s", def->name);
		break;
	case ev_void:
		sprintf (line, "void");
		break;
	case ev_float:
		sprintf (line, "%5.1f", *(float *)val);
		break;
	case ev_integer:
		sprintf (line, "%i", *(int *)val);
		break;
	case ev_vector:
		sprintf (line, "'%5.1f %5.1f %5.1f'", ((float *)val)[0], ((float *)val)[1], ((float *)val)[2]);
		break;
	case ev_pointer:
		sprintf (line, "pointer");
		break;
	default:
		sprintf (line, "bad type %i", type);
		break;
	}
	
	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
/*char *PR_GlobalStringNoContents (gofs_t ofs)
{
	int		i;
	def_t	*def;
	void	*val;
	static char	line[128];
	
	val = (void *)&pr_globals[ofs];
	def = pr_global_defs[ofs];
	if (!def)
//		Error ("PR_GlobalString: no def for %i", ofs);
		sprintf (line,"%i(?""?""?)", ofs);
	else
		sprintf (line,"%i(%s)", ofs, def->name);
	
	i = strlen(line);
	for ( ; i<16 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}

char *PR_GlobalString (gofs_t ofs)
{
	char	*s;
	int		i;
	def_t	*def;
	void	*val;
	static char	line[128];
	
	val = (void *)&pr_globals[ofs];
	def = pr_global_defs[ofs];
	if (!def)
		return PR_GlobalStringNoContents(ofs);
	if (def->initialized && def->type->type != ev_function)
	{
		s = PR_ValueString (def->type->type, &pr_globals[ofs]);
		sprintf (line,"%i(%s)", ofs, s);
	}
	else
		sprintf (line,"%i(%s)", ofs, def->name);
	
	i = strlen(line);
	for ( ; i<16 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}*/

/*
============
PR_PrintOfs
============
*/
/*void PR_PrintOfs (gofs_t ofs)
{
	Msg ("%s\n",PR_GlobalString(ofs));
}*/

/*
=================
PR_PrintStatement
=================
*/
/*void PR_PrintStatement (dstatement_t *s)
{
	int		i;
	
	Msg ("%4i : %4i : %s ", (int)(s - statements), statement_linenums[s-statements], pr_opcodes[s->op].opname);
	i = strlen(pr_opcodes[s->op].opname);
	for ( ; i<10 ; i++)
		Msg (" ");
		
	if (s->op == OP_IF || s->op == OP_IFNOT)
		Msg ("%sbranch %i",PR_GlobalString(s->a),s->b);
	else if (s->op == OP_GOTO)
	{
		Msg ("branch %i",s->a);
	}
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Msg ("%s",PR_GlobalString(s->a));
		Msg ("%s", PR_GlobalStringNoContents(s->b));
	}
	else
	{
		if (s->a)
			Msg ("%s",PR_GlobalString(s->a));
		if (s->b)
			Msg ("%s",PR_GlobalString(s->b));
		if (s->c)
			Msg ("%s", PR_GlobalStringNoContents(s->c));
	}
	Msg ("\n");
}*/


/*
============
PR_PrintDefs
============
*/
/*void PR_PrintDefs (void)
{
	def_t	*d;
	
	for (d=pr.def_head.next ; d ; d=d->next)
		PR_PrintOfs (d->ofs);
}*/

type_t *PR_NewType (char *name, int basictype)
{
	if (numtypeinfos>= maxtypeinfos)
		Sys_Error("Too many types");
	memset(&qcc_typeinfo[numtypeinfos], 0, sizeof(type_t));
	qcc_typeinfo[numtypeinfos].type = basictype;
	qcc_typeinfo[numtypeinfos].name = name;
	qcc_typeinfo[numtypeinfos].num_parms = 0;
	qcc_typeinfo[numtypeinfos].param = NULL;
	qcc_typeinfo[numtypeinfos].size = type_size[basictype];	


	numtypeinfos++;

	return &qcc_typeinfo[numtypeinfos-1];
}

/*
==============
PR_BeginCompilation

called before compiling a batch of files, clears the pr struct
==============
*/
void	PR_BeginCompilation (void *memory, int memsize)
{
	extern int recursivefunctiontype;
	extern struct freeoffset_s *freeofs;
	int		i;
	char name[16];
	
	pr.memory = memory;
	pr.max_memory = memsize;

	pr.def_tail = &pr.def_head;

	PR_ResetErrorScope();
	pr_scope = NULL;

/*	numpr_globals = RESERVED_OFS;	
	
	for (i=0 ; i<RESERVED_OFS ; i++)
		pr_global_defs[i] = &def_void;
*/
	
	type_void = PR_NewType("void", ev_void);
	type_string = PR_NewType("string", ev_string);
	type_float = PR_NewType("float", ev_float);
	type_vector = PR_NewType("vector", ev_vector);
	type_entity = PR_NewType("entity", ev_entity);
	type_field = PR_NewType("field", ev_field);	
	type_function = PR_NewType("function", ev_function);
	type_pointer = PR_NewType("pointer", ev_pointer);	
	type_integer = PR_NewType("__integer", ev_integer);
	type_variant = PR_NewType("__variant", ev_variant);

	type_floatfield = PR_NewType("fieldfloat", ev_field);
	type_floatfield->aux_type = type_float;
	type_pointer->aux_type = PR_NewType("pointeraux", ev_float);

	type_function->aux_type = type_void;

	//type_field->aux_type = type_float;

	if (keyword_int)
		PR_NewType("int", ev_integer);
	if (keyword_integer)
		PR_NewType("integer", ev_integer);
	


	if (output_parms)
	{	//this tends to confuse the brains out of decompilers. :)
		numpr_globals = 1;
		PR_GetDef(type_vector, "RETURN", NULL, true, 1)->references++;
		for (i = 0; i < MAX_PARMS; i++)
		{
			sprintf(name, "PARM%i", i);
			PR_GetDef(type_vector, name, NULL, true, 1)->references++;
		}
	}
	else
	{
		numpr_globals = RESERVED_OFS;
//		for (i=0 ; i<RESERVED_OFS ; i++)
//			pr_global_defs[i] = NULL;
	}

// link the function type in so state forward declarations match proper type
	pr.types = NULL;
//	type_function->next = NULL;
	pr_error_count = 0;
	pr_warning_count = 0;
	recursivefunctiontype = 0;

	freeofs = NULL;
}

/*
==============
PR_FinishCompilation

called after all files are compiled to check for errors
Returns false if errors were detected.
==============
*/
int PR_FinishCompilation (void)
{
	def_t		*d;
	int		errors = pr_error_count;
	
	// check to make sure all functions prototyped have code
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (d->type->type == ev_function && !d->scope)// function parms are ok
		{
			if (d->initialized == 0)
			{
				if (!strncmp(d->name, "ArrayGet*", 9))
				{
					PR_EmitArrayGetFunction(d, d->name + 9);
					pr_scope = NULL;
				}
				else if (!strncmp(d->name, "ArraySet*", 9))
				{
					PR_EmitArraySetFunction(d, d->name + 9);
					pr_scope = NULL;
				}
				else if (!strncmp(d->name, "Class*", 6))
				{
					PR_EmitClassFromFunction(d, d->name + 6);
					pr_scope = NULL;
				}
				else
				{
					PR_Warning(WARN_NOTDEFINED, strings + d->s_file, d->s_line, "function %s was not defined",d->name);
					bodylessfuncs = true;
				}
			}
			else if (d->initialized == 2) bodylessfuncs = true;
		}
	}
	pr_scope = NULL;

	return (errors == 0);
}

//=============================================================================

// FIXME: byte swap?

// this is a 16 bit, non-reflected CRC using the polynomial 0x1021
// and the initial and final xor values shown below...  in other words, the
// CCITT standard CRC used by XMODEM


#define CRC_INIT_VALUE	0xffff
#define CRC_XOR_VALUE	0x0000

static unsigned short PR_crctable[256] =
{
	0x0000,	0x1021,	0x2042,	0x3063,	0x4084,	0x50a5,	0x60c6,	0x70e7,
	0x8108,	0x9129,	0xa14a,	0xb16b,	0xc18c,	0xd1ad,	0xe1ce,	0xf1ef,
	0x1231,	0x0210,	0x3273,	0x2252,	0x52b5,	0x4294,	0x72f7,	0x62d6,
	0x9339,	0x8318,	0xb37b,	0xa35a,	0xd3bd,	0xc39c,	0xf3ff,	0xe3de,
	0x2462,	0x3443,	0x0420,	0x1401,	0x64e6,	0x74c7,	0x44a4,	0x5485,
	0xa56a,	0xb54b,	0x8528,	0x9509,	0xe5ee,	0xf5cf,	0xc5ac,	0xd58d,
	0x3653,	0x2672,	0x1611,	0x0630,	0x76d7,	0x66f6,	0x5695,	0x46b4,
	0xb75b,	0xa77a,	0x9719,	0x8738,	0xf7df,	0xe7fe,	0xd79d,	0xc7bc,
	0x48c4,	0x58e5,	0x6886,	0x78a7,	0x0840,	0x1861,	0x2802,	0x3823,
	0xc9cc,	0xd9ed,	0xe98e,	0xf9af,	0x8948,	0x9969,	0xa90a,	0xb92b,
	0x5af5,	0x4ad4,	0x7ab7,	0x6a96,	0x1a71,	0x0a50,	0x3a33,	0x2a12,
	0xdbfd,	0xcbdc,	0xfbbf,	0xeb9e,	0x9b79,	0x8b58,	0xbb3b,	0xab1a,
	0x6ca6,	0x7c87,	0x4ce4,	0x5cc5,	0x2c22,	0x3c03,	0x0c60,	0x1c41,
	0xedae,	0xfd8f,	0xcdec,	0xddcd,	0xad2a,	0xbd0b,	0x8d68,	0x9d49,
	0x7e97,	0x6eb6,	0x5ed5,	0x4ef4,	0x3e13,	0x2e32,	0x1e51,	0x0e70,
	0xff9f,	0xefbe,	0xdfdd,	0xcffc,	0xbf1b,	0xaf3a,	0x9f59,	0x8f78,
	0x9188,	0x81a9,	0xb1ca,	0xa1eb,	0xd10c,	0xc12d,	0xf14e,	0xe16f,
	0x1080,	0x00a1,	0x30c2,	0x20e3,	0x5004,	0x4025,	0x7046,	0x6067,
	0x83b9,	0x9398,	0xa3fb,	0xb3da,	0xc33d,	0xd31c,	0xe37f,	0xf35e,
	0x02b1,	0x1290,	0x22f3,	0x32d2,	0x4235,	0x5214,	0x6277,	0x7256,
	0xb5ea,	0xa5cb,	0x95a8,	0x8589,	0xf56e,	0xe54f,	0xd52c,	0xc50d,
	0x34e2,	0x24c3,	0x14a0,	0x0481,	0x7466,	0x6447,	0x5424,	0x4405,
	0xa7db,	0xb7fa,	0x8799,	0x97b8,	0xe75f,	0xf77e,	0xc71d,	0xd73c,
	0x26d3,	0x36f2,	0x0691,	0x16b0,	0x6657,	0x7676,	0x4615,	0x5634,
	0xd94c,	0xc96d,	0xf90e,	0xe92f,	0x99c8,	0x89e9,	0xb98a,	0xa9ab,
	0x5844,	0x4865,	0x7806,	0x6827,	0x18c0,	0x08e1,	0x3882,	0x28a3,
	0xcb7d,	0xdb5c,	0xeb3f,	0xfb1e,	0x8bf9,	0x9bd8,	0xabbb,	0xbb9a,
	0x4a75,	0x5a54,	0x6a37,	0x7a16,	0x0af1,	0x1ad0,	0x2ab3,	0x3a92,
	0xfd2e,	0xed0f,	0xdd6c,	0xcd4d,	0xbdaa,	0xad8b,	0x9de8,	0x8dc9,
	0x7c26,	0x6c07,	0x5c64,	0x4c45,	0x3ca2,	0x2c83,	0x1ce0,	0x0cc1,
	0xef1f,	0xff3e,	0xcf5d,	0xdf7c,	0xaf9b,	0xbfba,	0x8fd9,	0x9ff8,
	0x6e17,	0x7e36,	0x4e55,	0x5e74,	0x2e93,	0x3eb2,	0x0ed1,	0x1ef0
};

void PR_CRC_Init(unsigned short *crcvalue)
{
	*crcvalue = CRC_INIT_VALUE;
}

void PR_CRC_ProcessByte(unsigned short *crcvalue, byte data)
{
	*crcvalue = (*crcvalue << 8) ^ PR_crctable[(*crcvalue >> 8) ^ data];
}

unsigned short PR_CRC_Value(unsigned short crcvalue)
{
	return crcvalue ^ CRC_XOR_VALUE;
}
//=============================================================================


#define PROGDEFS_MAX_SIZE 16384
//write (to file buf) and add to the crc
void __inline Add(char *p, unsigned short *crc, char *file)
{
	char *s;
	int i = strlen(file);
	if (i + strlen(p)+1 >= PROGDEFS_MAX_SIZE)
		return;
	for(s=p;*s;s++,i++)
	{
		PR_CRC_ProcessByte(crc, *s);
		file[i] = *s;
	}
	file[i]='\0';
}
#define ADD(p) Add(p, &crc, file)
//#define ADD(p) {char *s;int i = strlen(p);for(s=p;*s;s++,i++){PR_CRC_ProcessByte(&crc, *s);file[i] = *s;}file[i]='\0';}

void __inline Add3(char *p, unsigned short *crc, char *file)
{
	char *s;
	for(s=p;*s;s++)
		PR_CRC_ProcessByte(crc, *s);	
}
#define ADD3(p) Add3(p, &crc, file)

unsigned short PR_WriteProgdefs (char *filename)
{
	extern int ForcedCRC;
#define ADD2(p) strncat(file, p, PROGDEFS_MAX_SIZE-1 - strlen(file))	//no crc (later changes)
	char file[PROGDEFS_MAX_SIZE];
	def_t	*d;
	int	f;
	unsigned short		crc;
//	int		c;

	file[0] = '\0';

	PR_CRC_Init (&crc);
	
// print global vars until the first field is defined
	ADD("\n/* ");
	if (targetformat == QCF_HEXEN2)
		ADD3("generated by hcc, do not modify");
	else
		ADD3("file generated by qcc, do not modify");
	ADD2("File generated by FTEQCC, relevent for engine modding only, the generated crc must be the same as your engine expects.");
	ADD(" */\n\ntypedef struct");
	ADD2(" globalvars_s");
	ADD(va("\n{"));	
	ADD2("\tint pad;\n"
		"\tint ofs_return[3];\n"	//makes it easier with the get globals func
		"\tint ofs_parm0[3];\n"
		"\tint ofs_parm1[3];\n"
		"\tint ofs_parm2[3];\n"
		"\tint ofs_parm3[3];\n"
		"\tint ofs_parm4[3];\n"
		"\tint ofs_parm5[3];\n"
		"\tint ofs_parm6[3];\n"
		"\tint ofs_parm7[3];\n");
	ADD3(va("\tint\tpad[%i];\n", RESERVED_OFS));
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (!strcmp (d->name, "end_sys_globals"))
			break;
		if (d->ofs<RESERVED_OFS)
			continue;
			
		switch (d->type->type)
		{
		case ev_float:
			ADD(va("\tfloat\t%s;\n",d->name));
			break;
		case ev_vector:
			ADD(va("\tvec3_t\t%s;\n",d->name));
			d=d->next->next->next;	// skip the elements
			break;
		case ev_string:
			ADD(va("\tstring_t\t%s;\n",d->name));
			break;
		case ev_function:
			ADD(va("\tfunc_t\t%s;\n",d->name));
			break;
		case ev_entity:
			ADD(va("\tint\t%s;\n",d->name));
			break;
		case ev_integer:
			ADD(va("\tint\t%s;\n",d->name));
			break;
		default:
			ADD(va("\tint\t%s;\n",d->name));
			break;
		}
	}
	ADD("} globalvars_t;\n\n");

// print all fields
	ADD("typedef struct");
	ADD2(" entvars_s");
	ADD("\n{\n");
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (!strcmp (d->name, "end_sys_fields"))
			break;
			
		if (d->type->type != ev_field)
			continue;
			
		switch (d->type->aux_type->type)
		{
		case ev_float:
			ADD(va("\tfloat\t%s;\n",d->name));
			break;
		case ev_vector:
			ADD(va("\tvec3_t\t%s;\n",d->name));
			d=d->next->next->next;	// skip the elements
			break;
		case ev_string:
			ADD(va("\tstring_t\t%s;\n",d->name));
			break;
		case ev_function:
			ADD(va("\tfunc_t\t%s;\n",d->name));
			break;
		case ev_entity:
			ADD(va("\tint\t%s;\n",d->name));
			break;
		case ev_integer:
			ADD(va("\tint\t%s;\n",d->name));
			break;
		default:
			ADD(va("\tint\t%s;\n",d->name));
			break;
		}
	}
	ADD("} entvars_t;\n\n");

	///temp
	ADD2("//with this the crc isn't needed for fields.\nstruct fieldvars_s\n{\n\tint ofs;\n\tint type;\n\tchar *name;\n} fieldvars[] = {\n");
	f=0;
	for (d=pr.def_head.next ; d ; d=d->next)
	{
		if (!strcmp (d->name, "end_sys_fields"))
			break;

		if (d->type->type != ev_field)
			continue;
		if (f)
			ADD2(",\n");	
		ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
		f = 1;
	}
	ADD2("\n};\n\n");
	//end temp

	ADD2(va("#define PROGHEADER_CRC %i\n", crc));

	if (FS_CheckParm("-progdefs"))
	{
		Msg ("writing %s\n", filename);
		FS_WriteFile (filename, file, strlen(file));
	}


	if (ForcedCRC) crc = ForcedCRC;

	switch (crc)
	{
	case 54730:
		Msg("Recognised progs as QuakeWorld\n");
		break;
	case 5927:
		Msg("Recognised progs as regular Quake\n");
		break;
	case 38488:
		Msg("Recognised progs as original Hexen2\n");
		break;
	case 26905:
		Msg("Recognised progs as Hexen2 Mission Pack\n");
		break;
	}


	return crc;
}


/*void PR_PrintFunction (char *name)
{
	int		i;
	dstatement_t	*ds;
	dfunction_t		*df;
	
	for (i=0 ; i<numfunctions ; i++)
		if (!strcmp (name, strings + functions[i].s_name))
			break;
	if (i==numfunctions)
		Sys_Error ("No function named \"%s\"", name);
	df = functions + i;	
	
	Msg ("Statements for function %s:\n", name);
	ds = statements + df->first_statement;
	while (1)
	{
		PR_PrintStatement (ds);
		if (!ds->op)
			break;
		ds++;
	}
}*/
/*
void PR_PrintOfs(unsigned int ofs)
{
	int i;
	bool printfunc;
	dstatement_t	*ds;
	dfunction_t		*df;

	for (i=0 ; i<numfunctions ; i++)
	{
		df = functions + i;
		ds = statements + df->first_statement;
		printfunc = false;
		while (1)
		{
			if (!ds->op)
				break;
			if (ds->a == ofs || ds->b == ofs || ds->c == ofs)
			{
				PR_PrintStatement (ds);
				printfunc = true;
			}
			ds++;
		}
		if (printfunc)
		{
			PR_PrintFunction(strings + functions[i].s_name);
			Msg(" \n \n");
		}
	}

}
*/
/*
==============================================================================

DIRECTORY COPYING / PACKFILE CREATION

==============================================================================
*/

typedef struct
{
	char	name[56];
	int		filepos, filelen;
} packfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} packheader_t;

packfile_t	pfiles[4096], *pf;
int			packhandle;
int			packbytes;


/*
============
CreatePath
============
*/
void	PR_CreatePath (char *path)
{
	/*
	char	*ofs;
	
	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/')
		{	// create the directory
			*ofs = 0;
#ifdef QCC
			mkdir(path);
#else
			PR_mkdir (path);
#endif
			*ofs = '/';
		}
	}
	*/
}

/*
===========
CopyFile

Copies a file, creating any directories needed
===========
*/
void PR_CopyFile (char *src, char *dest)
{
	/*
	int		in, out;
	int		remaining, count;
	char	buf[4096];
	
	print ("%s to %s\n", src, dest);

	in = SafeOpenRead (src);
	remaining = filelength (in);
	
	PR_CreatePath (dest);
	out = VFS_OpenWrite (dest, remaining+10);
	
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		SafeRead (in, buf, count);
		VFS_Write (out, buf, count);
		remaining -= count;
	}

	close (in);
	VFS_Close (out);	
	*/
}


/*
===========
CopyFiles
===========
*/

void PR_CopyFiles (void)
{
	if (numsounds > 0) Msg ("%3i unique precache_sounds\n", numsounds);
	if (nummodels > 0) Msg ("%3i unique precache_models\n", nummodels);
	if (numtextures > 0) Msg ("%3i unique precache_textures\n", numtextures);
	if (numfiles > 0) Msg ("%3i unique precache_files\n", numfiles);
}

//============================================================================


void PR_CommandLinePrecompilerOptions (void)
{
	const_t *cnst;
	int             i, p;
	char *name, *val;

	for (i = 1;i<fs_argc;i++)
	{
		//compiler constant
		if ( !strncmp(fs_argv[i], "-D", 2) )
		{
			name = fs_argv[i] + 2;
			val = strchr(name, '=');
			if (val)
			{
				*val = '\0';
				val++;
			}
			cnst = PR_DefineName(name);
			if (val)
			{
				if (strlen(val)+1 >= sizeof(cnst->value))
					Sys_Error("Compiler constant value is too long\n");
				strncpy(cnst->value, val, sizeof(cnst->value)-1);
				cnst->value[sizeof(cnst->value)-1] = '\0';
			}
		}

		//optimisations.
		else if ( !strnicmp(fs_argv[i], "-O", 2) || !strnicmp(fs_argv[i], "/O", 2) )
		{
			p = 0;
			if (fs_argv[i][2] >= '0' && fs_argv[i][2] <= '3')
			{
			}
			else if (!strnicmp(fs_argv[i]+2, "no-", 3))
			{
				if (fs_argv[i][5])
				{
					for (p = 0; optimisations[p].enabled; p++)
					{
						if ((*optimisations[p].abbrev && !stricmp(fs_argv[i]+5, optimisations[p].abbrev)) || !stricmp(fs_argv[i]+5, optimisations[p].fullname))
						{
							*optimisations[p].enabled = false;
							break;
						}
					}
				}
			}
			else
			{
				if (fs_argv[i][2])
					for (p = 0; optimisations[p].enabled; p++)
						if ((*optimisations[p].abbrev && !stricmp(fs_argv[i]+2, optimisations[p].abbrev)) || !stricmp(fs_argv[i]+2, optimisations[p].fullname))
						{
							*optimisations[p].enabled = true;
							break;
						}
			}
			if (!optimisations[p].enabled)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised optimisation parameter (%s)", fs_argv[i]);
		}
		
		else if ( !strnicmp(fs_argv[i], "-K", 2) || !strnicmp(fs_argv[i], "/K", 2) )
		{
			p = 0;
			if (!strnicmp(fs_argv[i]+2, "no-", 3))
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+5, compiler_flag[p].abbrev))
					{
						*compiler_flag[p].enabled = false;
						break;
					}
			}
			else
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+2, compiler_flag[p].abbrev))
					{
						*compiler_flag[p].enabled = true;
						break;
					}
			}

			if (!compiler_flag[p].enabled)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised keyword parameter (%s)", fs_argv[i]);
		}
		else if ( !strnicmp(fs_argv[i], "-F", 2) || !strnicmp(fs_argv[i], "/F", 2) )
		{
			p = 0;
			if (!strnicmp(fs_argv[i]+2, "no-", 3))
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+5, compiler_flag[p].abbrev))
					{
						*compiler_flag[p].enabled = false;
						break;
					}
			}
			else
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+2, compiler_flag[p].abbrev))
					{
						*compiler_flag[p].enabled = true;
						break;
					}
			}

			if (!compiler_flag[p].enabled)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised flag parameter (%s)", fs_argv[i]);
		}


		else if ( !strncmp(fs_argv[i], "-T", 2) || !strncmp(fs_argv[i], "/T", 2) )
		{
			p = 0;
			for (p = 0; targets[p].name; p++)
				if (!stricmp(fs_argv[i]+2, targets[p].name))
				{
					targetformat = targets[p].target;
					break;
				}

			if (!targets[p].name)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised target parameter (%s)", fs_argv[i]);
		}

		else if ( !strnicmp(fs_argv[i], "-W", 2) || !strnicmp(fs_argv[i], "/W", 2) )
		{
			if (!stricmp(fs_argv[i]+2, "all"))
				memset(pr_warning, 0, sizeof(pr_warning));
			else if (!stricmp(fs_argv[i]+2, "none"))
				memset(pr_warning, 1, sizeof(pr_warning));
			else if (!stricmp(fs_argv[i]+2, "no-mundane"))
			{	//disable mundane performance/efficiency/blah warnings that don't affect code.
				pr_warning[WARN_SAMENAMEASGLOBAL] = true;
				pr_warning[WARN_DUPLICATEDEFINITION] = true;
				pr_warning[WARN_CONSTANTCOMPARISON] = true;
				pr_warning[WARN_ASSIGNMENTINCONDITIONAL] = true;
				pr_warning[WARN_DEADCODE] = true;
				pr_warning[WARN_NOTREFERENCEDCONST] = true;
				pr_warning[WARN_NOTREFERENCED] = true;
				pr_warning[WARN_POINTLESSSTATEMENT] = true;
				pr_warning[WARN_ASSIGNMENTTOCONSTANTFUNC] = true;
				pr_warning[WARN_BADPRAGMA] = true;	//C specs say that these should be ignored. We're close enough to C that I consider that a valid statement.
				pr_warning[WARN_IDENTICALPRECOMPILER] = true;
				pr_warning[WARN_UNDEFNOTDEFINED] = true;
			}
			else
			{
				p = 0;
				if (!strnicmp(fs_argv[i]+2, "no-", 3))
				{
					for (p = 0; warningnames[p].name; p++)
						if (!strcmp(fs_argv[i]+5, warningnames[p].name))
						{
							pr_warning[warningnames[p].index] = true;
							break;
						}
				}
				else
				{
					for (p = 0; warningnames[p].name; p++)
						if (!stricmp(fs_argv[i]+2, warningnames[p].name))
						{
							pr_warning[warningnames[p].index] = false;
							break;
						}
				}

				if (!warningnames[p].name)
					PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised warning parameter (%s)", fs_argv[i]);
			}
		}
	}
}

/*
============
main
============
*/

char	*qccmsrc;
char	*qccmsrc2;
char	qccmfilename[1024];
char	qccmprogsdat[1024];
char	qccmsourcedir[1024];

void PR_FinishCompile(void);


void PR_SetDefaultProperties (void)
{
	extern int ForcedCRC;
	int i;

	Hash_InitTable(&compconstantstable, MAX_CONSTANTS, Qalloc(BytesForBuckets(MAX_CONSTANTS)));

	ForcedCRC = 0;

	PR_DefineName("FTEQCC");

	if (FS_CheckParm("/Oz"))
	{
		targetformat = QCF_FTE;
		PR_DefineName("OP_COMP_STATEMENTS");
		PR_DefineName("OP_COMP_DEFS");
		PR_DefineName("OP_COMP_FIELDS");
		PR_DefineName("OP_COMP_FUNCTIONS");
		PR_DefineName("OP_COMP_STRINGS");
		PR_DefineName("OP_COMP_GLOBALS");
		PR_DefineName("OP_COMP_LINES");
		PR_DefineName("OP_COMP_TYPES");
	}

	if (FS_CheckParm("/O0") || FS_CheckParm("-O0"))
		level = 0;
	else if (FS_CheckParm("/O1") || FS_CheckParm("-O1"))
		level = 1;
	else if (FS_CheckParm("/O2") || FS_CheckParm("-O2"))
		level = 2;
	else if (FS_CheckParm("/O3") || FS_CheckParm("-O3"))
		level = 3;
	else
		level = -1;

	if (level == -1)
	{
		for (i = 0; optimisations[i].enabled; i++)
		{
			if (optimisations[i].flags & FLAG_ASDEFAULT)
				*optimisations[i].enabled = true;
			else
				*optimisations[i].enabled = false;
		}
	}
	else
	{
		for (i = 0; optimisations[i].enabled; i++)
		{
			if (level >= optimisations[i].optimisationlevel)
				*optimisations[i].enabled = true;
			else
				*optimisations[i].enabled = false;
		}
	}

	targetformat = QCF_STANDARD;


	//enable all warnings
	memset(pr_warning, 0, sizeof(pr_warning));

	//play with default warnings.
	pr_warning[WARN_NOTREFERENCEDCONST] = true;
	pr_warning[WARN_MACROINSTRING] = true;
//	pr_warning[WARN_ASSIGNMENTTOCONSTANT] = true;
	pr_warning[WARN_FIXEDRETURNVALUECONFLICT] = true;
	pr_warning[WARN_EXTRAPRECACHE] = true;
	pr_warning[WARN_DEADCODE] = true;
	pr_warning[WARN_INEFFICIENTPLUSPLUS] = true;
	pr_warning[WARN_FTE_SPECIFIC] = true;
	pr_warning[WARN_EXTENSION_USED] = true;
	pr_warning[WARN_IFSTRING_USED] = true;

	//Check the command line
	PR_CommandLinePrecompilerOptions();


	if (targetformat == QCF_HEXEN2)	//force on the thinktime keyword if hexen2 progs.
		keyword_thinktime = true;

	if (FS_CheckParm("-debug"))	//disable any debug optimisations
	{
		for (i = 0; optimisations[i].enabled; i++)
		{
			if (optimisations[i].flags & FLAG_KILLSDEBUGGERS)
				*optimisations[i].enabled = false;
		}
	}
}

int qcc_compileactive = false;
extern int accglobalsblock;
char *originalqccmsrc;	//for autoprototype.
void PR_main ( void ) //as part of the quake engine
{
	extern int	pr_bracelevel;

	int		p;

	qcc_compileactive = true;

	pHash_Get = &Hash_Get;
	pHash_GetNext = &Hash_GetNext;
	pHash_Add = &Hash_Add;

	// tune limits
	MAX_REGS		= 65536;
	MAX_STRINGS	= 1000000;
	MAX_GLOBALS	= 32768;
	MAX_FIELDS	= 2048;
	MAX_STATEMENTS	= 0x80000;
	MAX_FUNCTIONS	= 16384;
	maxtypeinfos	= 16384;
	MAX_CONSTANTS	= 2048;

	compressoutput = 0;
	strcpy(v_copyright, "This file was created with ForeThought's modified QuakeC compiler\nThanks to ID Software");

	for (p = 0; compiler_flag[p].enabled; p++)
	{
		*compiler_flag[p].enabled = compiler_flag[p].flags & FLAG_ASDEFAULT;
	}

	PR_SetDefaultProperties();

	optres_shortenifnots = 0;
	optres_overlaptemps = 0;
	optres_noduplicatestrings = 0;
	optres_constantarithmatic = 0;
	optres_nonvec_parms = 0;
	optres_constant_names = 0;
	optres_constant_names_strings = 0;
	optres_precache_file = 0;
	optres_filenames = 0;
	optres_assignments = 0;
	optres_unreferenced = 0;
	optres_function_names = 0;
	optres_locals = 0;
	optres_dupconstdefs = 0;
	optres_return_only = 0;
	optres_compound_jumps = 0;
//	optres_comexprremoval = 0;
	optres_stripfunctions = 0;
	optres_locals_marshalling = 0;
	optres_logicops = 0;

	optres_test1 = 0;
	optres_test2 = 0;

	accglobalsblock = 0;
          
	
	numtemps = 0;
	functemps=NULL;

	strings = (void *)Qalloc(sizeof(char) * MAX_STRINGS);
	strofs = 1;

	statements = (void *)Qalloc(sizeof(dstatement_t) * MAX_STATEMENTS);
	numstatements = 0;
	statement_linenums = (void *)Qalloc(sizeof(int) * MAX_STATEMENTS);

	functions = (void *)Qalloc(sizeof(dfunction_t) * MAX_FUNCTIONS);
	numfunctions=0;

	pr_bracelevel = 0;

	pr_globals = (void *)Qalloc(sizeof(float) * MAX_REGS);
	numpr_globals=0;

	Hash_InitTable(&globalstable, MAX_REGS, Qalloc(BytesForBuckets(MAX_REGS)));
	Hash_InitTable(&localstable, MAX_REGS, Qalloc(BytesForBuckets(MAX_REGS)));
	Hash_InitTable(&floatconstdefstable, MAX_REGS+1, Qalloc(BytesForBuckets(MAX_REGS+1)));
	Hash_InitTable(&stringconstdefstable, MAX_REGS, Qalloc(BytesForBuckets(MAX_REGS)));
	
//	pr_global_defs = (def_t **)Qalloc(sizeof(def_t *) * MAX_REGS);

	qcc_globals = (void *)Qalloc(sizeof(ddef_t) * MAX_GLOBALS);
	numglobaldefs=0;	

	fields = (void *)Qalloc(sizeof(ddef_t) * MAX_FIELDS);
	numfielddefs=0;

	memset(pr_immediate_string, 0, sizeof(pr_immediate_string));

	precache_sounds = (void *)Qalloc(sizeof(char)*MAX_NAME*MAX_SOUNDS);
	numsounds = 0;

	precache_textures = (void *)Qalloc(sizeof(char)*MAX_NAME*MAX_TEXTURES);
	numtextures=0;

	precache_models = (void *)Qalloc(sizeof(char)*MAX_NAME*MAX_MODELS);
	nummodels=0;

	precache_files = (void *)Qalloc(sizeof(char)*MAX_NAME*MAX_FILES);
	numfiles = 0;

	qcc_typeinfo = (void *)Qalloc(sizeof(type_t)*maxtypeinfos);
	numtypeinfos = 0;

	qcc_tempofs = Qalloc(sizeof(int) * max_temps);
	tempsstart = 0;

	bodylessfuncs=0;

	memset(&pr, 0, sizeof(pr));
#ifdef MAX_EXTRA_PARMS
	memset(&extra_parms, 0, sizeof(extra_parms));
#endif

	if (opt_locals_marshalling)
		Msg("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\nLocals marshalling might be buggy. Use with caution\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	
	if(FS_GetParmFromCmdLine("-src", qccmsourcedir ))
	{
		strcat (qccmsourcedir, "/");
		Msg ("Source directory: %s\n", qccmsourcedir);
	}
	else *qccmsourcedir = '\0';
          
	PR_InitData ();
	PR_BeginCompilation (Qalloc (0x100000), 0x100000);

	sprintf (qccmprogsdat, "%sprogs.src", qccmsourcedir);
	qccmsrc = QCC_LoadFile (qccmprogsdat);

	if (writeasm)
	{
		asmfile = FS_Open("qc.asm", "wb" );
		if (!asmfile) Sys_Error ("Couldn't open file for asm output.");
	}

	while(*qccmsrc && *qccmsrc < ' ')
		qccmsrc++;

	pr_file_p = SC_ParseToken(&qccmsrc);
	strcpy (destfile, token);
	FS_StripExtension( token );

	// msvc6.0 style message
	Msg("--------------------Configuration: %s - Vm16 %s--------------------\n", token, level <= 0 ? "Debug" : "Release" ); 
	
	pr_dumpasm = false;
	currentchunk = NULL;
	originalqccmsrc = qccmsrc;
}

// called between exe frames - won't loose net connection (is the theory)...
void PR_ContinueCompile(void)
{
	char *s, *s2;
	currentchunk = NULL;
	if (!qcc_compileactive) return;

	SC_ParseToken(&qccmsrc);

	if (!qccmsrc)
	{
		if (autoprototype)
		{
			qccmsrc = originalqccmsrc;
			PR_SetDefaultProperties();
			autoprototype = false;
			Msg("Compiling...\n");
			return;
		}
		PR_FinishCompile();
		return;
	}
	s = token;

	strcpy (qccmfilename, qccmsourcedir);
	while(1)
	{
		if (!strncmp(s, "..\\", 3))
		{
			s2 = qccmfilename + strlen(qccmfilename)-2;
			while (s2>=qccmfilename)
			{
				if (*s2 == '/' || *s2 == '\\')
				{
					s2[1] = '\0';
					break;
				}
				s2--;
			}
			s+=3;
			continue;
		}
		if (!strncmp(s, ".\\", 2))
		{
			s+=2;
			continue;
		}

		break;
	}
	strcat (qccmfilename, s);
	Msg ("%s\n", qccmfilename);

	qccmsrc2 = QCC_LoadFile (qccmfilename);
	if(!PR_CompileFile (qccmsrc2, qccmfilename))
	{
		Msg("error count exceeds  %i; stopping compilation\n", MAX_ERRORS);
		Sys_Error("%s - %i error(s), %i warning(s)\n", destfile, pr_error_count, pr_warning_count);
	}
}

void PR_FinishCompile(void)
{	
	int	crc;
	currentchunk = NULL;

	if (setjmp(pr_parse_abort)) Sys_Error(""); // freeze console
	if (!PR_FinishCompilation()) Sys_Error ("Compilation errors");
	
	// write progdefs.h
	crc = PR_WriteProgdefs ("progdefs.h");
	
	// write data file
	PR_WriteData (crc);
	
	// report / copy the data files
	PR_CopyFiles ();

	if (optres_shortenifnots) Msg("optres_shortenifnots %i\n", optres_shortenifnots);
	if (optres_overlaptemps) Msg("optres_overlaptemps %i\n", optres_overlaptemps);
	if (optres_noduplicatestrings) Msg("optres_noduplicatestrings %i\n", optres_noduplicatestrings);
	if (optres_constantarithmatic) Msg("optres_constantarithmatic %i\n", optres_constantarithmatic);
	if (optres_nonvec_parms) Msg("optres_nonvec_parms %i\n", optres_nonvec_parms);
	if (optres_constant_names) Msg("optres_constant_names %i\n", optres_constant_names);
	if (optres_constant_names_strings) Msg("optres_constant_names_strings %i\n", optres_constant_names_strings);
	if (optres_precache_file) Msg("optres_precache_file %i\n", optres_precache_file);
	if (optres_filenames) Msg("optres_filenames %i\n", optres_filenames);
	if (optres_assignments) Msg("optres_assignments %i\n", optres_assignments);
	if (optres_unreferenced) Msg("optres_unreferenced %i\n", optres_unreferenced);
	if (optres_locals) Msg("optres_locals %i\n", optres_locals);
	if (optres_function_names) Msg("optres_function_names %i\n", optres_function_names);
	if (optres_dupconstdefs) Msg("optres_dupconstdefs %i\n", optres_dupconstdefs);
	if (optres_return_only) Msg("optres_return_only %i\n", optres_return_only);
	if (optres_compound_jumps) Msg("optres_compound_jumps %i\n", optres_compound_jumps);
//	if (optres_comexprremoval) Msg("optres_comexprremoval %i\n", optres_comexprremoval);
	if (optres_stripfunctions) Msg("optres_stripfunctions %i\n", optres_stripfunctions);
	if (optres_locals_marshalling) Msg("optres_locals_marshalling %i\n", optres_locals_marshalling);
	if (optres_logicops) Msg("optres_logicops %i\n", optres_logicops);


	if (optres_test1) Msg("optres_test1 %i\n", optres_test1);
	if (optres_test2) Msg("optres_test2 %i\n", optres_test2);
	
	Msg ("�����஢��� 䠩���:         1.\n\n");// enigma from M$ :)
	Msg("%s - %i error(s), %i warning(s)\n", destfile, pr_error_count, pr_warning_count);

	qcc_compileactive = false;
}

bool PrepareDATProgs ( const char *dir, const char *name, byte params )
{
	qccpool = Mem_AllocPool( "QCC Compiler" );

	FS_InitRootDir(".");
	PR_ResetErrorScope();

	return true;
}

bool CompileDATProgs ( void )
{
	PR_main();

	if (autoprototype) Msg ("Prototyping...\n");
	else Msg("Compiling...\n");
	while(qcc_compileactive)
		PR_ContinueCompile();

	if (asmfile) FS_Close(asmfile);
          
	Mem_FreePool( &qccpool );
	return true;
}