# Update the web site based on contents of the source repo

# The directory containing a built gpscorrelate. At least 'make docs' must have
# been run here.
BUILDDIR=../gpscorrelate

.PHONY: all update check

all: update check

update:
	cp -fv "$(BUILDDIR)"/doc/*{html,png} "$(BUILDDIR)"/README.md "$(BUILDDIR)"/gpscorrelate-gui.svg .
	cp -rfv "$(BUILDDIR)"/doc/fr/ .
	# Convert the file from ISO-8859-1 to ASCII. Github only supports UTF-8 for
	# HTML since it adds a charset=utf-8 header into every response.
	tidy -m -q -ascii --doctype loose gpscorrelate.html || test $$? -eq 1

check: *.html fr/*.html
	tidy -q -e -utf8 $^ || test $$? -eq 1
