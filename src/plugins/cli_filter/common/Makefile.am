# Makefile.am for cli_filter/common

AUTOMAKE_OPTIONS = foreign
CLEANFILES = core.* cli_filter_common_gen.c

AM_CPPFLAGS = -I$(top_srcdir)
BUILT_SOURCES = cli_filter_common_gen.c

CLI_OPT_HEADERFILES = $(top_srcdir)/src/common/slurm_opt.h \
		$(top_srcdir)/src/bcast/file_bcast.h

CLI_OPT_STRUCTS = sbatch_opt salloc_opt srun_opt slurm_options bcast_parameters

# making a .la

noinst_LTLIBRARIES = libcli_filter_common.la
libcli_filter_common_la_SOURCES =	\
	cli_filter_common.c		\
	cli_filter_common_gen.c

cli_filter_common_gen.c: Makefile
	/bin/sh -c "CPPFLAGS=\"-I. -I$(srcdir) -I$(top_srcdir) -I$(top_builddir) -I$(top_builddir)/slurm\" python $(srcdir)/gen_cli_filter_common.py $(CLI_OPT_HEADERFILES) $(CLI_OPT_STRUCTS) --string > cli_filter_common_gen.c"
