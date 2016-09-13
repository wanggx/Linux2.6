#ifndef _LINUX_MODULE_H
#define _LINUX_MODULE_H
/*
 * Dynamic loading of modules into the kernel.
 *
 * Rewritten by Richard Henderson <rth@tamu.edu> Dec 1996
 * Rewritten again by Rusty Russell, 2002
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/stat.h>
#include <linux/compiler.h>
#include <linux/cache.h>
#include <linux/kmod.h>
#include <linux/elf.h>
#include <linux/stringify.h>
#include <linux/kobject.h>
#include <linux/moduleparam.h>
#include <asm/local.h>

#include <asm/module.h>

/* Not Yet Implemented */
#define MODULE_SUPPORTED_DEVICE(name)

/* v850 toolchain uses a `_' prefix for all user symbols */
#ifndef MODULE_SYMBOL_PREFIX
#define MODULE_SYMBOL_PREFIX ""
#endif

#define MODULE_NAME_LEN (64 - sizeof(unsigned long))

/* 内核导出符号结构 */
struct kernel_symbol
{
	unsigned long value;			/* 符号在内存中的地址 */
	const char *name;			/* 该符号的名称 */
};

struct modversion_info
{
	unsigned long crc;
	char name[MODULE_NAME_LEN];
};

struct module;

struct module_attribute {
        struct attribute attr;
        ssize_t (*show)(struct module_attribute *, struct module *, char *);
        ssize_t (*store)(struct module_attribute *, struct module *,
			 const char *, size_t count);
};

/* 模块的内核对象结构，一般会以/sys/modules目录的形式变现出来 */
struct module_kobject
{
	struct kobject kobj;
	struct module *mod;
};

/* These are either module local, or the kernel's dummy ones. */
/* 内核的初始化和退出函数，要么本地模块提供，要么使用系统自带 */
extern int init_module(void);
extern void cleanup_module(void);

/* Archs provide a method of finding the correct exception table. */
struct exception_table_entry;

const struct exception_table_entry *
search_extable(const struct exception_table_entry *first,
	       const struct exception_table_entry *last,
	       unsigned long value);
void sort_extable(struct exception_table_entry *start,
		  struct exception_table_entry *finish);
void sort_main_extable(void);

extern struct subsystem module_subsys;

#ifdef MODULE
#define ___module_cat(a,b) __mod_ ## a ## b
#define __module_cat(a,b) ___module_cat(a,b)
#define __MODULE_INFO(tag, name, info)					  \
static const char __module_cat(name,__LINE__)[]				  \
  __attribute_used__							  \
  __attribute__((section(".modinfo"),unused)) = __stringify(tag) "=" info

#define MODULE_GENERIC_TABLE(gtype,name)			\
extern const struct gtype##_id __mod_##gtype##_table		\
  __attribute__ ((unused, alias(__stringify(name))))

extern struct module __this_module;
#define THIS_MODULE (&__this_module)

#else  /* !MODULE */

#define MODULE_GENERIC_TABLE(gtype,name)
#define __MODULE_INFO(tag, name, info)
#define THIS_MODULE ((struct module *)0)
#endif

/* Generic info of form tag = "info" */
#define MODULE_INFO(tag, info) __MODULE_INFO(tag, tag, info)

/* For userspace: you can also call me... */
#define MODULE_ALIAS(_alias) MODULE_INFO(alias, _alias)

/*
 * The following license idents are currently accepted as indicating free
 * software modules
 *
 *	"GPL"				[GNU Public License v2 or later]
 *	"GPL v2"			[GNU Public License v2]
 *	"GPL and additional rights"	[GNU Public License v2 rights and more]
 *	"Dual BSD/GPL"			[GNU Public License v2
 *					 or BSD license choice]
 *	"Dual MPL/GPL"			[GNU Public License v2
 *					 or Mozilla license choice]
 *
 * The following other idents are available
 *
 *	"Proprietary"			[Non free products]
 *
 * There are dual licensed components, but when running with Linux it is the
 * GPL that is relevant so this is a non issue. Similarly LGPL linked with GPL
 * is a GPL combined work.
 *
 * This exists for several reasons
 * 1.	So modinfo can show license info for users wanting to vet their setup 
 *	is free
 * 2.	So the community can ignore bug reports including proprietary modules
 * 3.	So vendors can do likewise based on their own policies
 */
#define MODULE_LICENSE(_license) MODULE_INFO(license, _license)

/* Author, ideally of form NAME <EMAIL>[, NAME <EMAIL>]*[ and NAME <EMAIL>] */
#define MODULE_AUTHOR(_author) MODULE_INFO(author, _author)
  
/* What your module does. */
#define MODULE_DESCRIPTION(_description) MODULE_INFO(description, _description)

/* One for each parameter, describing how to use it.  Some files do
   multiple of these per line, so can't just use MODULE_INFO. */
#define MODULE_PARM_DESC(_parm, desc) \
	__MODULE_INFO(parm, _parm, #_parm ":" desc)

#define MODULE_DEVICE_TABLE(type,name)		\
  MODULE_GENERIC_TABLE(type##_device,name)

/* Version of form [<epoch>:]<version>[-<extra-version>].
   Or for CVS/RCS ID version, everything but the number is stripped.
  <epoch>: A (small) unsigned integer which allows you to start versions
           anew. If not mentioned, it's zero.  eg. "2:1.0" is after
	   "1:2.0".
  <version>: The <version> may contain only alphanumerics and the
           character `.'.  Ordered by numeric sort for numeric parts,
	   ascii sort for ascii parts (as per RPM or DEB algorithm).
  <extraversion>: Like <version>, but inserted for local
           customizations, eg "rh3" or "rusty1".

  Using this automatically adds a checksum of the .c files and the
  local headers in "srcversion".
*/
#define MODULE_VERSION(_version) MODULE_INFO(version, _version)

/* Given an address, look for it in the exception tables */
const struct exception_table_entry *search_exception_tables(unsigned long add);

struct notifier_block;

#ifdef CONFIG_MODULES

/* Get/put a kernel symbol (calls must be symmetric) */
void *__symbol_get(const char *symbol);
void *__symbol_get_gpl(const char *symbol);
#define symbol_get(x) ((typeof(&x))(__symbol_get(MODULE_SYMBOL_PREFIX #x)))

#ifndef __GENKSYMS__
#ifdef CONFIG_MODVERSIONS
/* Mark the CRC weak since genksyms apparently decides not to
 * generate a checksums for some symbols */
#define __CRC_SYMBOL(sym, sec)					\
	extern void *__crc_##sym __attribute__((weak));		\
	static const unsigned long __kcrctab_##sym		\
	__attribute_used__					\
	__attribute__((section("__kcrctab" sec), unused))	\
	= (unsigned long) &__crc_##sym;
#else
#define __CRC_SYMBOL(sym, sec)
#endif

/* For every exported symbol, place a struct in the __ksymtab section */
/* 导出到内核符号表当中 */
#define __EXPORT_SYMBOL(sym, sec)				\
	__CRC_SYMBOL(sym, sec)					\
	static const char __kstrtab_##sym[]			\
	__attribute__((section("__ksymtab_strings")))		\
	= MODULE_SYMBOL_PREFIX #sym;                    	\
	static const struct kernel_symbol __ksymtab_##sym	\
	__attribute_used__					\
	__attribute__((section("__ksymtab" sec), unused))	\
	= { (unsigned long)&sym, __kstrtab_##sym }

#define EXPORT_SYMBOL(sym)					\
	__EXPORT_SYMBOL(sym, "")

#define EXPORT_SYMBOL_GPL(sym)					\
	__EXPORT_SYMBOL(sym, "_gpl")

#endif

/* 内核模块的引用计数 */
struct module_ref
{
	local_t count;			/* 模块的引用计数 */
} ____cacheline_aligned;

enum module_state
{
	MODULE_STATE_LIVE, /* 模块当前正在使用中 */
	MODULE_STATE_COMING, /* 模块当前正在被加载 */
	MODULE_STATE_GOING,   /* 模块正在被卸载 */
};

/* Similar stuff for section attributes. */
#define MODULE_SECT_NAME_LEN 32
struct module_sect_attr
{
	struct module_attribute mattr;
	char name[MODULE_SECT_NAME_LEN];
	unsigned long address;
};

/* 内核模块的段属性 */
struct module_sect_attrs
{
	struct attribute_group grp;
	struct module_sect_attr attrs[0];
};

struct module_param_attrs;

/* 内核模块结构 */
struct module
{
	enum module_state state;      /* 记录模块的状态 */

	/* Member of list of modules */
	struct list_head list;           /* 作为一个双向链表，链接所有的内核模块 */

	/* Unique handle for this module */
	char name[MODULE_NAME_LEN];        /* 内核模块的名称 */

	/* Sysfs stuff. */
	struct module_kobject mkobj;
	struct module_param_attrs *param_attrs;

	/* Exported symbols */
        /* 内核导出的符号以及符号的数量 */
	const struct kernel_symbol *syms;
	unsigned int num_syms;
        /* 也是一个具有num_syms个元素的数组，存储了导出符号的校验和，用于实现版本控制 */
	const unsigned long *crcs; 

	/* GPL-only exported symbols. */
	const struct kernel_symbol *gpl_syms;
	unsigned int num_gpl_syms;
	const unsigned long *gpl_crcs;

	/* Exception table */
        /* 异常表，num_exentries指定了数组的长度 */
	unsigned int num_exentries;
	const struct exception_table_entry *extable;

	/* Startup function. */   /* 模块的初始化函数，一般在内核模块开发的时候，需要实现 */
	int (*init)(void);

	/* If this is non-NULL, vfree after init() returns */
        /* 在初始化完成最后阶段会释放该指针指向的内存 */
	void *module_init;

	/* Here is the actual code + data, vfree'd on unload. */
	void *module_core;

	/* Here are the sizes of the init and core sections */
	unsigned long init_size, core_size;

	/* The size of the executable code in each section.  */
	unsigned long init_text_size, core_text_size;

	/* Arch-specific module values */
        /* 与特定的处理器架构相关 */
	struct mod_arch_specific arch;

	/* Am I unsafe to unload? */
	int unsafe;

	/* Am I GPL-compatible */
	int license_gplok;  /* 是否和gpl兼容的许可 */

#ifdef CONFIG_MODULE_UNLOAD
	/* Reference counts */
	struct module_ref ref[NR_CPUS];			/* 模块在不同CPU上的引用计数 */

	/* What modules depend on me? */
        /* 内核中那些模块依赖该模块，注意这里并不是通过每个struct module结构的
          * 该变量来形成双向链表，而是通过struct module_use，注意这里也用到了容器技术
          */
	struct list_head modules_which_use_me;

	/* Who is waiting for us to be unloaded */
        /* 等待使用模块的进程队列 ，指向导致模块卸载并且正在等待该操作结束的进程 */
	struct task_struct *waiter;				

	/* Destruction function. */           /* 模块的退出函数 */
	void (*exit)(void);
#endif

#ifdef CONFIG_KALLSYMS
	/* We keep the symbol and string tables for kallsyms. */
        /*  kallsyms的符号表和字符串标，用于记录该模块的所有符号信息 */
	Elf_Sym *symtab;
	unsigned long num_symtab;
	char *strtab;

	/* Section attributes */
        /* 模块中各段的属性 */
	struct module_sect_attrs *sect_attrs;
#endif

	/* Per-cpu data. */
	void *percpu;

	/* The command line arguments (may be mangled).  People like
	   keeping pointers to this stuff */
	char *args;
};

/* FIXME: It'd be nice to isolate modules during init, too, so they
   aren't used before they (may) fail.  But presently too much code
   (IDE & SCSI) require entry into the module during init.*/
/* 判断模块是否被卸载 */
static inline int module_is_live(struct module *mod)
{
	return mod->state != MODULE_STATE_GOING;
}

/* Is this address in a module? (second is with no locks, for oops) */
struct module *module_text_address(unsigned long addr);
struct module *__module_text_address(unsigned long addr);

/* Returns module and fills in value, defined and namebuf, or NULL if
   symnum out of range. */
struct module *module_get_kallsym(unsigned int symnum,
				  unsigned long *value,
				  char *type,
				  char namebuf[128]);

/* Look for this name: can be of form module:name. */
unsigned long module_kallsyms_lookup_name(const char *name);

int is_exported(const char *name, const struct module *mod);

extern void __module_put_and_exit(struct module *mod, long code)
	__attribute__((noreturn));
#define module_put_and_exit(code) __module_put_and_exit(THIS_MODULE, code);

#ifdef CONFIG_MODULE_UNLOAD
unsigned int module_refcount(struct module *mod);
void __symbol_put(const char *symbol);
#define symbol_put(x) __symbol_put(MODULE_SYMBOL_PREFIX #x)
void symbol_put_addr(void *addr);

/* Sometimes we know we already have a refcount, and it's easier not
   to handle the error case (which only happens with rmmod --wait). */
static inline void __module_get(struct module *module)
{
	if (module) {
		BUG_ON(module_refcount(module) == 0);
		local_inc(&module->ref[get_cpu()].count);
		put_cpu();
	}
}

/* 判断模块是否活动，并增加模块的引用计数，
  * 正确返回1 ，否则返回0 
  */
static inline int try_module_get(struct module *module)
{
	int ret = 1;

	if (module) {
		unsigned int cpu = get_cpu();
		if (likely(module_is_live(module)))
			local_inc(&module->ref[cpu].count);
		else
			ret = 0;
		put_cpu();
	}
	return ret;
}

/* 减少模块的引用计数 */
static inline void module_put(struct module *module)
{
	if (module) {
		unsigned int cpu = get_cpu();
		local_dec(&module->ref[cpu].count);
		/* Maybe they're waiting for us to drop reference? */
		if (unlikely(!module_is_live(module)))
			wake_up_process(module->waiter);
		put_cpu();
	}
}

#else /*!CONFIG_MODULE_UNLOAD*/
static inline int try_module_get(struct module *module)
{
	return !module || module_is_live(module);
}
static inline void module_put(struct module *module)
{
}
static inline void __module_get(struct module *module)
{
}
#define symbol_put(x) do { } while(0)
#define symbol_put_addr(p) do { } while(0)

#endif /* CONFIG_MODULE_UNLOAD */

/* This is a #define so the string doesn't get put in every .o file */
#define module_name(mod)			\
({						\
	struct module *__mod = (mod);		\
	__mod ? __mod->name : "kernel";		\
})

#define __unsafe(mod)							     \
do {									     \
	if (mod && !(mod)->unsafe) {					     \
		printk(KERN_WARNING					     \
		       "Module %s cannot be unloaded due to unsafe usage in" \
		       " %s:%u\n", (mod)->name, __FILE__, __LINE__);	     \
		(mod)->unsafe = 1;					     \
	}								     \
} while(0)

/* For kallsyms to ask for address resolution.  NULL means not found. */
const char *module_address_lookup(unsigned long addr,
				  unsigned long *symbolsize,
				  unsigned long *offset,
				  char **modname);

/* For extable.c to search modules' exception tables. */
const struct exception_table_entry *search_module_extables(unsigned long addr);

int register_module_notifier(struct notifier_block * nb);
int unregister_module_notifier(struct notifier_block * nb);

extern void print_modules(void);

struct device_driver;
void module_add_driver(struct module *, struct device_driver *);
void module_remove_driver(struct device_driver *);

#else /* !CONFIG_MODULES... */
#define EXPORT_SYMBOL(sym)
#define EXPORT_SYMBOL_GPL(sym)

/* Given an address, look for it in the exception tables. */
static inline const struct exception_table_entry *
search_module_extables(unsigned long addr)
{
	return NULL;
}

/* Is this address in a module? */
static inline struct module *module_text_address(unsigned long addr)
{
	return NULL;
}

/* Is this address in a module? (don't take a lock, we're oopsing) */
static inline struct module *__module_text_address(unsigned long addr)
{
	return NULL;
}

/* Get/put a kernel symbol (calls should be symmetric) */
#define symbol_get(x) ({ extern typeof(x) x __attribute__((weak)); &(x); })
#define symbol_put(x) do { } while(0)
#define symbol_put_addr(x) do { } while(0)

static inline void __module_get(struct module *module)
{
}

static inline int try_module_get(struct module *module)
{
	return 1;
}

static inline void module_put(struct module *module)
{
}

#define module_name(mod) "kernel"

#define __unsafe(mod)

/* For kallsyms to ask for address resolution.  NULL means not found. */
static inline const char *module_address_lookup(unsigned long addr,
						unsigned long *symbolsize,
						unsigned long *offset,
						char **modname)
{
	return NULL;
}

static inline struct module *module_get_kallsym(unsigned int symnum,
						unsigned long *value,
						char *type,
						char namebuf[128])
{
	return NULL;
}

static inline unsigned long module_kallsyms_lookup_name(const char *name)
{
	return 0;
}

static inline int is_exported(const char *name, const struct module *mod)
{
	return 0;
}

static inline int register_module_notifier(struct notifier_block * nb)
{
	/* no events will happen anyway, so this can always succeed */
	return 0;
}

static inline int unregister_module_notifier(struct notifier_block * nb)
{
	return 0;
}

#define module_put_and_exit(code) do_exit(code)

static inline void print_modules(void)
{
}

struct device_driver;
struct module;

static inline void module_add_driver(struct module *module, struct device_driver *driver)
{
}

static inline void module_remove_driver(struct device_driver *driver)
{
}

#endif /* CONFIG_MODULES */

#define symbol_request(x) try_then_request_module(symbol_get(x), "symbol:" #x)

/* BELOW HERE ALL THESE ARE OBSOLETE AND WILL VANISH */

struct obsolete_modparm {
	char name[64];
	char type[64-sizeof(void *)];
	void *addr;
};

static inline void MODULE_PARM_(void) { }
#ifdef MODULE
/* DEPRECATED: Do not use. */
#define MODULE_PARM(var,type)						    \
struct obsolete_modparm __parm_##var __attribute__((section("__obsparm"))) = \
{ __stringify(var), type, &MODULE_PARM_ };
#else
#define MODULE_PARM(var,type) static void __attribute__((__unused__)) *__parm_##var = &MODULE_PARM_;
#endif

#define __MODULE_STRING(x) __stringify(x)

/* Use symbol_get and symbol_put instead.  You'll thank me. */
#define HAVE_INTER_MODULE
extern void __deprecated inter_module_register(const char *,
		struct module *, const void *);
extern void __deprecated inter_module_unregister(const char *);
extern const void * __deprecated inter_module_get(const char *);
extern const void * __deprecated inter_module_get_request(const char *,
		const char *);
extern void __deprecated inter_module_put(const char *);

#endif /* _LINUX_MODULE_H */
