/* vi: set sw=4 ts=4: */
/* i386 ELF shared library loader suppport
 *
 * Copyright (c) 1994-2000 Eric Youngdale, Peter MacDonald, 
 *				David Engel, Hongjiu Lu and Mitch D'Souza
 * Copyright (C) 2001-2002, Erik Andersen
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the above contributors may not be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined (__SUPPORT_LD_DEBUG__)
static const char *_dl_reltypes_tab[] =
{
  [0]	"R_386_NONE",	    "R_386_32",	    "R_386_PC32",	"R_386_GOT32",
  [4]	"R_386_PLT32",	    "R_386_COPY",   "R_386_GLOB_DAT",	"R_386_JMP_SLOT",
  [8]	"R_386_RELATIVE",   "R_386_GOTOFF", "R_386_GOTPC",
};

static const char *
_dl_reltypes(int type)
{
  static char buf[22];  
  const char *str;
  
  if (type >= (int)(sizeof (_dl_reltypes_tab)/sizeof(_dl_reltypes_tab[0])) ||
      NULL == (str = _dl_reltypes_tab[type]))
  {
    str =_dl_simple_ltoa( buf, (unsigned long)(type));
  }
  return str;
}

static 
void debug_sym(Elf32_Sym *symtab,char *strtab,int symtab_index)
{
  if(_dl_debug_symbols)
  {
    if(symtab_index){
      _dl_dprintf(_dl_debug_file, "\n%s\n\tvalue=%x\tsize=%x\tinfo=%x\tother=%x\tshndx=%x",
		  strtab + symtab[symtab_index].st_name,
		  symtab[symtab_index].st_value,
		  symtab[symtab_index].st_size,
		  symtab[symtab_index].st_info,
		  symtab[symtab_index].st_other,
		  symtab[symtab_index].st_shndx);
    }
  }
}

static void debug_reloc(Elf32_Sym *symtab,char *strtab, ELF_RELOC *rpnt)
{
  if(_dl_debug_reloc)
  {
    int symtab_index;
    const char *sym;
    symtab_index = ELF32_R_SYM(rpnt->r_info);
    sym = symtab_index ? strtab + symtab[symtab_index].st_name : "sym=0x0";
    
  if(_dl_debug_symbols)
	  _dl_dprintf(_dl_debug_file, "\n\t");
  else
	  _dl_dprintf(_dl_debug_file, "\n%s\n\t", sym);
#ifdef ELF_USES_RELOCA
    _dl_dprintf(_dl_debug_file, "%s\toffset=%x\taddend=%x",
		_dl_reltypes(ELF32_R_TYPE(rpnt->r_info)),
		rpnt->r_offset,
		rpnt->r_addend);
#else
    _dl_dprintf(_dl_debug_file, "%s\toffset=%x\n",
		_dl_reltypes(ELF32_R_TYPE(rpnt->r_info)),
		rpnt->r_offset);
#endif
  }
}
#endif

/* Program to load an ELF binary on a linux system, and run it.
   References to symbols in sharable libraries can be resolved by either
   an ELF sharable library or a linux style of shared library. */

/* Disclaimer:  I have never seen any AT&T source code for SVr4, nor have
   I ever taken any courses on internals.  This program was developed using
   information available through the book "UNIX SYSTEM V RELEASE 4,
   Programmers guide: Ansi C and Programming Support Tools", which did
   a more than adequate job of explaining everything required to get this
   working. */

extern int _dl_linux_resolve(void);

unsigned long _dl_linux_resolver(struct elf_resolve *tpnt, int reloc_entry)
{
	int reloc_type;
	ELF_RELOC *this_reloc;
	char *strtab;
	Elf32_Sym *symtab;
	ELF_RELOC *rel_addr;
	int symtab_index;
	char *new_addr;
	char **got_addr;
	unsigned long instr_addr;
	char *symname;

	rel_addr = (ELF_RELOC *) (tpnt->dynamic_info[DT_JMPREL] + tpnt->loadaddr);
	this_reloc = (ELF_RELOC *)(intptr_t)(rel_addr + reloc_entry);
	reloc_type = ELF32_R_TYPE(this_reloc->r_info);
	symtab_index = ELF32_R_SYM(this_reloc->r_info);

	symtab = (Elf32_Sym *)(intptr_t) (tpnt->dynamic_info[DT_SYMTAB] + tpnt->loadaddr);
	strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadaddr);
	symname= strtab + symtab[symtab_index].st_name;

	if (reloc_type != R_386_JMP_SLOT) {
		_dl_dprintf(2, "%s: Incorrect relocation type in jump relocations\n", 
				_dl_progname);
		_dl_exit(1);
	}

	/* Address of jump instruction to fix up */
	instr_addr = ((unsigned long) this_reloc->r_offset + 
			(unsigned long) tpnt->loadaddr);
	got_addr = (char **) instr_addr;

	/* Get the address of the GOT entry */
	new_addr = _dl_find_hash(symname, tpnt->symbol_scope, tpnt, resolver);
	if (!new_addr) {
		new_addr = _dl_find_hash(symname, NULL, NULL, resolver);
		if (new_addr) {
			return (unsigned long) new_addr;
		}
		_dl_dprintf(2, "%s: can't resolve symbol '%s'\n", _dl_progname, symname);
		_dl_exit(1);
	}

#if defined (__SUPPORT_LD_DEBUG__)
	if ((unsigned long) got_addr < 0x40000000)
	{
		if (_dl_debug_bindings)
		{
			_dl_dprintf(_dl_debug_file, "\nresolve function: %s", symname);
			if(_dl_debug_detail) _dl_dprintf(_dl_debug_file, 
					"\n\tpatched %x ==> %x @ %x\n", *got_addr, new_addr, got_addr);
		}
	}
	if (!_dl_debug_nofixups) {
		*got_addr = new_addr;
	}
#else
	*got_addr = new_addr;
#endif

	return (unsigned long) new_addr;
}

static int
_dl_parse(struct elf_resolve *tpnt, struct dyn_elf *scope,
	  unsigned long rel_addr, unsigned long rel_size,
	  int (*reloc_fnc) (struct elf_resolve *tpnt, struct dyn_elf *scope,
			    ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab))
{
	unsigned int i;
	char *strtab;
	Elf32_Sym *symtab;
	ELF_RELOC *rpnt;
	int symtab_index;

	/* Now parse the relocation information */
	rpnt = (ELF_RELOC *)(intptr_t) (rel_addr + tpnt->loadaddr);
	rel_size = rel_size / sizeof(ELF_RELOC);

	symtab = (Elf32_Sym *)(intptr_t) (tpnt->dynamic_info[DT_SYMTAB] + tpnt->loadaddr);
	strtab = (char *) (tpnt->dynamic_info[DT_STRTAB] + tpnt->loadaddr);

	  for (i = 0; i < rel_size; i++, rpnt++) {
	        int res;
	    
		symtab_index = ELF32_R_SYM(rpnt->r_info);
		
		/* When the dynamic linker bootstrapped itself, it resolved some symbols.
		   Make sure we do not do them again */
		if (!symtab_index && tpnt->libtype == program_interpreter)
			continue;
		if (symtab_index && tpnt->libtype == program_interpreter &&
		    _dl_symbol(strtab + symtab[symtab_index].st_name))
			continue;

#if defined (__SUPPORT_LD_DEBUG__)
		debug_sym(symtab,strtab,symtab_index);
		debug_reloc(symtab,strtab,rpnt);
#endif

		res = reloc_fnc (tpnt, scope, rpnt, symtab, strtab);

		if (res==0) continue;

		_dl_dprintf(2, "\n%s: ",_dl_progname);
		
		if (symtab_index)
		  _dl_dprintf(2, "symbol '%s': ", strtab + symtab[symtab_index].st_name);
		  
		if (res <0)
		{
		        int reloc_type = ELF32_R_TYPE(rpnt->r_info);
#if defined (__SUPPORT_LD_DEBUG__)
			_dl_dprintf(2, "can't handle reloc type %s\n ", _dl_reltypes(reloc_type));
#else
			_dl_dprintf(2, "can't handle reloc type %x\n", reloc_type);
#endif			
			_dl_exit(-res);
		}
		else if (res >0)
		{
			_dl_dprintf(2, "can't resolve symbol\n");
			return res;
		}
	  }
	  return 0;
}


static int
_dl_do_reloc (struct elf_resolve *tpnt,struct dyn_elf *scope,
	      ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab)
{
	int reloc_type;
	int symtab_index;
	char *symname;
	unsigned long *reloc_addr;
	unsigned long symbol_addr;
#if defined (__SUPPORT_LD_DEBUG__)
	unsigned long old_val;
#endif

	reloc_addr   = (unsigned long *)(intptr_t) (tpnt->loadaddr + (unsigned long) rpnt->r_offset);
	reloc_type   = ELF32_R_TYPE(rpnt->r_info);
	symtab_index = ELF32_R_SYM(rpnt->r_info);
	symbol_addr  = 0;
	symname      = strtab + symtab[symtab_index].st_name;

	if (symtab_index) {

		symbol_addr = (unsigned long) _dl_find_hash(symname, scope, 
				(reloc_type == R_386_JMP_SLOT ? tpnt : NULL), symbolrel);

		/*
		 * We want to allow undefined references to weak symbols - this might
		 * have been intentional.  We should not be linking local symbols
		 * here, so all bases should be covered.
		 */

		if (!symbol_addr && ELF32_ST_BIND(symtab[symtab_index].st_info) == STB_GLOBAL) {
#if defined (__SUPPORT_LD_DEBUG__)
			_dl_dprintf(2, "\tglobal symbol '%s' already defined in '%s'\n",
					symname, tpnt->libname);
#endif
			return 0;
		}
	}

#if defined (__SUPPORT_LD_DEBUG__)
	old_val = *reloc_addr;
#endif
		switch (reloc_type) {
			case R_386_NONE:
				break;
			case R_386_32:
				*reloc_addr += symbol_addr;
				break;
			case R_386_PC32:
				*reloc_addr += symbol_addr - (unsigned long) reloc_addr;
				break;
			case R_386_GLOB_DAT:
			case R_386_JMP_SLOT:
				*reloc_addr = symbol_addr;
				break;
			case R_386_RELATIVE:
				*reloc_addr += (unsigned long) tpnt->loadaddr;
				break;
			case R_386_COPY:
				/* handled later on */
				break;
			default:
				return -1; /*call _dl_exit(1) */
		}
#if defined (__SUPPORT_LD_DEBUG__)
	if(_dl_debug_reloc && _dl_debug_detail)
		_dl_dprintf(_dl_debug_file, "\tpatched: %x ==> %x @ %x", old_val, *reloc_addr, reloc_addr);
#endif

	return 0;
}

static int
_dl_do_lazy_reloc (struct elf_resolve *tpnt, struct dyn_elf *scope,
		   ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab)
{
	int reloc_type;
	unsigned long *reloc_addr;
#if defined (__SUPPORT_LD_DEBUG__)
	unsigned long old_val;
#endif
	(void)scope;
	(void)symtab;
	(void)strtab;

	reloc_addr = (unsigned long *)(intptr_t) (tpnt->loadaddr + (unsigned long) rpnt->r_offset);
	reloc_type = ELF32_R_TYPE(rpnt->r_info);

#if defined (__SUPPORT_LD_DEBUG__)
	old_val = *reloc_addr;
#endif
		switch (reloc_type) {
			case R_386_NONE:
				break;
			case R_386_JMP_SLOT:
				*reloc_addr += (unsigned long) tpnt->loadaddr;
				break;
			default:
				return -1; /*call _dl_exit(1) */
		}
#if defined (__SUPPORT_LD_DEBUG__)
	if(_dl_debug_reloc && _dl_debug_detail)
		_dl_dprintf(_dl_debug_file, "\tpatched: %x ==> %x @ %x", old_val, *reloc_addr, reloc_addr);
#endif
	return 0;

}

/* This is done as a separate step, because there are cases where
   information is first copied and later initialized.  This results in
   the wrong information being copied.  Someone at Sun was complaining about
   a bug in the handling of _COPY by SVr4, and this may in fact be what he
   was talking about.  Sigh. */

/* No, there are cases where the SVr4 linker fails to emit COPY relocs
   at all */
static int
_dl_do_copy (struct elf_resolve *tpnt, struct dyn_elf *scope,
	     ELF_RELOC *rpnt, Elf32_Sym *symtab, char *strtab)
{
	int reloc_type;
	int symtab_index;
	unsigned long *reloc_addr;
	unsigned long symbol_addr;
	int goof = 0;
	char *symname;
	  
	reloc_addr = (unsigned long *)(intptr_t) (tpnt->loadaddr + (unsigned long) rpnt->r_offset);
	reloc_type = ELF32_R_TYPE(rpnt->r_info);
	if (reloc_type != R_386_COPY) 
		return 0;
	symtab_index = ELF32_R_SYM(rpnt->r_info);
	symbol_addr = 0;
	symname      = strtab + symtab[symtab_index].st_name;
		
	if (symtab_index) {
		symbol_addr = (unsigned long) _dl_find_hash(symname, scope, NULL, copyrel);
		if (!symbol_addr) goof++;
	}
	if (!goof) {
#if defined (__SUPPORT_LD_DEBUG__)
	        if(_dl_debug_move)
		  _dl_dprintf(_dl_debug_file,"\n%s move %x bytes from %x to %x",
			     symname, symtab[symtab_index].st_size,
			     symbol_addr, symtab[symtab_index].st_value);
#endif
		_dl_memcpy((char *) symtab[symtab_index].st_value, 
			(char *) symbol_addr, symtab[symtab_index].st_size);
	}

	return goof;
}

void _dl_parse_lazy_relocation_information(struct elf_resolve *tpnt, 
	unsigned long rel_addr, unsigned long rel_size, int type)
{
	(void) type;
	(void)_dl_parse(tpnt, NULL, rel_addr, rel_size, _dl_do_lazy_reloc);
}

int _dl_parse_relocation_information(struct elf_resolve *tpnt, 
	unsigned long rel_addr, unsigned long rel_size, int type)
{
	(void) type;
	return _dl_parse(tpnt, tpnt->symbol_scope, rel_addr, rel_size, _dl_do_reloc);
}

int _dl_parse_copy_information(struct dyn_elf *xpnt, unsigned long rel_addr, 
	unsigned long rel_size, int type)
{
	(void) type;
	return _dl_parse(xpnt->dyn, xpnt->next, rel_addr, rel_size, _dl_do_copy);
}

