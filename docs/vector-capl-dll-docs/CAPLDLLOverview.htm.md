---
meta-msapplication-config: ../../../../../Skins/Favicons/browserconfig.xml
meta-viewport: width=device-width, initial-scale=1.0
title: CAPL DLL
---

[Open topic with navigation](../../../../../CANoe.htm#Topics/SampConf/Programming/CAPLDLL/CAPLDLLOverview.htm)

[Sample Configurations](../../CANoeCANalyzerSampleConfigurations.htm) Â» [Programming](../SampConfsProgramming.htm) Â» CAPL DLL

# CAPL DLL

 

[Valid for](../../../Shared/FeatureAvailability.htm "Feature availabilty for your product"): CANoe DE

| ![Example](../../../../Resources/vImages/vExample.png "Example") | Click on the blue link to load the sample configuration in your CANoe.<br>CAPL DLL â [CANoeCAPLdll.cfg](javascript:startCANoeLauncher('"SAMPLES:\\Programming\\Capldll\\EXAMPLE\\CANoeCAPLdll.cfg"'))   Linking CAPL-DLLs in CANoe. |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |

This example will be used to illustrate the principle of CAPL-DLL with your CANoe. This example provides a good introduction to how CAPL-DLLs works.

When linking your own DLLs in CAPL programs, all methods are available to you like accessing system resources (Memory management, hardware, etc.). However, only experienced programmers should make use of this option due to the multitude of associated problems. We ask for your understanding that Vector is not able to provide any support for creating DLLs.

## Description

This sample configuration contains a CAPL node, which demonstrates the CAPL-DLL functions.

Since a CAPL DLL can be used simultaneously by multiple CAPL nodes, these nodes must be clearly differentiated in the CAPL DLL. For this reason each CAPL node gets a unique Handle after the measurement start.

After Handles have been allocated, first all Callback functions in the CAPL program are called by the DLL. Afterwards the CAPL program calls other functions in the DLL. The return values are displayed in the Write window.

Keyboard keys <1> to <8> can be used to call supplemental functions during the measurement. Here too the values returned by the DLL are output in the Write window.

## Further Information

The Source directory contains the source code for the CAPL DLL: [Open Folder](javascript:startCANoeLauncher('"SAMPLES:\\Programming\\CAPLdll\\Sources"'))

The project directories contain the project files of different Visual Studio versions. These allow you to compile the source code directly by the Visual C++ compiler for the Windows platform (32 bit and 64 bit). For the use of another compiler (e.g. GCC or Clang for Linux or the GCC variant mingw-64 for Windows) a project file for CMake is included. Project directories: [Open Folder](javascript:startCANoeLauncher('"SAMPLES:\\Programming\\Capldll"'))

The centerpiece of this CAPL DLL is its [Export table](../../../ProgrammingInterfaces/CAPL/CAPLDLL/CAPLExportTable.htm) (CAPL_DLL_INFO_LIST). It exports user-created functions to the CAPL code.

The CAPL DLL interface also supports callback functions effective with your CANoe Version 5.0.

The CAPL DLL integration is done using the command #pragma library("file path") in CAPL directly. Starting with your CANoe version 13.0 for the file path a [module description file](../../../DescriptionFiles/VModule/VModule.htm) is used. For older CANoe versions the path of the Windows DLLs is directly specified.

[CAPL DLL Introduction](../../../ProgrammingInterfaces/CAPL/CAPLDLL/CAPLIncludeWindowsDLL.htm) â¢ [Description of the CAPL Export Table](../../../ProgrammingInterfaces/CAPL/CAPLDLL/CAPLExportTable.htm)

- [../../../Shared/HowToUseOnlineHelp.htm](../../../Shared/HowToUseOnlineHelp.htm "Tips for using the help") [](<> "Opens German help") [](<> "Opens English help") [](<> "Opens Japanese help")[../../../Shared/HowToUseOnlineHelp.htm#BMLanguage](../../../Shared/HowToUseOnlineHelp.htm#BMLanguage) [Vector Help Dashboard](https://help.vector.com/ "Opens the Vector Help Dashboard") â¢ [Version Selection](https://help.vector.com/CANoeDEFamily/index.html "Opens the version overview of your product") 2025-10-11T20:31:10

- Â© Vector Informatik GmbH CANoe (Desktop Editions & Test Bench Editions) Version 19.3.1 [Contact/Copyright/License](../../../Shared/ContactCopyrightLicense.htm) Cookie Settings [Data Privacy Notice](https://www.vector.com/int/en/company/get-info/privacy-policy/)