plugin_LTLIBRARIES = libgsthyperspectral.la

libgsthyperspectral_la_SOURCES = gsthyperspectral.c \
	gsthyperspectralenc.c \
	gsthyperspectraldec.c \
	gsthspecfilesink.c \
	gsthspecreducer.c

libgsthyperspectral_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgsthyperspectral_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthyperspectral_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) \
	$(top_builddir)/gst-libs/gst/hyperspectral/libgsthyperspectrallib.la \
	-lgsthyperspectrallib -lgstvideo-$(GST_API_VERSION)

libgsthyperspectral_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gsthyperspectralenc.h \
	gsthyperspectraldec.h \
	gsthspecfilesink.h \
	gsthspecreducer.h

-include $(top_srcdir)/git.mk
