# TestCpp-xml-schematron

[![C++ CI](https://github.com/jmarrec/TestCpp-xml-schematron/actions/workflows/build.yml/badge.svg)](https://github.com/jmarrec/TestCpp-xml-schematron/actions/workflows/build.yml)
[![codecov](https://codecov.io/gh/jmarrec/TestCpp-xml-schematron/branch/main/graph/badge.svg?token=CZCY313ERT)](https://codecov.io/gh/jmarrec/TestCpp-xml-schematron)
[![clang-format](https://github.com/jmarrec/TestCpp-xml-schematron/actions/workflows/clang-format-check.yml/badge.svg)](https://github.com/jmarrec/TestCpp-xml-schematron/actions/workflows/clang-format-check.yml)
[![cppcheck](https://github.com/jmarrec/TestCpp-xml-schematron/actions/workflows/cppcheck.yml/badge.svg)](https://github.com/jmarrec/TestCpp-xml-schematron/actions/workflows/cppcheck.yml)

A repo to test how to validate a document using a Schematron XML using `libxml2` and `libxslt`
report coverage of a C++ project using Github Actions, using https://codecov.io.

See:
* the Github Actions workflow at [.github/workflows/build.yml](.github/workflows/build.yml)

----

### Notes:

* `libxml2` includes an old version of schematron. So the idea is to convert the schematron xml to an XSLT stylesheet, and use `libxslt` to apply that stylesheet and get validation errors.
    * this is what the python `lxml` module ends up doing.

### TODO:

* Currently I manually converted the schematron xml to an xslt stylesheet using `lxml`.
* Clean up the API
* Investigate potentially memory leaks (I didn't pay too much attention at freeing stuff yet)
