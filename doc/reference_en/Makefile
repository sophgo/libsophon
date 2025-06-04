# You can set these variables from the command line.

# module_name === project in conf.py, use \_ instead of _ if module name contains _
module_name := BMLIB-REFERENCE

LANG := zh

ifeq ($(LANG), zh)
	SOURCEDIR     = .
	pdf_name      = BMLib_Technical_Reference_Manual.pdf
else
	echo unsupported language!
	exit 1
endif

#echo $SOURCEDIR
#exit 1

SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
BUILDDIR      = build

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

pdf: latex
	@cd $(BUILDDIR)/latex && xelatex $(module_name).tex
	@cd $(BUILDDIR)/latex && xelatex $(module_name).tex
	@mv $(BUILDDIR)/latex/$(module_name).pdf $(BUILDDIR)/"${pdf_name}" && rm -rf $(BUILDDIR)/latex

web: html
	#@python3 -m http.server --directory build/html

clean:
	@rm -rf $(BUILDDIR)
