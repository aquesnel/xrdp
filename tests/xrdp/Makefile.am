AM_CPPFLAGS = \
  -I$(top_srcdir)/common \
  $(FREERDP_CFLAGS)

if XRDP_DEBUG
AM_CPPFLAGS += -DXRDP_DEBUG
endif

sbin_PROGRAMS = \
  test-xrdp-client

test_xrdp_client_SOURCES = \
  test_xrdp_connect.c

test_xrdp_client_LDADD = \
  $(top_builddir)/common/libcommon.la \
  $(FREERDP_LIBS)
