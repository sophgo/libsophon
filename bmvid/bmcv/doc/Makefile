# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SOURCEDIR     = source
BUILDDIR      = build

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

pdf: latexpdf
	@cd $(BUILDDIR)/latex && xelatex BMCV_User_Guide.tex
	@cd $(BUILDDIR)/latex && xelatex BMCV_User_Guide.tex
	@mv $(BUILDDIR)/latex/*.pdf $(BUILDDIR) && rm -rf $(BUILDDIR)/latex

web: html
	#@python3 -m http.server --directory build/html

clean:
	@rm -rf $(BUILDDIR)
