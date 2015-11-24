lib_LTLIBRARIES = libgsthyperspectrallib.la

CLEANFILES = $(BUILT_SOURCES)

libgsthyperspectrallib_la_SOURCES = \
    hyperspectral-info.c \
    hyperspectral-mosaic.c \
    hyperspectral-frame.c \
    hyperspectral-format.c

libgsthyperspectrallibincludedir = $(includedir)/gstreamer/gst/histogram

libgsthyperspectrallibinclude_HEADERS = \
    hyperspectral.h \
    hyperspectral-info.h \
    hyperspectral-mosaic.h \
    hyperspectral-frame.h \
    hyperspectral-format.h



libgsthyperspectrallib_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)

libgsthyperspectrallib_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS) $(LIBM) -lgstvideo-$(GST_API_VERSION)
libgsthyperspectrallib_la_LDFLAGS = $(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS)



-include $(top_srcdir)/git.mk
