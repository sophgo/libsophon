This is the document repo of BMCV.

## install latex：

```
sudo apt install texlive-xetex texlive-latex-recommended
```



## install sphinx：

```
sudo apt install python-is-python3
pip install sphinx sphinx-autobuild sphinx_rtd_theme rst2pdf
```



## Install the stuttering Chinese word splitting library to support Chinese search:

```
pip install jieba3k
```



## Get Fandol font for linux system

```
wget http://mirrors.ctan.org/fonts/fandol.zip
unzip fandol.zip
sudo cp -r fandol /usr/share/fonts/
cp -r fandol ~/.fonts
```





## Compilation

```shell
## if you want Chinese html.Under directory “/xxx/bmcv”execute:
make web # to build document to static html files
         # or make html
# then directly open /xxx/bmcv/build/html/index.html

## if you want Chinese html.Under directory “/xxx/bmcv”execute:
make web LANG=en # to build document to static html files
         # or make html
# then directly open /xxx/bmcv/build/html/index.html



## if you want Chinese pdf.Under directory “/xxx/bmcv”execute:
make pdf
# the BMCV_User_Guide_zh.pdf is in build/

## if you want English pdf.
make pdf LANG=en
# the BMCV_User_Guide_en.pdf is in build/



## Testing

TODO



## Deployment

TODO



## Built With

* [Sphinx](http://www.sphinx-doc.org) - Document auto generate tool
* [Latex](https://www.latex-project.org/) - High-quality typesetting system



## License

This project is licensed under the MIT License - see the LICENSE file for details



## Acknowledgments
