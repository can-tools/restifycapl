---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: CAPL Functions
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/ProgrammingInterfaces/CAPL/General/Functions.htm)

[CAPL Introduction](../CAPLIntroduction.htm) Â» CAPL Functions

# CAPL Functions

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE â¢ CANoe MedTech DE â¢ CANoe4SW:lite DE

CAPL functions are used among other things to modularize code for repeated use, and to form interfaces. The syntax is C-like, but there are a number of supplemental features not included in C:

* A missing result type is interpreted as void
* An empty parameter list is permitted as in C++
* Overloading of functions (i.e. multiple functions with the same name but different parameter lists) is possible as in C++
* A [parameter check](FunctionsParameterCheck.htm) is performed as in C++
* Arrays of arbitrary dimension and size may be passed

CAPL provides you with a library of predefined (intrinsic) functions. For the selection of a [predefined function](../../../CAPLFunctions/CAPLfunctions.htm), the CAPL Browser makes available the [CAPL Function Explorer](../../../Shared/FunctionExplorer.htm).

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Example<br>The following function outputs a matrix to the Write Window:<br>void printMatrix(int m[][])   {    int i,j;    for(i=0;i<elCount(m);i++) {    for(j=0;j<elCount(m[0]);j++){    write("%ld", m[i][j]);    }    }   }<br>You can overload the function write() in the following manner:<br>write(long), write(float),   write(int matrix[][]), write(message\*) |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

[Glossary](../../../Shared/Glossary.htm) â¢ [Event Procedures](../../../CAPLFunctions/Other/CAPLfunctionsEventProceduresOverview.htm) â¢ [Keywords](Keywords.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)