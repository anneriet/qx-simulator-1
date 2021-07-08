/**
 * @file   qxelarator.i
 * @author Imran Ashraf
 * @brief  swig interface file
 */
%define DOCSTRING
"`qxelarator` Python interface to QX simulator as an accelerator."
%enddef

%module(docstring=DOCSTRING) qxelarator

%include "std_string.i"
%include "std_vector.i"
%include "std_complex.i"

%{
#include "qx/qxelarator.h"
%}

%template(vectord) std::vector<double>;
%template(vectorcd) std::vector<std::complex<double>>;

// Include the header file with above prototypes
%include "qx/qxelarator.h"
